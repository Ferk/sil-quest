/* File: ui-model.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL 
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral methods for building structured UI text
 * with a parallel attribute buffer.
 */

#include "angband.h"

#include "ui-input.h"
#include "ui-model.h"

#define UI_MENU_ITEMS_MAX 96
#define UI_MENU_TEXT_MAX 16384

static ui_menu_item ui_menu_items[UI_MENU_ITEMS_MAX];
static int ui_menu_item_count = 0;
static int ui_menu_selected_hint = -1;
static int ui_menu_active_x = 0;
static bool ui_menu_active_x_valid = FALSE;
static bool ui_menu_active_all_columns = FALSE;
static unsigned int ui_menu_revision = 1;
static char ui_menu_text[UI_MENU_TEXT_MAX];
static byte ui_menu_attrs[UI_MENU_TEXT_MAX];
static int ui_menu_attrs_len = 0;
static char ui_menu_details[UI_MENU_TEXT_MAX];
static byte ui_menu_details_attrs[UI_MENU_TEXT_MAX];
static int ui_menu_details_attrs_len = 0;
static int ui_menu_details_width = 0;
static int ui_menu_details_visual_kind = UI_MENU_VISUAL_NONE;
static int ui_menu_details_visual_attr = TERM_WHITE;
static int ui_menu_details_visual_char = 0;
static bool ui_menu_snapshot_keep = TRUE;
static unsigned int ui_modal_revision = 1;
static char ui_modal_text[UI_MENU_TEXT_MAX];
static byte ui_modal_attrs[UI_MENU_TEXT_MAX];
static int ui_modal_attrs_len = 0;
static int ui_modal_dismiss_key = 0;
static int ui_saved_screen_depth = 0;
static bool ui_saved_screen_restored = FALSE;
static unsigned int ui_saved_screen_revision = 1;

/* Bumps the menu revision so frontends can detect updates. */
static void ui_menu_touch(void)
{
    ui_menu_revision++;
    if (ui_menu_revision == 0)
        ui_menu_revision = 1;
}

/* Bumps the modal revision so frontends can detect updates. */
static void ui_modal_touch(void)
{
    ui_modal_revision++;
    if (ui_modal_revision == 0)
        ui_modal_revision = 1;
}

/* Bumps the saved-screen revision so frontends can detect lifecycle changes. */
static void ui_saved_screen_touch(void)
{
    ui_saved_screen_revision++;
    if (ui_saved_screen_revision == 0)
        ui_saved_screen_revision = 1;
}

/* Clamps one exported visual type to the supported menu range. */
static int ui_menu_normalize_visual_kind(int visual_kind)
{
    if ((visual_kind < UI_MENU_VISUAL_NONE) || (visual_kind > UI_MENU_VISUAL_TILE))
        return UI_MENU_VISUAL_NONE;

    return visual_kind;
}

/* Saves the current text_out() globals and applies a temporary override. */
void ui_text_output_begin(ui_text_output_state* state,
    void (*hook)(byte a, cptr str), int indent, int wrap)
{
    if (!state)
        return;

    state->hook = text_out_hook;
    state->wrap = text_out_wrap;
    state->indent = text_out_indent;

    text_out_hook = hook;
    text_out_indent = indent;
    text_out_wrap = wrap;
}

/* Restores the previous text_out() globals after a temporary override. */
void ui_text_output_end(const ui_text_output_state* state)
{
    if (!state)
        return;

    text_out_hook = state->hook;
    text_out_wrap = state->wrap;
    text_out_indent = state->indent;
}

/* Resets all exported semantic menu state to an empty menu. */
static void ui_menu_reset_state(void)
{
    ui_menu_item_count = 0;
    ui_menu_selected_hint = -1;
    ui_menu_active_x = 0;
    ui_menu_active_x_valid = FALSE;
    ui_menu_active_all_columns = FALSE;
    ui_menu_text[0] = '\0';
    ui_menu_attrs_len = 0;
    ui_menu_details[0] = '\0';
    ui_menu_details_attrs_len = 0;
    ui_menu_details_width = 0;
    ui_menu_details_visual_kind = UI_MENU_VISUAL_NONE;
    ui_menu_details_visual_attr = TERM_WHITE;
    ui_menu_details_visual_char = 0;
}

/* Copies plain text plus optional attrs into one exported UI buffer. */
static void ui_model_set_buffer_text(char* text_buf, byte* attr_buf, int* attr_len,
    size_t buf_size, cptr text, const byte* attrs, int attrs_len)
{
    size_t text_len = 0;
    size_t i;

    if (!text_buf || !attr_buf || !attr_len || (buf_size == 0))
        return;

    if (text)
        text_len = strlen(text);
    if (text_len >= buf_size)
        text_len = buf_size - 1;

    if (text_len > 0)
        memcpy(text_buf, text, text_len);
    text_buf[text_len] = '\0';

    *attr_len = (int)text_len;
    for (i = 0; i < text_len; i++)
    {
        if (attrs && ((int)i < attrs_len))
            attr_buf[i] = attrs[i];
        else
            attr_buf[i] = TERM_WHITE;
    }
}

/* Starts writing into one text-plus-attrs builder. */
void ui_text_builder_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size)
{
    if (!builder)
        return;

    builder->text = text;
    builder->attrs = attrs;
    builder->size = size;
    builder->off = 0;

    if (text && (size > 0))
        text[0] = '\0';
}

/* Appends one text run with a uniform terminal attr. */
void ui_text_builder_append(
    ui_text_builder* builder, cptr text, byte attr)
{
    const char* s = text ? text : "";

    if (!builder || !builder->text || !builder->attrs || (builder->size == 0))
        return;

    while (*s && (builder->off + 1 < builder->size))
    {
        builder->text[builder->off] = *s++;
        builder->attrs[builder->off] = attr;
        builder->off++;
    }

    builder->text[builder->off] = '\0';
}

/* Appends a newline to one text builder. */
void ui_text_builder_newline(ui_text_builder* builder, byte attr)
{
    if (!builder || !builder->text || !builder->attrs || (builder->size == 0)
        || (builder->off + 1 >= builder->size))
    {
        return;
    }

    builder->text[builder->off] = '\n';
    builder->attrs[builder->off] = attr;
    builder->off++;
    builder->text[builder->off] = '\0';
}

/* Appends one line of text followed by a newline. */
void ui_text_builder_append_line(
    ui_text_builder* builder, cptr text, byte attr)
{
    ui_text_builder_append(builder, text, attr);
    ui_text_builder_newline(builder, attr);
}

/* Returns the current text length stored in one builder. */
int ui_text_builder_length(const ui_text_builder* builder)
{
    if (!builder)
        return 0;

    return (int)builder->off;
}

/* Renders one simple vertical text menu plus its details pane. */
void ui_simple_menu_render(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details)
{
    ui_simple_menu_render_custom(title, title_row, col, entries, entry_count,
        highlight, extra_details, 0, UI_MENU_VISUAL_NONE, TERM_WHITE, 0);
}

/* Renders one simple vertical text menu with custom details-pane settings. */
void ui_simple_menu_render_custom(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details, int details_width, int details_visual_kind,
    int details_visual_attr, int details_visual_char)
{
    int i;
    char menu_text[1024];
    byte menu_attrs[1024];
    char menu_details[2048];
    byte menu_details_attrs[2048];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, menu_details, menu_details_attrs, sizeof(menu_details));

    Term_putstr(col - 3, title_row, -1, TERM_WHITE, title);
    ui_text_builder_append_line(&menu_builder, title, TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Use 8/2 to move, Enter/Space to choose, Escape to cancel, or click an option.",
        TERM_SLATE);

    ui_menu_begin();
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details_width(details_width);
    ui_menu_set_details_visual(
        details_visual_kind, details_visual_attr, details_visual_char);

    for (i = 0; i < entry_count; i++)
    {
        byte attr = (i + 1 == highlight) ? TERM_L_BLUE : TERM_WHITE;
        cptr label = entries[i].label ? entries[i].label : "";
        size_t label_len = strlen(label);

        Term_putstr(col, entries[i].row, -1, attr, label);
        ui_menu_add(col, entries[i].row, (int)label_len, 1, entries[i].key,
            (i + 1 == highlight), TERM_WHITE, label);
    }

    if ((highlight >= 1) && (highlight <= entry_count))
    {
        const ui_simple_menu_entry* selected = &entries[highlight - 1];

        ui_text_builder_append_line(&details_builder,
            selected->label ? selected->label : "", TERM_L_WHITE);
        ui_text_builder_newline(&details_builder, TERM_WHITE);
        ui_text_builder_append_line(&details_builder,
            selected->details ? selected->details : "", TERM_SLATE);
    }

    if (extra_details && extra_details[0] != '\0')
    {
        if (ui_text_builder_length(&details_builder) > 0)
        {
            ui_text_builder_newline(&details_builder, TERM_WHITE);
            ui_text_builder_newline(&details_builder, TERM_WHITE);
        }
        ui_text_builder_append_line(&details_builder, extra_details, TERM_SLATE);
    }

    ui_menu_set_details(
        menu_details, menu_details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}

/* Reads one action from a simple vertical text menu. */
int ui_simple_menu_read_action(
    int* highlight, const ui_simple_menu_entry* entries, int entry_count)
{
    int i;
    char ch;
    ui_input_simple_menu_action action;

    if (entry_count <= 0)
        return ESCAPE;

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > entry_count)
        *highlight = entry_count;

    hide_cursor = TRUE;
    ch = inkey();
    hide_cursor = FALSE;

    action = ui_input_parse_simple_menu_key(ch);
    if (action == UI_INPUT_SIMPLE_MENU_ACTION_CANCEL)
        return ESCAPE;

    if (action == UI_INPUT_SIMPLE_MENU_ACTION_CHOOSE)
        return entries[*highlight - 1].key;

    for (i = 0; i < entry_count; i++)
    {
        if (ch == entries[i].key)
        {
            *highlight = i + 1;
            return entries[i].key;
        }
    }

    if (action == UI_INPUT_SIMPLE_MENU_ACTION_PREV)
    {
        *highlight = (*highlight + entry_count - 2) % entry_count + 1;
        return 0;
    }

    if (action == UI_INPUT_SIMPLE_MENU_ACTION_NEXT)
    {
        *highlight = *highlight % entry_count + 1;
        return 0;
    }

    return -1;
}

/* Begins publishing a fresh semantic menu frame. */
void ui_menu_begin(void)
{
    ui_menu_reset_state();
    ui_menu_snapshot_keep = TRUE;
    ui_menu_touch();
}

/* Adds a plain-text menu item with no visual or custom navigation. */
void ui_menu_add(
    int x, int y, int w, int h, int key, int selected, int attr, cptr label)
{
    ui_menu_add_visual_with_nav(x, y, w, h, key, selected, attr,
        UI_MENU_VISUAL_NONE, TERM_WHITE, 0, label, NULL);
}

/* Adds one menu item with an explicit visual payload. */
void ui_menu_add_visual(int x, int y, int w, int h, int key, int selected,
    int attr, int visual_kind, int visual_attr, int visual_char, cptr label)
{
    ui_menu_add_visual_with_nav(x, y, w, h, key, selected, attr, visual_kind,
        visual_attr, visual_char, label, NULL);
}

/* Adds one plain-text menu item with synthetic navigation hints. */
void ui_menu_add_with_nav(int x, int y, int w, int h, int key, int selected,
    int attr, cptr label, cptr nav)
{
    ui_menu_add_visual_with_nav(x, y, w, h, key, selected, attr,
        UI_MENU_VISUAL_NONE, TERM_WHITE, 0, label, nav);
}

/* Adds one fully-specified semantic menu item to the current frame. */
void ui_menu_add_visual_with_nav(int x, int y, int w, int h, int key,
    int selected, int attr, int visual_kind, int visual_attr, int visual_char,
    cptr label, cptr nav)
{
    ui_menu_item* item;
    size_t nav_len = 0;

    if ((w <= 0) || (h <= 0) || (key < 0))
        return;

    if (ui_menu_item_count >= UI_MENU_ITEMS_MAX)
        return;

    visual_kind = ui_menu_normalize_visual_kind(visual_kind);

    item = &ui_menu_items[ui_menu_item_count];
    item->x = x;
    item->y = y;
    item->w = w;
    item->h = h;
    item->key = key;
    item->selected = selected ? 1 : 0;
    item->attr = attr;
    item->visual_kind = visual_kind;
    item->visual_attr = visual_attr;
    item->visual_char = visual_char;
    my_strcpy(item->label, label ? label : "", sizeof(item->label));
    item->nav_len = 0;
    item->nav[0] = '\0';
    if (nav)
    {
        nav_len = strlen(nav);
        if (nav_len >= sizeof(item->nav))
            nav_len = sizeof(item->nav) - 1;
        if (nav_len > 0)
            memcpy(item->nav, nav, nav_len);
        item->nav[nav_len] = '\0';
        item->nav_len = (int)nav_len;
    }
    if (selected)
        ui_menu_selected_hint = ui_menu_item_count;
    ui_menu_item_count++;
    ui_menu_touch();
}

/* Publishes the menu's main text block. */
void ui_menu_set_text(cptr text, const byte* attrs, int attrs_len)
{
    ui_model_set_buffer_text(ui_menu_text, ui_menu_attrs, &ui_menu_attrs_len,
        sizeof(ui_menu_text), text, attrs, attrs_len);
    ui_menu_touch();
}

/* Publishes the menu's details pane text block. */
void ui_menu_set_details(cptr text, const byte* attrs, int attrs_len)
{
    ui_model_set_buffer_text(ui_menu_details, ui_menu_details_attrs,
        &ui_menu_details_attrs_len, sizeof(ui_menu_details), text, attrs,
        attrs_len);
    ui_menu_touch();
}

/* Sets the preferred details-pane width in character cells. */
void ui_menu_set_details_width(int chars)
{
    ui_menu_details_width = (chars > 0) ? chars : 0;
    ui_menu_touch();
}

/* Publishes the visual preview shown beside the details text. */
void ui_menu_set_details_visual(int kind, int attr, int chr)
{
    kind = ui_menu_normalize_visual_kind(kind);

    ui_menu_details_visual_kind = kind;
    ui_menu_details_visual_attr = attr;
    ui_menu_details_visual_char = chr;
    ui_menu_touch();
}

/* Marks the current menu frame as complete. */
void ui_menu_end(void)
{
    ui_menu_touch();
}

/* Restricts menu selection to one column, or all columns with `-1`. */
void ui_menu_set_active_column(int x)
{
    if (x < 0)
    {
        ui_menu_active_all_columns = TRUE;
        ui_menu_active_x = 0;
        ui_menu_active_x_valid = FALSE;
        ui_menu_touch();
        return;
    }

    ui_menu_active_all_columns = FALSE;
    ui_menu_active_x = x;
    ui_menu_active_x_valid = TRUE;
    ui_menu_touch();
}

/* Clears the exported semantic menu state. */
void ui_menu_clear(void)
{
    ui_menu_reset_state();
    ui_menu_touch();
}

/* Controls whether the last menu may remain visible during follow-up prompts. */
void ui_menu_set_snapshot_retained(bool retained)
{
    ui_menu_snapshot_keep = retained ? TRUE : FALSE;
}

/* Returns whether follow-up prompts may reuse the last rendered menu. */
bool ui_menu_snapshot_retained(void)
{
    return ui_menu_snapshot_keep;
}

/* Returns the current semantic menu item array. */
const ui_menu_item* ui_menu_get_items(void)
{
    return ui_menu_items;
}

/* Returns how many semantic menu items are currently exported. */
int ui_menu_get_item_count(void)
{
    return ui_menu_item_count;
}

/* Returns the currently selected menu item index, if any. */
int ui_menu_get_selected_index(void)
{
    int i;

    if ((ui_menu_selected_hint >= 0) && (ui_menu_selected_hint < ui_menu_item_count))
        return ui_menu_selected_hint;

    for (i = 0; i < ui_menu_item_count; i++)
    {
        if (ui_menu_items[i].selected)
            return i;
    }

    return -1;
}

/* Selects one menu item and updates the active column state. */
int ui_menu_select_index(int index)
{
    int i;
    int column_x;

    if ((index < 0) || (index >= ui_menu_item_count))
        return 0;

    column_x = ui_menu_items[index].x;
    if (ui_menu_active_all_columns)
    {
        for (i = 0; i < ui_menu_item_count; i++)
        {
            ui_menu_items[i].selected = (i == index) ? 1 : 0;
        }
    }
    else
    {
        for (i = 0; i < ui_menu_item_count; i++)
        {
            if (ui_menu_items[i].x == column_x)
                ui_menu_items[i].selected = (i == index) ? 1 : 0;
        }
    }
    ui_menu_selected_hint = index;
    if (!ui_menu_active_all_columns)
    {
        ui_menu_active_x = column_x;
        ui_menu_active_x_valid = TRUE;
    }
    ui_menu_touch();

    return 1;
}

/* Returns the active menu column, or `-1` when all are active. */
int ui_menu_get_active_column(void)
{
    int i;
    int max_x;

    if (ui_menu_active_all_columns)
        return -1;

    if (ui_menu_active_x_valid)
        return ui_menu_active_x;

    if ((ui_menu_selected_hint >= 0) && (ui_menu_selected_hint < ui_menu_item_count))
        return ui_menu_items[ui_menu_selected_hint].x;

    if (ui_menu_item_count <= 0)
        return 0;

    max_x = ui_menu_items[0].x;
    for (i = 1; i < ui_menu_item_count; i++)
    {
        if (ui_menu_items[i].x > max_x)
            max_x = ui_menu_items[i].x;
    }

    return max_x;
}

/* Returns the menu revision used by the frontend cache. */
unsigned int ui_menu_get_revision(void)
{
    return ui_menu_revision;
}

/* Returns the current menu text buffer. */
const char* ui_menu_get_text(void)
{
    return ui_menu_text;
}

/* Returns the current menu text length. */
int ui_menu_get_text_len(void)
{
    return (int)strlen(ui_menu_text);
}

/* Returns the attrs paired with the menu text buffer. */
const byte* ui_menu_get_attrs(void)
{
    return ui_menu_attrs;
}

/* Returns how many attrs are valid in the menu text buffer. */
int ui_menu_get_attrs_len(void)
{
    return ui_menu_attrs_len;
}

/* Returns the current menu details text buffer. */
const char* ui_menu_get_details(void)
{
    return ui_menu_details;
}

/* Returns the current menu details text length. */
int ui_menu_get_details_len(void)
{
    return (int)strlen(ui_menu_details);
}

/* Returns the attrs paired with the menu details buffer. */
const byte* ui_menu_get_details_attrs(void)
{
    return ui_menu_details_attrs;
}

/* Returns how many attrs are valid in the menu details buffer. */
int ui_menu_get_details_attrs_len(void)
{
    return ui_menu_details_attrs_len;
}

/* Returns the requested details-pane width in characters. */
int ui_menu_get_details_width(void)
{
    return ui_menu_details_width;
}

/* Returns the visual kind exported for the details pane. */
int ui_menu_get_details_visual_kind(void)
{
    return ui_menu_details_visual_kind;
}

/* Returns the visual attr exported for the details pane. */
int ui_menu_get_details_visual_attr(void)
{
    return ui_menu_details_visual_attr;
}

/* Returns the visual character exported for the details pane. */
int ui_menu_get_details_visual_char(void)
{
    return ui_menu_details_visual_char;
}

/* Publishes one modal text block plus its dismiss key. */
void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key)
{
    ui_model_set_buffer_text(ui_modal_text, ui_modal_attrs, &ui_modal_attrs_len,
        sizeof(ui_modal_text), text, attrs, attrs_len);
    ui_modal_dismiss_key = dismiss_key;
    ui_modal_touch();
}

/* Clears the exported modal state. */
void ui_modal_clear(void)
{
    ui_modal_text[0] = '\0';
    ui_modal_attrs_len = 0;
    ui_modal_dismiss_key = 0;
    ui_modal_touch();
}

/* Returns the current modal text buffer. */
const char* ui_modal_get_text(void)
{
    return ui_modal_text;
}

/* Returns the current modal text length. */
int ui_modal_get_text_len(void)
{
    return (int)strlen(ui_modal_text);
}

/* Returns the attrs paired with the modal text buffer. */
const byte* ui_modal_get_attrs(void)
{
    return ui_modal_attrs;
}

/* Returns how many attrs are valid in the modal text buffer. */
int ui_modal_get_attrs_len(void)
{
    return ui_modal_attrs_len;
}

/* Returns the modal's configured dismiss key. */
int ui_modal_get_dismiss_key(void)
{
    return ui_modal_dismiss_key;
}

/* Returns the modal revision used by the frontend cache. */
unsigned int ui_modal_get_revision(void)
{
    return ui_modal_revision;
}

/* Marks the start of one saved-screen scope such as screen_save(). */
void ui_saved_screen_begin(void)
{
    ui_saved_screen_depth++;
    ui_saved_screen_restored = FALSE;
    ui_saved_screen_touch();
}

/* Marks the end of one saved-screen scope such as screen_load(). */
void ui_saved_screen_end(void)
{
    if (ui_saved_screen_depth > 0)
        ui_saved_screen_depth--;

    if (ui_saved_screen_depth == 0)
        ui_saved_screen_restored = TRUE;

    ui_saved_screen_touch();
}

/* Returns the current nested saved-screen depth. */
int ui_saved_screen_get_depth(void)
{
    return ui_saved_screen_depth;
}

/* Returns whether the last saved screen was restored and not reopened yet. */
bool ui_saved_screen_was_restored(void)
{
    return ui_saved_screen_restored;
}

/* Returns the revision used by frontends to detect saved-screen transitions. */
unsigned int ui_saved_screen_get_revision(void)
{
    return ui_saved_screen_revision;
}
