/* File: ui-inventory.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral unified inventory+equipment browser kept separate from the
 * original command flow in cmd3.c.
 */

#include "angband.h"

#include "ui-inventory.h"
#include "ui-model.h"

#define UI_INVENTORY_TEXT_MAX 8192
#define UI_INVENTORY_LABEL_MAX 96
#define UI_INVENTORY_ACTION_MAX 16

typedef struct ui_inventory_item_entry ui_inventory_item_entry;
typedef struct ui_inventory_action_entry ui_inventory_action_entry;
typedef struct ui_inventory_section_layout ui_inventory_section_layout;

static bool (*ui_inventory_can_wear_hook)(const object_type*) = NULL;

struct ui_inventory_item_entry
{
    int item;
    int section;
    int section_row;
    byte attr;
    int visual_kind;
    int visual_attr;
    int visual_char;
    char label[UI_INVENTORY_LABEL_MAX];
};

struct ui_inventory_action_entry
{
    int key;
    cptr label;
    cptr details;
};

struct ui_inventory_section_layout
{
    int section;
    int item_start;
    int item_end;
    int term_col;
    int menu_col;
    cptr heading;
    cptr empty_text;
};

static const ui_inventory_action_entry ui_inventory_action_defs[] = {
    { 'u', "Use", "Use the item's primary action." },
    { 'w', "Wield or wear", "Equip this item." },
    { 'r', "Remove", "Take this equipped item off." },
    { 'd', "Drop", "Drop some or all of this item." },
    { 'k', "Destroy", "Destroy some or all of this item." },
    { 'x', "Examine", "Show the known properties of this item." },
    { '{', "Inscribe", "Add or edit an inscription." },
    { 'a', "Activate staff", "Use one charge from this staff." },
    { 'p', "Play instrument", "Play this instrument." },
    { 'q', "Quaff", "Drink this potion." },
    { 'E', "Eat", "Eat this item." },
    { 't', "Throw", "Throw this item." },
    { 'f', "Fire 1st quiver", "Fire the arrows from your first quiver." },
    { 'F', "Fire 2nd quiver", "Fire the arrows from your second quiver." },
};

static const ui_inventory_section_layout ui_inventory_sections[] = {
    { UI_INVENTORY_SECTION_EQUIP, INVEN_WIELD, INVEN_TOTAL, 2, 0,
        "Equipped", "(nothing equipped)" },
    { UI_INVENTORY_SECTION_INVEN, 0, INVEN_PACK, 41, 1,
        "Inventory", "(nothing in your pack)" },
};

/* Returns one inventory slot pointer or NULL for invalid indices. */
static object_type* ui_inventory_object(int item)
{
    if ((item < 0) || (item >= INVEN_TOTAL))
        return NULL;

    return &inventory[item];
}

/* Returns the opposite inventory section for left/right navigation. */
static int ui_inventory_other_section(int section)
{
    return (section == UI_INVENTORY_SECTION_EQUIP)
        ? UI_INVENTORY_SECTION_INVEN
        : UI_INVENTORY_SECTION_EQUIP;
}

/* Chooses the row color used for one item in the inventory lists. */
static byte ui_inventory_item_attr(object_type* o_ptr)
{
    if (!o_ptr)
        return TERM_WHITE;

    if (weapon_glows(o_ptr))
        return TERM_L_BLUE;

    return tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];
}

/* Exports the visual that should be shown beside an inventory entry. */
static void ui_inventory_item_visual(object_type* o_ptr, int* kind,
    int* attr, int* chr)
{
    if (!kind || !attr || !chr || !o_ptr)
        return;

    *kind = graphics_are_ascii() ? UI_MENU_VISUAL_TEXT : UI_MENU_VISUAL_TILE;
    *attr = (byte)object_attr(o_ptr);
    *chr = (byte)object_char(o_ptr);
}

/* Checks whether the generic "use item" action is legal for this object. */
static bool ui_inventory_item_can_use(int item, const object_type* o_ptr)
{
    object_type* l_ptr;

    if (!o_ptr || !o_ptr->k_idx)
        return FALSE;

    switch (o_ptr->tval)
    {
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    case TV_ARROW:
    case TV_FLASK:
    case TV_NOTE:
    case TV_METAL:
    case TV_CHEST:
    case TV_STAFF:
    case TV_HORN:
    case TV_POTION:
    case TV_FOOD:
        break;

    default:
        return FALSE;
    }

    if ((item >= INVEN_WIELD) && (o_ptr->tval == TV_FLASK))
        return FALSE;

    if ((item < INVEN_WIELD) && (o_ptr->tval == TV_FLASK))
    {
        l_ptr = &inventory[INVEN_LITE];
        return (l_ptr->sval == SV_LIGHT_LANTERN);
    }

    return TRUE;
}

/* Returns whether one action hotkey is currently available for the item. */
static bool ui_inventory_action_available(
    int key, int item, const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return FALSE;

    switch (key)
    {
    case 'u':
        return ui_inventory_item_can_use(item, o_ptr);

    case 'w':
        return (item < INVEN_WIELD) && ui_inventory_can_wear_hook
            && ui_inventory_can_wear_hook(o_ptr);

    case 'r':
        return (item >= INVEN_WIELD);

    case 'd':
        return TRUE;

    case 'k':
        return (item < INVEN_WIELD);

    case 'x':
    case '{':
    case 't':
        return TRUE;

    case 'a':
        return (item < INVEN_WIELD) && (o_ptr->tval == TV_STAFF);

    case 'p':
        return (item < INVEN_WIELD) && (o_ptr->tval == TV_HORN);

    case 'q':
        return (item < INVEN_WIELD) && (o_ptr->tval == TV_POTION);

    case 'E':
        return (item < INVEN_WIELD) && (o_ptr->tval == TV_FOOD);

    case 'f':
        return (item == INVEN_QUIVER1) && inventory[INVEN_BOW].k_idx
            && p_ptr->ammo_tval;

    case 'F':
        return (item == INVEN_QUIVER2) && inventory[INVEN_BOW].k_idx
            && p_ptr->ammo_tval;

    default:
        return FALSE;
    }
}

/* Builds the action list shown after selecting one inventory item. */
static int ui_inventory_build_actions(ui_inventory_action_entry* entries,
    int max_entries, int item, const object_type* o_ptr)
{
    int i;
    int count = 0;

    if (!entries || (max_entries <= 0))
        return 0;

    for (i = 0; i < (int)N_ELEMENTS(ui_inventory_action_defs); i++)
    {
        if (!ui_inventory_action_available(
                ui_inventory_action_defs[i].key, item, o_ptr))
        {
            continue;
        }
        if (count >= max_entries)
            break;

        entries[count++] = ui_inventory_action_defs[i];
    }

    return count;
}

/* Formats the user-facing label for one action menu entry. */
static void ui_inventory_format_action_label(
    char* buf, size_t buf_size, const ui_inventory_action_entry* action)
{
    if (!buf || (buf_size == 0))
        return;

    if (!action)
    {
        buf[0] = '\0';
        return;
    }

    strnfmt(buf, buf_size, "%c) %s", action->key, action->label);
}

/* Builds the synthetic key path that moves from one item row to another. */
static void ui_inventory_build_nav(
    char* nav, size_t nav_size, int from_section, int from_row, int to_section,
    int to_row)
{
    size_t off = 0;

    if (!nav || (nav_size == 0))
        return;

    nav[0] = '\0';

    if ((from_section < 0) || (from_row < 0) || (to_section < 0)
        || (to_row < 0))
    {
        return;
    }

#define APPEND_INVENTORY_NAV(ch)                                              \
    do                                                                        \
    {                                                                         \
        if (off + 1 < nav_size)                                               \
            nav[off++] = (ch);                                                \
    } while (0)

    if (from_section < to_section)
        APPEND_INVENTORY_NAV('6');
    else if (from_section > to_section)
        APPEND_INVENTORY_NAV('4');

    while (from_row < to_row)
    {
        APPEND_INVENTORY_NAV('2');
        from_row++;
    }
    while (from_row > to_row)
    {
        APPEND_INVENTORY_NAV('8');
        from_row--;
    }

#undef APPEND_INVENTORY_NAV

    nav[off] = '\0';
}

/* Counts how many rows are currently present inside one section. */
static int ui_inventory_section_count(
    const ui_inventory_item_entry* entries, int item_count, int section)
{
    int i;
    int count = 0;

    for (i = 0; i < item_count; i++)
    {
        if (entries[i].section == section)
            count++;
    }

    return count;
}

/* Finds one item by section and row within that section. */
static int ui_inventory_find_by_section_row(
    const ui_inventory_item_entry* entries, int item_count, int section,
    int section_row)
{
    int i;

    for (i = 0; i < item_count; i++)
    {
        if ((entries[i].section == section)
            && (entries[i].section_row == section_row))
        {
            return i;
        }
    }

    return -1;
}

/* Finds one item by its inventory letter shortcut. */
static int ui_inventory_find_by_label(const ui_inventory_item_entry* entries,
    int item_count, char ch)
{
    int i;

    ch = (char)tolower((unsigned char)ch);

    for (i = 0; i < item_count; i++)
    {
        if (ch == index_to_label(entries[i].item))
            return i;
    }

    return -1;
}

/* Collects equipped and carried items into one flat semantic menu list. */
static int ui_inventory_collect_items(ui_inventory_item_entry* entries,
    int max_entries, int preferred_section)
{
    int count = 0;
    int section_rows[N_ELEMENTS(ui_inventory_sections)];
    size_t s;

    (void)preferred_section;

    for (s = 0; s < N_ELEMENTS(section_rows); s++)
        section_rows[s] = 0;

    for (s = 0; s < N_ELEMENTS(ui_inventory_sections); s++)
    {
        const ui_inventory_section_layout* layout = &ui_inventory_sections[s];
        int i;

        for (i = layout->item_start; i < layout->item_end; i++)
        {
            object_type* o_ptr = &inventory[i];
            char o_name[80];

            if (!o_ptr->k_idx)
                continue;
            if (count >= max_entries)
                break;

            object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);

            entries[count].item = i;
            entries[count].section = layout->section;
            entries[count].section_row = section_rows[s]++;
            entries[count].attr = ui_inventory_item_attr(o_ptr);
            ui_inventory_item_visual(o_ptr, &entries[count].visual_kind,
                &entries[count].visual_attr, &entries[count].visual_char);
            strnfmt(entries[count].label, sizeof(entries[count].label), "%c) %s",
                index_to_label(i), o_name);
            count++;
        }
    }

    return count;
}

/* Picks the first sensible selection when the menu opens. */
static int ui_inventory_default_selection(
    const ui_inventory_item_entry* entries, int count, int initial_section)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (entries[i].section == initial_section)
            return i;
    }

    return (count > 0) ? 0 : -1;
}

/* Keeps the selected index within the available item range. */
static int ui_inventory_clamp_selection(int selected, int item_count)
{
    if (selected >= item_count)
        selected = item_count - 1;
    if ((item_count > 0) && (selected < 0))
        selected = 0;

    return selected;
}

/* Moves the selection up or down inside the current section. */
static int ui_inventory_move_vertical(const ui_inventory_item_entry* entries,
    int item_count, int selected, int initial_section, int delta)
{
    int section;
    int row;
    int count;

    if (item_count <= 0)
        return selected;

    section = (selected >= 0) ? entries[selected].section : initial_section;
    row = (selected >= 0) ? entries[selected].section_row : 0;
    count = ui_inventory_section_count(entries, item_count, section);
    if (count <= 0)
        return selected;

    row = (row + delta + count) % count;
    return ui_inventory_find_by_section_row(entries, item_count, section, row);
}

/* Moves the selection left or right between equipped and inventory columns. */
static int ui_inventory_move_horizontal(const ui_inventory_item_entry* entries,
    int item_count, int selected, int initial_section, int preferred_section)
{
    int current_section;
    int target_section;
    int row;
    int count;

    if (item_count <= 0)
        return selected;

    current_section = (selected >= 0) ? entries[selected].section : initial_section;
    target_section = preferred_section;
    if (target_section == current_section)
        target_section = ui_inventory_other_section(current_section);

    count = ui_inventory_section_count(entries, item_count, target_section);
    if (count <= 0)
        return selected;

    row = (selected >= 0) ? entries[selected].section_row : 0;
    if (row >= count)
        row = count - 1;

    return ui_inventory_find_by_section_row(
        entries, item_count, target_section, row);
}

/* Appends one compact item-summary block above the full lore text. */
static void ui_inventory_append_item_summary(
    ui_text_builder* builder, const object_type* o_ptr)
{
    char buf[160];

    if (!builder || !o_ptr)
        return;

    if (o_ptr->weight > 0)
    {
        int wgt = o_ptr->weight * o_ptr->number;

        strnfmt(buf, sizeof(buf), "Weight: %d.%d lb", wgt / 10, wgt % 10);
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if ((o_ptr->dd > 0) && (o_ptr->ds > 0))
    {
        strnfmt(buf, sizeof(buf), "Combat: (%+d,%dd%d)", o_ptr->att, o_ptr->dd,
            o_ptr->ds);
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if ((o_ptr->evn != 0) || (o_ptr->pd > 0) || (o_ptr->ps > 0))
    {
        if ((o_ptr->pd > 0) && (o_ptr->ps > 0))
        {
            strnfmt(buf, sizeof(buf), "Defence: [%+d,%dd%d]", o_ptr->evn,
                o_ptr->pd, o_ptr->ps);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "Defence: [%+d]", o_ptr->evn);
        }
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if ((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
    {
        strnfmt(buf, sizeof(buf), "Charges: %d", o_ptr->pval);
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if ((o_ptr->tval == TV_LIGHT) && (o_ptr->timeout > 0))
    {
        strnfmt(buf, sizeof(buf), "%s: %d turns",
            fuelable_light_p(o_ptr) ? "Fuel" : "Light", o_ptr->timeout);
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if (o_ptr->obj_note)
    {
        strnfmt(buf, sizeof(buf), "Inscription: %s", quark_str(o_ptr->obj_note));
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    ui_text_builder_append_line(builder,
        object_known_p(o_ptr) ? "Knowledge: identified"
                              : "Knowledge: not fully identified",
        TERM_SLATE);
}

/* Builds the details pane for the currently highlighted inventory item. */
static void ui_inventory_append_selected_details(ui_text_builder* builder,
    int item, const object_type* o_ptr)
{
    char buf[128];

    if (!builder || !o_ptr)
        return;

    strnfmt(buf, sizeof(buf), "%s item",
        (item >= INVEN_WIELD) ? "Equipped" : "Inventory");
    ui_text_builder_append_line(builder, buf, TERM_WHITE);

    if (item >= INVEN_WIELD)
    {
        strnfmt(buf, sizeof(buf), "Slot: %s", mention_use(item));
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Pack slot: %c", index_to_label(item));
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    if (o_ptr->number > 1)
    {
        strnfmt(buf, sizeof(buf), "Quantity: %d", o_ptr->number);
        ui_text_builder_append_line(builder, buf, TERM_SLATE);
    }

    ui_text_builder_newline(builder, TERM_WHITE);
    ui_inventory_append_item_summary(builder, o_ptr);
    ui_text_builder_newline(builder, TERM_WHITE);
    object_info_append_ui_text(builder, o_ptr, TRUE);
}

/* Populates the details pane and preview visual for the current selection. */
static void ui_inventory_publish_selected_details(
    const ui_inventory_item_entry* entries, int item_count, int selected,
    ui_text_builder* details_builder, char* details_text, byte* details_attrs)
{
    object_type* o_ptr = NULL;

    if (!details_builder || !details_text || !details_attrs)
        return;

    if ((selected >= 0) && (selected < item_count))
    {
        o_ptr = ui_inventory_object(entries[selected].item);
        ui_inventory_append_selected_details(
            details_builder, entries[selected].item, o_ptr);
        ui_menu_set_details(
            details_text, details_attrs, ui_text_builder_length(details_builder));
        ui_menu_set_details_visual(entries[selected].visual_kind,
            entries[selected].visual_attr, entries[selected].visual_char);
        return;
    }

    ui_text_builder_append_line(
        details_builder, "You are not carrying or wearing anything.", TERM_SLATE);
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(details_builder));
}

/* Renders one inventory column, including its empty-state message. */
static void ui_inventory_render_section(const ui_inventory_item_entry* entries,
    int item_count, int selected, const ui_inventory_section_layout* layout)
{
    int count;
    int i;

    if (!entries || !layout)
        return;

    Term_putstr(layout->term_col, 4, -1, TERM_L_BLUE, layout->heading);

    count = ui_inventory_section_count(entries, item_count, layout->section);
    if (count <= 0)
    {
        Term_putstr(layout->term_col, 6, -1, TERM_SLATE, layout->empty_text);
        return;
    }

    for (i = 0; i < item_count; i++)
    {
        char nav[UI_MENU_NAV_MAX];
        bool is_selected;
        byte term_attr;
        int term_row;

        if (entries[i].section != layout->section)
            continue;

        term_row = 6 + entries[i].section_row;
        ui_inventory_build_nav(nav, sizeof(nav),
            entries[selected].section, entries[selected].section_row,
            entries[i].section, entries[i].section_row);
        is_selected = (i == selected);
        term_attr = is_selected ? TERM_L_BLUE : entries[i].attr;
        ui_menu_add_visual_with_nav(layout->menu_col, term_row,
            (int)strlen(entries[i].label), 1, '\r', is_selected,
            entries[i].attr, entries[i].visual_kind, entries[i].visual_attr,
            entries[i].visual_char, entries[i].label, nav);
        if (term_row < Term->hgt)
            Term_putstr(layout->term_col, term_row, -1, term_attr,
                entries[i].label);
    }
}

/* Publishes the main inventory browser view into the semantic menu model. */
static void ui_inventory_render_item_menu(
    const ui_inventory_item_entry* entries, int item_count, int selected)
{
    char menu_text[UI_INVENTORY_TEXT_MAX];
    byte menu_attrs[UI_INVENTORY_TEXT_MAX];
    char menu_details[UI_INVENTORY_TEXT_MAX];
    byte menu_details_attrs[UI_INVENTORY_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    size_t i;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, menu_details, menu_details_attrs, sizeof(menu_details));

    ui_text_builder_append_line(&menu_builder, "Inventory", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Use movement keys or pointer to select an item.", TERM_SLATE);
    ui_text_builder_append_line(&menu_builder,
        "Equipped items on the left; Inventory on the right.",
        TERM_SLATE);

    Term_putstr(2, 1, -1, TERM_WHITE, "Inventory");

    ui_menu_begin();
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details_width(30);

    for (i = 0; i < N_ELEMENTS(ui_inventory_sections); i++)
    {
        ui_inventory_render_section(
            entries, item_count, selected, &ui_inventory_sections[i]);
    }

    ui_menu_set_active_column(-1);
    ui_inventory_publish_selected_details(entries, item_count, selected,
        &details_builder, menu_details, menu_details_attrs);
    ui_menu_end();
}

/* Shows the action chooser for one already-selected inventory item. */
static int ui_inventory_action_menu(int item)
{
    char menu_text[UI_INVENTORY_TEXT_MAX];
    byte menu_attrs[UI_INVENTORY_TEXT_MAX];
    char menu_details[UI_INVENTORY_TEXT_MAX];
    byte menu_details_attrs[UI_INVENTORY_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    ui_inventory_action_entry actions[UI_INVENTORY_ACTION_MAX];
    object_type* o_ptr = ui_inventory_object(item);
    char item_name[80];
    int action_count;
    int highlight = 0;
    int visual_kind = UI_MENU_VISUAL_NONE;
    int visual_attr = TERM_WHITE;
    int visual_char = 0;
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    action_count = ui_inventory_build_actions(
        actions, N_ELEMENTS(actions), item, o_ptr);
    if (action_count <= 0)
        return 0;

    ui_inventory_item_visual(o_ptr, &visual_kind, &visual_attr, &visual_char);
    object_desc(item_name, sizeof(item_name), o_ptr, TRUE, 3);

    while (TRUE)
    {
        char ch;
        char action_label[96];

        if (highlight < 0)
            highlight = 0;
        if (highlight >= action_count)
            highlight = action_count - 1;

        ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
        ui_text_builder_init(
            &details_builder, menu_details, menu_details_attrs, sizeof(menu_details));

        ui_text_builder_append_line(&menu_builder, item_name, TERM_WHITE);
        ui_text_builder_newline(&menu_builder, TERM_WHITE);
        ui_text_builder_append_line(&menu_builder,
            "Choose an action with movement keys, Enter, or click.", TERM_SLATE);

        ui_inventory_format_action_label(
            action_label, sizeof(action_label), &actions[highlight]);
        ui_text_builder_append_line(&details_builder, action_label, TERM_L_WHITE);
        ui_text_builder_newline(&details_builder, TERM_WHITE);
        ui_text_builder_append_line(
            &details_builder, actions[highlight].details, TERM_SLATE);

        Term_putstr(2, 1, -1, TERM_WHITE, item_name);

        ui_menu_begin();
        ui_menu_set_text(
            menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
        ui_menu_set_details_width(36);
        ui_menu_set_details(menu_details, menu_details_attrs,
            ui_text_builder_length(&details_builder));
        ui_menu_set_details_visual(visual_kind, visual_attr, visual_char);

        for (i = 0; i < action_count; i++)
        {
            byte attr = (i == highlight) ? TERM_L_BLUE : TERM_WHITE;

            ui_inventory_format_action_label(
                action_label, sizeof(action_label), &actions[i]);
            ui_menu_add(2, 4 + i, (int)strlen(action_label), 1, actions[i].key,
                i == highlight, TERM_WHITE, action_label);
            if (4 + i < Term->hgt)
                Term_putstr(2, 4 + i, -1, attr, action_label);
        }

        ui_menu_end();
        Term_fresh();

        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        if (ch == ESCAPE)
        {
            ui_menu_clear();
            return 0;
        }

        if (ch == '8')
        {
            highlight = (highlight + action_count - 1) % action_count;
            continue;
        }

        if (ch == '2')
        {
            highlight = (highlight + 1) % action_count;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' '))
        {
            int key = actions[highlight].key;

            ui_menu_clear();
            return key;
        }

        for (i = 0; i < action_count; i++)
        {
            if (ch == actions[i].key)
            {
                ui_menu_clear();
                return actions[i].key;
            }
        }
    }
}

/* Restores the pre-inventory screen state before running a real command. */
static void ui_inventory_finish_menu(void)
{
    ui_menu_clear();
    ui_clear_preselected_item();
    ui_inventory_can_wear_hook = NULL;
    p_ptr->command_see = FALSE;
    p_ptr->command_wrk = 0;
    item_tester_full = FALSE;
    screen_load();
}

/* Dispatches one chosen action back into the normal command handlers. */
static bool ui_inventory_execute_action(int action_key, int item)
{
    object_type* o_ptr = ui_inventory_object(item);

    if (!o_ptr || !o_ptr->k_idx)
        return FALSE;

    ui_inventory_finish_menu();

    switch (action_key)
    {
    case 'u':
        ui_preselect_item(item);
        do_cmd_use_item();
        return TRUE;

    case 'w':
        do_cmd_wield(o_ptr, item);
        return TRUE;

    case 'r':
        do_cmd_takeoff(o_ptr, item);
        return TRUE;

    case 'd':
        ui_preselect_item(item);
        do_cmd_drop();
        return TRUE;

    case 'k':
        ui_preselect_item(item);
        do_cmd_destroy();
        return TRUE;

    case 'x':
        ui_preselect_item(item);
        do_cmd_observe();
        return TRUE;

    case '{':
        ui_preselect_item(item);
        do_cmd_inscribe();
        return TRUE;

    case 'a':
        do_cmd_activate_staff(o_ptr, item);
        return TRUE;

    case 'p':
        do_cmd_play_instrument(o_ptr, item);
        return TRUE;

    case 'q':
        do_cmd_quaff_potion(o_ptr, item);
        return TRUE;

    case 'E':
        do_cmd_eat_food(o_ptr, item);
        return TRUE;

    case 't':
        ui_preselect_item(item);
        do_cmd_throw(FALSE);
        return TRUE;

    case 'f':
        do_cmd_fire(1);
        return TRUE;

    case 'F':
        do_cmd_fire(2);
        return TRUE;

    default:
        return FALSE;
    }
}

/* Opens the action chooser for the current selection and runs the result. */
static bool ui_inventory_open_selected_item(
    const ui_inventory_item_entry* entries, int selected)
{
    int action_key;

    if (selected < 0)
        return FALSE;

    action_key = ui_inventory_action_menu(entries[selected].item);
    if (!action_key)
        return FALSE;

    return ui_inventory_execute_action(action_key, entries[selected].item);
}

/* Runs the unified inventory browser for inventory or equipment views. */
bool do_cmd_ui_inventory_menu(
    int initial_section, bool (*can_wear)(const object_type*))
{
    ui_inventory_item_entry entries[INVEN_TOTAL];
    int item_count;
    int selected;

    if (!can_wear)
        return FALSE;

    ui_inventory_can_wear_hook = can_wear;
    screen_save();

    item_count = ui_inventory_collect_items(
        entries, N_ELEMENTS(entries), initial_section);
    selected = ui_inventory_default_selection(entries, item_count, initial_section);

    while (TRUE)
    {
        ui_inventory_action_entry actions[UI_INVENTORY_ACTION_MAX];
        object_type* o_ptr = NULL;
        int action_count = 0;
        int label_index;
        char ch;
        int i;

        selected = ui_inventory_clamp_selection(selected, item_count);

        ui_inventory_render_item_menu(entries, item_count, selected);
        Term_fresh();

        if ((selected >= 0) && (selected < item_count))
        {
            o_ptr = ui_inventory_object(entries[selected].item);
            action_count = ui_inventory_build_actions(
                actions, N_ELEMENTS(actions), entries[selected].item, o_ptr);
        }

        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        if (ch == ESCAPE)
            break;

        if ((item_count > 0) && (ch == '8'))
        {
            selected = ui_inventory_move_vertical(
                entries, item_count, selected, initial_section, -1);
            continue;
        }

        if ((item_count > 0) && (ch == '2'))
        {
            selected = ui_inventory_move_vertical(
                entries, item_count, selected, initial_section, 1);
            continue;
        }

        if ((item_count > 0) && (ch == '4'))
        {
            selected = ui_inventory_move_horizontal(entries, item_count, selected,
                initial_section, UI_INVENTORY_SECTION_EQUIP);
            continue;
        }

        if ((item_count > 0) && (ch == '6'))
        {
            selected = ui_inventory_move_horizontal(entries, item_count, selected,
                initial_section, UI_INVENTORY_SECTION_INVEN);
            continue;
        }

        if ((item_count > 0)
            && ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '5')))
        {
            if (ui_inventory_open_selected_item(entries, selected))
                return TRUE;
            continue;
        }

        for (i = 0; i < action_count; i++)
        {
            if (ch == actions[i].key)
            {
                if (ui_inventory_execute_action(ch, entries[selected].item))
                    return TRUE;
                break;
            }
        }

        label_index = ui_inventory_find_by_label(entries, item_count, ch);
        if (label_index >= 0)
        {
            selected = label_index;
            if (ui_inventory_open_selected_item(entries, selected))
                return TRUE;
        }
    }

    ui_inventory_finish_menu();
    return TRUE;
}
