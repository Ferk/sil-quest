/* File: ui-knowledge.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Knowledge-screen presentation helpers and browser UI wrappers.
 */

#include "angband.h"

#include "ui-input.h"
#include "ui-knowledge.h"
#include "ui-model.h"
#include "ui-preview.h"

#define UI_KNOWLEDGE_TEXT_MAX 8192

static ui_text_builder* ui_knowledge_browser_builder = NULL;

static cptr ui_knowledge_menu_labels[] = { "(a) Display known artefacts",
    "(b) Display known monsters", "(c) Display known objects",
    "(d) Display names of the fallen", "(e) Display kill counts",
    "(f) Display character notes file" };

static cptr ui_knowledge_menu_details[] = {
    "Browse artefacts you have discovered and recall their properties.",
    "Browse known monsters and review their recall information.",
    "Browse known object groups and inspect individual items.",
    "Show the recorded names of the fallen.",
    "Display a sorted record of slain creatures.",
    "Open the character notes file if one exists." };

/* Appends browser recall text through the shared UI text builder hook. */
static void ui_knowledge_text_out_to_builder(byte attr, cptr str)
{
    if (!ui_knowledge_browser_builder || !str)
        return;

    ui_text_builder_append(ui_knowledge_browser_builder, str, attr);
}

/* Appends the standard modal footer used by knowledge recall popups. */
static void ui_knowledge_finish_modal(ui_text_builder* builder)
{
    if (!builder)
        return;

    if ((builder->off > 0) && (builder->text[builder->off - 1] != '\n'))
        ui_text_builder_newline(builder, TERM_WHITE);

    ui_text_builder_newline(builder, TERM_WHITE);
    ui_text_builder_append_line(builder, "(press any key)", TERM_L_BLUE);
}

/* Fills one object buffer with the fake artefact information for browsing. */
static bool ui_knowledge_artefact_prepare_info(
    int a_idx, object_type* o_ptr, char* name, size_t name_size)
{
    if (!o_ptr)
        return FALSE;

    if (!ui_preview_prepare_artefact_object(o_ptr, a_idx))
        return FALSE;

    if (name && (name_size > 0))
        object_desc(name, name_size, o_ptr, TRUE, 0);

    return TRUE;
}

/* Appends the details pane text for one artefact entry. */
static void ui_knowledge_artefact_append_details(
    ui_text_builder* builder, int a_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    artefact_type* a_ptr;
    char buf[128];

    if (!builder)
        return;

    if (!ui_knowledge_artefact_prepare_info(a_idx, i_ptr, NULL, 0))
        return;

    a_ptr = &a_info[a_idx];
    if (cheat_know)
    {
        strnfmt(buf, sizeof(buf), "Idx: %d   Depth: %d   Rarity: %d", a_idx,
            a_ptr->level, a_ptr->rarity);
        ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
        ui_text_builder_newline(builder, TERM_WHITE);
    }

    object_info_append_ui_text(builder, i_ptr, TRUE);
}

/* Builds the navigation hint sequence for one two-column browser item. */
static void ui_knowledge_build_browser_nav(char* buf, size_t buf_size,
    int from_column, int from_group, int from_list, int to_column, int to_group,
    int to_list)
{
    size_t off = 0;

    if (!buf || (buf_size == 0))
        return;

    buf[0] = '\0';

#define APPEND_BROWSER_NAV(ch)                                                \
    do                                                                        \
    {                                                                         \
        if (off + 1 < buf_size)                                               \
            buf[off++] = (ch);                                                \
    } while (0)

    if (to_column == 0)
    {
        if (from_column > 0)
            APPEND_BROWSER_NAV('4');

        while (from_group < to_group)
        {
            APPEND_BROWSER_NAV('2');
            from_group++;
        }
        while (from_group > to_group)
        {
            APPEND_BROWSER_NAV('8');
            from_group--;
        }
    }
    else
    {
        if (from_column == 0)
            APPEND_BROWSER_NAV('6');

        while (from_list < to_list)
        {
            APPEND_BROWSER_NAV('2');
            from_list++;
        }
        while (from_list > to_list)
        {
            APPEND_BROWSER_NAV('8');
            from_list--;
        }
    }

#undef APPEND_BROWSER_NAV

    buf[off] = '\0';
}

/* Formats the terminal summary line shown for the current monster group. */
static void ui_knowledge_monster_group_summary(
    char* buf, size_t buf_size, cptr group_char[], int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    if (!buf || (buf_size == 0))
        return;

    buf[0] = '\0';

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (group_char[grp_cur] != (char*)-1L)
        strnfmt(buf, buf_size, "Total Creatures Slain: %u.", slay_count);
    else
        strnfmt(buf, buf_size, "Known Uniques: %u, Slain Uniques: %u.",
            known_uniques, dead_uniques);
}

/* Appends the details pane text for one monster entry. */
static void ui_knowledge_monster_append_details(
    ui_text_builder* builder, int r_idx)
{
    char race_name[80];
    char buf[80];
    byte title_attr = TERM_WHITE;
    ui_text_output_state text_output_state;

    if (!builder || (r_idx <= 0))
        return;

    monster_desc_race(race_name, sizeof(race_name), r_idx);
    title_attr = r_info[r_idx].d_attr ? r_info[r_idx].d_attr : TERM_WHITE;
    ui_text_builder_append_line(builder, race_name, title_attr);
    if (cheat_know)
    {
        strnfmt(buf, sizeof(buf), "Idx: %d", r_idx);
        ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
    }
    ui_text_builder_newline(builder, TERM_WHITE);

    ui_knowledge_browser_builder = builder;
    ui_text_output_begin(
        &text_output_state, ui_knowledge_text_out_to_builder, 0, 0);

    describe_monster(r_idx, FALSE);

    ui_text_output_end(&text_output_state);
    ui_knowledge_browser_builder = NULL;
}

/* Publishes the appropriate glyph preview for one monster details pane. */
static void ui_knowledge_monster_set_details_visual(int r_idx)
{
    monster_race* r_ptr;

    if (r_idx <= 0)
        return;

    r_ptr = &r_info[r_idx];
    if (use_graphics)
    {
        ui_menu_set_details_visual(
            UI_MENU_VISUAL_TILE, (byte)r_ptr->x_attr, (byte)r_ptr->x_char);
    }
    else
    {
        ui_menu_set_details_visual(
            UI_MENU_VISUAL_TEXT, (byte)r_ptr->d_attr, (byte)r_ptr->d_char);
    }
}

/* Appends the summary text shown above the monster browser groups. */
static void ui_knowledge_monster_group_append_summary(
    ui_text_builder* builder, cptr group_char[], int grp_cur)
{
    char buf[128];

    if (!builder)
        return;

    ui_knowledge_monster_group_summary(buf, sizeof(buf), group_char, grp_cur);
    ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
}

/* Returns the details-pane visual for one object browser entry. */
static bool ui_knowledge_object_entry_visual(
    const ui_knowledge_object_entry* obj, int* attr, int* chr, int* kind)
{
    int k_idx = -1;
    int i;
    object_kind* k_ptr;

    if (!obj || !attr || !chr || !kind)
        return FALSE;

    switch (obj->type)
    {
    case UI_KNOWLEDGE_OBJECT_NORMAL:
        k_idx = obj->idx;
        break;

    case UI_KNOWLEDGE_OBJECT_SPECIAL:
        for (i = 0; i < z_info->k_max; i++)
        {
            if (k_info[i].tval != obj->tval)
                continue;
            if ((obj->sval != -1) && (k_info[i].sval != obj->sval))
                continue;

            k_idx = i;
            break;
        }
        break;

    case UI_KNOWLEDGE_OBJECT_NONE:
    default:
        return FALSE;
    }

    if ((k_idx < 0) || (k_idx >= z_info->k_max))
        return FALSE;

    k_ptr = &k_info[k_idx];
    if (use_graphics)
    {
        *attr = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_attr : k_ptr->x_attr;
        *chr = k_ptr->flavor ? flavor_info[k_ptr->flavor].x_char : k_ptr->x_char;
        *kind = UI_MENU_VISUAL_TILE;
    }
    else
    {
        *attr = k_ptr->flavor ? flavor_info[k_ptr->flavor].d_attr : k_ptr->d_attr;
        *chr = k_ptr->flavor ? flavor_info[k_ptr->flavor].d_char : k_ptr->d_char;
        *kind = UI_MENU_VISUAL_TEXT;
    }

    return TRUE;
}

/* Appends the details pane text for one object browser entry. */
static void ui_knowledge_object_append_details(
    ui_text_builder* builder, const ui_knowledge_object_entry* obj)
{
    char buf[80];
    cptr title;
    byte title_attr = TERM_WHITE;

    if (!builder || !obj || (obj->type == UI_KNOWLEDGE_OBJECT_NONE))
        return;

    if ((obj->type == UI_KNOWLEDGE_OBJECT_NORMAL) && k_info[obj->idx].aware)
    {
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;

        if (!ui_preview_prepare_kind_object(i_ptr, obj->idx, TRUE))
            return;
        if (cheat_know)
        {
            strnfmt(buf, sizeof(buf), "Idx: %d", obj->idx);
            ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
            ui_text_builder_newline(builder, TERM_WHITE);
        }
        object_info_append_ui_text(builder, i_ptr, TRUE);
        return;
    }

    ui_knowledge_object_entry_label(buf, sizeof(buf), obj);
    title = buf;
    while (*title == ' ')
        title++;

    title_attr = ui_knowledge_object_entry_attr(obj, FALSE);
    ui_text_builder_append_line(builder, title, title_attr);
    if ((obj->type == UI_KNOWLEDGE_OBJECT_NORMAL) && cheat_know)
    {
        strnfmt(buf, sizeof(buf), "Idx: %d", obj->idx);
        ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
    }
    ui_text_builder_newline(builder, TERM_WHITE);

    if (obj->type == UI_KNOWLEDGE_OBJECT_SPECIAL)
    {
        ego_item_type* e_ptr = &e_info[obj->e_idx];

        if (e_ptr->aware && e_ptr->text)
        {
            ui_text_builder_append(builder, e_text + e_ptr->text, TERM_WHITE);
            if ((builder->off > 0) && (builder->text[builder->off - 1] != '\n'))
                ui_text_builder_newline(builder, TERM_WHITE);
        }
        else
        {
            ui_text_builder_append_line(builder,
                "This special item has not been fully identified.",
                TERM_SLATE);
        }

        return;
    }

    ui_text_builder_append_line(
        builder, "This item has not been identified.", TERM_SLATE);
}

/* Runs one interaction step for the top-level knowledge menu. */
static int ui_knowledge_menu_choose(int* highlight)
{
    int action;
    int i;
    ui_simple_menu_entry entries[6];

    if (!highlight)
        return 0;

    for (i = 0; i < 6; i++)
    {
        entries[i].key = 'a' + i;
        entries[i].row = 4 + i;
        entries[i].label = ui_knowledge_menu_labels[i];
        entries[i].details = ui_knowledge_menu_details[i];
    }

    ui_simple_menu_render("Display current knowledge", 2, 5, entries, 6,
        *highlight, "Use the keyboard or mouse to choose a knowledge screen.");

    action = ui_simple_menu_read_action(highlight, entries, 6);
    if (action == ESCAPE)
    {
        ui_menu_clear();
        return 0;
    }

    for (i = 0; i < 6; i++)
    {
        if (action == entries[i].key)
        {
            ui_menu_clear();
            return i + 1;
        }
    }

    return -1;
}

/* Formats the display name for one artefact browser entry. */
void ui_knowledge_artefact_name(int a_idx, char* name, size_t name_size)
{
    object_type object_type_body;

    if (!name || (name_size == 0))
        return;

    if (!ui_knowledge_artefact_prepare_info(
            a_idx, &object_type_body, name, name_size))
    {
        name[0] = '\0';
    }
}

/* Publishes the semantic artefact browser UI for the current selection. */
void ui_knowledge_publish_artefacts(cptr group_text[], int grp_idx[],
    int grp_cnt, int grp_cur, int grp_top, int artefact_idx[],
    int artefact_cnt, int artefact_cur, int artefact_top, int column)
{
    char menu_text[UI_KNOWLEDGE_TEXT_MAX];
    byte menu_attrs[UI_KNOWLEDGE_TEXT_MAX];
    char details_text[UI_KNOWLEDGE_TEXT_MAX];
    byte details_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    int i;

    ui_text_builder_init(
        &menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));

    ui_text_builder_append_line(&menu_builder, "Knowledge - Artefacts", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);

    if ((artefact_cnt > 0) && (artefact_cur >= 0) && (artefact_cur < artefact_cnt))
        ui_knowledge_artefact_append_details(
            &details_builder, artefact_idx[artefact_cur]);

    ui_menu_begin();
    ui_menu_set_layout_kind(UI_MENU_LAYOUT_BROWSER);

    for (i = 0; i < UI_KNOWLEDGE_BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        ui_knowledge_build_browser_nav(nav, sizeof(nav), column, grp_cur,
            artefact_cur, 0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE, group_text[grp_idx[group_index]],
            nav);
    }

    for (i = 0;
         i < UI_KNOWLEDGE_BROWSER_ROWS && (artefact_top + i < artefact_cnt); i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int artefact_index = artefact_top + i;

        ui_knowledge_artefact_name(
            artefact_idx[artefact_index], buf, sizeof(buf));
        ui_knowledge_build_browser_nav(nav, sizeof(nav), column, grp_cur,
            artefact_cur, 1, grp_cur, artefact_index);
        ui_menu_add_with_nav(1, 6 + i, (int)strlen(buf), 1, 0,
            artefact_index == artefact_cur, TERM_WHITE, buf, nav);
    }

    ui_menu_set_active_column(column ? 1 : 0);
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}

/* Shows the full recall modal for one artefact entry. */
void ui_knowledge_recall_artefact(int a_idx)
{
    char modal_text[UI_KNOWLEDGE_TEXT_MAX];
    byte modal_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder modal_builder;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    ui_knowledge_artefact_append_details(&modal_builder, a_idx);
    ui_knowledge_finish_modal(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    ui_preview_show_artefact_recall(a_idx);

    ui_modal_clear();
}

/* Publishes the semantic monster browser UI for the current selection. */
void ui_knowledge_publish_monsters(cptr group_text[], cptr group_char[],
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    ui_knowledge_monster_entry* mon_idx, int monster_count, int mon_cur, int mon_top,
    int column)
{
    char menu_text[UI_KNOWLEDGE_TEXT_MAX];
    byte menu_attrs[UI_KNOWLEDGE_TEXT_MAX];
    char details_text[UI_KNOWLEDGE_TEXT_MAX];
    byte details_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    int i;

    ui_text_builder_init(
        &menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));

    ui_text_builder_append_line(&menu_builder, "Knowledge - Monsters", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Browse groups on the left and monsters on the right.", TERM_SLATE);
    ui_text_builder_append_line(
        &menu_builder, "Press r for full recall. Press Esc to exit.", TERM_SLATE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_knowledge_monster_group_append_summary(
        &menu_builder, group_char, grp_idx[grp_cur]);

    if ((monster_count > 0) && (mon_cur >= 0) && (mon_cur < monster_count))
    {
        ui_knowledge_monster_append_details(
            &details_builder, mon_idx[mon_cur].r_idx);
    }
    else
    {
        ui_text_builder_append_line(&details_builder,
            "No known monsters in this group yet.", TERM_SLATE);
    }

    ui_menu_begin();
    ui_menu_set_layout_kind(UI_MENU_LAYOUT_BROWSER);

    for (i = 0; i < UI_KNOWLEDGE_BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        ui_knowledge_build_browser_nav(
            nav, sizeof(nav), column, grp_cur, mon_cur, 0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE, group_text[grp_idx[group_index]],
            nav);
    }

    for (i = 0; i < UI_KNOWLEDGE_BROWSER_ROWS && (mon_top + i < monster_count);
         i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int mon_index = mon_top + i;

        monster_desc_race(buf, sizeof(buf), mon_idx[mon_index].r_idx);
        ui_knowledge_build_browser_nav(
            nav, sizeof(nav), column, grp_cur, mon_cur, 1, grp_cur, mon_index);
        ui_menu_add_with_nav(1, 6 + i, (int)strlen(buf), 1, 0,
            mon_index == mon_cur, TERM_WHITE, buf, nav);
    }

    ui_menu_set_active_column(column ? 1 : 0);
    ui_menu_set_details_width(36);
    if ((monster_count > 0) && (mon_cur >= 0) && (mon_cur < monster_count))
        ui_knowledge_monster_set_details_visual(mon_idx[mon_cur].r_idx);
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}

/* Shows the full recall modal for one monster entry. */
void ui_knowledge_recall_monster(int r_idx)
{
    char modal_text[UI_KNOWLEDGE_TEXT_MAX];
    byte modal_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder modal_builder;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    ui_knowledge_monster_append_details(&modal_builder, r_idx);
    ui_knowledge_finish_modal(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    screen_roff(r_idx);
    (void)inkey();

    ui_modal_clear();
}

/* Formats the display label for one object browser entry. */
void ui_knowledge_object_entry_label(
    char* buf, size_t buf_size, const ui_knowledge_object_entry* obj)
{
    if (!buf || (buf_size == 0))
        return;

    buf[0] = '\0';

    if (!obj)
        return;

    switch (obj->type)
    {
    case UI_KNOWLEDGE_OBJECT_NORMAL:
        strip_name(buf, obj->idx);
        break;

    case UI_KNOWLEDGE_OBJECT_SPECIAL:
    {
        ego_item_type* e_ptr = &e_info[obj->e_idx];

        if (obj->sval == -1)
        {
            strnfmt(buf, buf_size, "  %s", &e_name[e_ptr->name]);
        }
        else
        {
            int j;
            char kind_name[79];

            kind_name[0] = '\0';
            for (j = 0; j < z_info->k_max; ++j)
            {
                if ((k_info[j].tval == obj->tval) && (k_info[j].sval == obj->sval))
                {
                    strip_name(kind_name, j);
                    break;
                }
            }

            strnfmt(buf, buf_size, "%s %s", kind_name, &e_name[e_ptr->name]);
        }
        break;
    }

    case UI_KNOWLEDGE_OBJECT_NONE:
    default:
        break;
    }
}

/* Returns the preferred terminal attr for one object browser entry. */
byte ui_knowledge_object_entry_attr(
    const ui_knowledge_object_entry* obj, bool selected)
{
    bool aware = FALSE;

    if (!obj)
        return TERM_WHITE;

    switch (obj->type)
    {
    case UI_KNOWLEDGE_OBJECT_NORMAL:
        aware = k_info[obj->idx].aware ? TRUE : FALSE;
        break;

    case UI_KNOWLEDGE_OBJECT_SPECIAL:
        aware = e_info[obj->e_idx].aware ? TRUE : FALSE;
        break;

    case UI_KNOWLEDGE_OBJECT_NONE:
    default:
        break;
    }

    if (selected)
        return aware ? TERM_L_BLUE : TERM_BLUE;

    return aware ? TERM_WHITE : TERM_SLATE;
}

/* Returns a stable tracking id for one object browser entry. */
int ui_knowledge_object_entry_id(const ui_knowledge_object_entry* obj)
{
    if (!obj)
        return -1;

    switch (obj->type)
    {
    case UI_KNOWLEDGE_OBJECT_NORMAL:
        return obj->idx + 1;

    case UI_KNOWLEDGE_OBJECT_SPECIAL:
        return -(obj->e_idx + 1);

    case UI_KNOWLEDGE_OBJECT_NONE:
    default:
        return 0;
    }
}

/* Publishes the semantic object browser UI for the current selection. */
void ui_knowledge_publish_objects(cptr group_text[], int grp_idx[], int grp_cnt,
    int grp_cur, int grp_top, ui_knowledge_object_entry object_idx[], int object_cnt,
    int object_cur, int object_top, int column)
{
    char menu_text[UI_KNOWLEDGE_TEXT_MAX];
    byte menu_attrs[UI_KNOWLEDGE_TEXT_MAX];
    char details_text[UI_KNOWLEDGE_TEXT_MAX];
    byte details_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    int details_visual_attr;
    int details_visual_char;
    int details_visual_kind;
    int i;

    ui_text_builder_init(
        &menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));

    ui_text_builder_append_line(&menu_builder, "Knowledge - Objects", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);

    if ((object_cnt > 0) && (object_cur >= 0) && (object_cur < object_cnt))
        ui_knowledge_object_append_details(&details_builder, &object_idx[object_cur]);

    ui_menu_begin();
    ui_menu_set_layout_kind(UI_MENU_LAYOUT_BROWSER);

    for (i = 0; i < UI_KNOWLEDGE_BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        ui_knowledge_build_browser_nav(nav, sizeof(nav), column, grp_cur,
            object_cur, 0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE, group_text[grp_idx[group_index]],
            nav);
    }

    for (i = 0; i < UI_KNOWLEDGE_BROWSER_ROWS && (object_top + i < object_cnt);
         i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int object_index = object_top + i;
        ui_knowledge_object_entry* obj = &object_idx[object_index];

        ui_knowledge_object_entry_label(buf, sizeof(buf), obj);
        ui_knowledge_build_browser_nav(nav, sizeof(nav), column, grp_cur,
            object_cur, 1, grp_cur, object_index);
        ui_menu_add_with_nav(1, 6 + i, (int)strlen(buf), 1, 0,
            object_index == object_cur,
            ui_knowledge_object_entry_attr(obj, FALSE), buf, nav);
    }

    ui_menu_set_active_column(column ? 1 : 0);
    ui_menu_set_details_width(34);
    if ((object_cnt > 0) && (object_cur >= 0) && (object_cur < object_cnt)
        && ui_knowledge_object_entry_visual(&object_idx[object_cur],
            &details_visual_attr, &details_visual_char, &details_visual_kind))
    {
        ui_menu_set_details_visual(
            details_visual_kind, details_visual_attr, details_visual_char);
    }
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}

/* Shows the full recall modal for one object browser entry. */
void ui_knowledge_recall_object(const ui_knowledge_object_entry* obj)
{
    char modal_text[UI_KNOWLEDGE_TEXT_MAX];
    byte modal_attrs[UI_KNOWLEDGE_TEXT_MAX];
    ui_text_builder modal_builder;

    if (!obj)
        return;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    ui_knowledge_object_append_details(&modal_builder, obj);
    ui_knowledge_finish_modal(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    if ((obj->type == UI_KNOWLEDGE_OBJECT_NORMAL) && k_info[obj->idx].aware)
        ui_preview_show_kind_recall(obj->idx);

    ui_modal_clear();
}

/* Applies one pending semantic browser-row selection request, if any. */
bool ui_knowledge_apply_browser_request(int* column, int* grp_cur, int grp_top,
    int grp_cnt, int* list_cur, int list_top, int list_cnt)
{
    const ui_menu_item* items = ui_menu_get_items();
    int item_count = ui_menu_get_item_count();
    int requested = ui_menu_consume_requested_index();
    int row;

    if (!column || !grp_cur || !list_cur)
        return FALSE;

    if ((requested < 0) || (requested >= item_count))
        return FALSE;

    row = items[requested].y - 6;
    if (row < 0)
        return FALSE;

    if (items[requested].x <= 0)
    {
        *column = 0;
        if (grp_cnt > 0)
            *grp_cur = MIN(grp_top + row, grp_cnt - 1);
        else
            *grp_cur = 0;
        return TRUE;
    }

    if (items[requested].x == 1)
    {
        *column = 1;
        if (list_cnt > 0)
            *list_cur = MIN(list_top + row, list_cnt - 1);
        else
            *list_cur = 0;
        return TRUE;
    }

    return FALSE;
}

/* Runs the top-level knowledge command loop. */
void do_cmd_knowledge(void)
{
    int highlight = 1;

    FILE_TYPE(FILE_TYPE_TEXT);
    screen_save();

    while (TRUE)
    {
        int choice;

        Term_clear();
        Term_fresh();
        choice = ui_knowledge_menu_choose(&highlight);

        if (choice == 0)
            break;

        if (choice == 1)
            do_cmd_knowledge_artefacts();
        else if (choice == 2)
            do_cmd_knowledge_monsters();
        else if (choice == 3)
            do_cmd_knowledge_objects();
        else if (choice == 4)
            show_scores();
        else if (choice == 5)
            do_cmd_knowledge_kills();
        else if (choice == 6)
            do_cmd_knowledge_notes();
        else if (choice != -1)
            bell("Illegal command for knowledge!");

        message_flush();
    }

    screen_load();
}
