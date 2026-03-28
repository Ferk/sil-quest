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
#define UI_MENU_TEXT_MAX (MESSAGE_BUF * 2)

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
static char ui_menu_summary[UI_MENU_TEXT_MAX];
static byte ui_menu_summary_attrs[UI_MENU_TEXT_MAX];
static int ui_menu_summary_attrs_len = 0;
static int ui_menu_details_width = 0;
static int ui_menu_details_rows = 0;
static int ui_menu_summary_rows = 0;
static int ui_menu_details_visual_kind = UI_MENU_VISUAL_NONE;
static int ui_menu_details_visual_attr = TERM_WHITE;
static int ui_menu_details_visual_char = 0;
static bool ui_menu_snapshot_keep = TRUE;
static unsigned int ui_modal_revision = 1;
static char ui_modal_text[UI_MENU_TEXT_MAX];
static byte ui_modal_attrs[UI_MENU_TEXT_MAX];
static int ui_modal_attrs_len = 0;
static int ui_modal_dismiss_key = 0;
static ui_modal_kind ui_modal_kind_value = UI_MODAL_KIND_GENERIC;
static unsigned int ui_prompt_revision = 1;
static char ui_prompt_text[UI_MENU_TEXT_MAX];
static byte ui_prompt_attrs[UI_MENU_TEXT_MAX];
static int ui_prompt_attrs_len = 0;
static ui_prompt_kind ui_prompt_kind_value = UI_PROMPT_KIND_NONE;
static bool ui_prompt_more_hint = FALSE;
static ui_prompt_kind ui_prompt_pending_kind = UI_PROMPT_KIND_NONE;
static bool ui_prompt_pending_more_hint = FALSE;
static ui_prompt_render_hook ui_front_prompt_render = NULL;
static ui_prompt_clear_hook ui_front_prompt_clear = NULL;
static ui_menu_render_hook ui_front_menu_render = NULL;
static int ui_saved_screen_depth = 0;
static bool ui_saved_screen_restored = FALSE;
static unsigned int ui_saved_screen_revision = 1;
static bool ui_message_recall_semantic = FALSE;

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

/* Bumps the prompt revision so frontends can detect updates. */
static void ui_prompt_touch(void)
{
    ui_prompt_revision++;
    if (ui_prompt_revision == 0)
        ui_prompt_revision = 1;
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
    ui_menu_summary[0] = '\0';
    ui_menu_summary_attrs_len = 0;
    ui_menu_details_width = 0;
    ui_menu_details_rows = 0;
    ui_menu_summary_rows = 0;
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

/* Returns the plain title text for one prefixed simple-menu label. */
static cptr ui_model_simple_menu_title(cptr label)
{
    const char* title = label ? label : "";
    int i = 0;

    while (isspace((unsigned char)title[i]))
        i++;

    if (title[i] == '(')
    {
        int end = i + 1;

        while (title[end] && (title[end] != ')') && !isspace((unsigned char)title[end]))
            end++;
        if (title[end] == ')')
        {
            end++;
            while (isspace((unsigned char)title[end]))
                end++;
            if (title[end])
                return title + end;
        }
    }
    else if (title[i] && !isspace((unsigned char)title[i]))
    {
        int end = i;

        while (title[end] && (title[end] != ')') && !isspace((unsigned char)title[end]))
            end++;
        if (title[end] == ')')
        {
            end++;
            while (isspace((unsigned char)title[end]))
                end++;
            if (title[end])
                return title + end;
        }
    }

    return title + i;
}

/* Stores one new semantic prompt state and invalidates frontend caches. */
static void ui_prompt_set_state(
    cptr text, byte attr, ui_prompt_kind kind, bool has_more_hint)
{
    ui_model_set_buffer_text(ui_prompt_text, ui_prompt_attrs, &ui_prompt_attrs_len,
        sizeof(ui_prompt_text), text, NULL, 0);

    if (ui_prompt_attrs_len > 0)
        memset(ui_prompt_attrs, attr, (size_t)ui_prompt_attrs_len);

    ui_prompt_kind_value = kind;
    ui_prompt_more_hint = has_more_hint ? TRUE : FALSE;
    ui_prompt_touch();
}

/* Asks the active frontend to redraw the current semantic top-line prompt. */
static void ui_prompt_render_current_frontend(int row, int col)
{
    if (ui_front_prompt_render)
        ui_front_prompt_render(row, col);
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

/* Clamps one menu selection index into the valid range for its list. */
static int ui_menu_clamp_index(int current, int count)
{
    if (count <= 0)
        return 0;

    if (current < 0)
        return 0;

    if (current >= count)
        return count - 1;

    return current;
}

/* Registers frontend-specific rendering hooks for prompts and menus. */
void ui_front_set_hooks(ui_prompt_render_hook prompt_render,
    ui_prompt_clear_hook prompt_clear, ui_menu_render_hook menu_render)
{
    ui_front_prompt_render = prompt_render;
    ui_front_prompt_clear = prompt_clear;
    ui_front_menu_render = menu_render;
}

/* Keeps one paged menu selection visible inside its current viewport. */
void ui_menu_scroll_selection_into_view(
    int current, int* top, int count, int page_rows)
{
    int max_top;

    if (!top)
        return;

    if ((count <= 0) || (page_rows <= 0))
    {
        *top = 0;
        return;
    }

    current = ui_menu_clamp_index(current, count);
    max_top = count - page_rows;
    if (max_top < 0)
        max_top = 0;

    if (*top < 0)
        *top = 0;
    if (*top > max_top)
        *top = max_top;

    if (current < *top)
        *top = current;
    else if (current >= *top + page_rows)
        *top = current - page_rows + 1;

    if (*top > max_top)
        *top = max_top;
}

/* Moves one two-column menu selection using viewport-aware semantics. */
void ui_menu_move_two_column_selection(int direction, int page_rows,
    int* column, int* left_cur, int left_top, int left_count, int* right_cur,
    int right_top, int right_count)
{
    int active_column;
    int dx;
    int dy;
    int* active_cur;
    int active_count;
    int* target_cur;
    int target_count;

    if (!column || !left_cur || !right_cur || !direction)
        return;

    (void)left_top;
    (void)right_top;

    active_column = (*column > 0) ? 1 : 0;
    dx = ddx[direction];
    dy = ddy[direction];

    active_cur = active_column ? right_cur : left_cur;
    active_count = active_column ? right_count : left_count;
    *active_cur = ui_menu_clamp_index(*active_cur, active_count);

    if (dx && !dy)
    {
        if ((active_column == 0) && (right_count > 0))
        {
            *column = 1;
            *right_cur = ui_menu_clamp_index(*right_cur, right_count);
        }
        else if ((active_column == 1) && (left_count > 0))
        {
            *column = 0;
            *left_cur = ui_menu_clamp_index(*left_cur, left_count);
        }

        return;
    }

    if (dx && dy)
    {
        if (page_rows <= 0)
            page_rows = 1;
        *active_cur = ui_menu_clamp_index(*active_cur + (dy * page_rows),
            active_count);
        return;
    }

    if (!dy)
        return;

    *active_cur = ui_menu_clamp_index(*active_cur + dy, active_count);

    target_cur = active_column ? right_cur : left_cur;
    target_count = active_column ? right_count : left_count;
    *target_cur = ui_menu_clamp_index(*target_cur, target_count);
}

/* Renders one simple vertical text menu plus its details pane. */
void ui_model_publish_simple_menu(cptr title, int title_row, int col,
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

    (void)title_row;
    (void)col;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, menu_details, menu_details_attrs, sizeof(menu_details));

    ui_text_builder_append_line(&menu_builder, title, TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Use 8/2 to move, 6 or Enter/Space to choose, 4 or Escape to cancel, or click an option.",
        TERM_SLATE);

    ui_menu_begin();
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details_width(details_width);
    ui_menu_set_details_visual(
        details_visual_kind, details_visual_attr, details_visual_char);

    for (i = 0; i < entry_count; i++)
    {
        cptr label = entries[i].label ? entries[i].label : "";
        size_t label_len = strlen(label);

        ui_menu_add(0, entries[i].row, (int)label_len, 1, entries[i].key,
            (i + 1 == highlight), TERM_WHITE, label);
    }

    if ((highlight >= 1) && (highlight <= entry_count))
    {
        const ui_simple_menu_entry* selected = &entries[highlight - 1];
        cptr title = ui_model_simple_menu_title(selected->label);

        ui_text_builder_append_line(&details_builder,
            title, TERM_YELLOW);
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
    ui_model_publish_simple_menu(title, title_row, col, entries, entry_count,
        highlight, extra_details, details_width, details_visual_kind,
        details_visual_attr, details_visual_char);
    ui_menu_render_current();
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

    for (i = 0; i < entry_count; i++)
    {
        int entry_key = entries[i].key;

        if ((ch == entry_key)
            || (isalpha((unsigned char)ch)
                && isalpha((unsigned char)entry_key)
                && (tolower((unsigned char)ch)
                    == tolower((unsigned char)entry_key))))
        {
            *highlight = i + 1;
            return entry_key;
        }
    }

    return -1;
}

/* Runs one command-style simple menu and returns the chosen 1-based index. */
int ui_command_menu_choose(cptr title, int col,
    const ui_simple_menu_entry* entries, int entry_count, int* highlight,
    cptr extra_details, int escape_choice, int escape_result)
{
    int action;
    int i;

    if (!highlight || !entries || (entry_count <= 0))
        return 0;

    ui_simple_menu_render(
        title, 1, col, entries, entry_count, *highlight, extra_details);

    action = ui_simple_menu_read_action(highlight, entries, entry_count);
    if (action == ESCAPE)
    {
        if (escape_choice > 0)
            *highlight = escape_choice;
        ui_menu_clear();
        return escape_result;
    }

    for (i = 0; i < entry_count; i++)
    {
        if (action == entries[i].key)
        {
            ui_menu_clear();
            return i + 1;
        }
    }

    return 0;
}

/* Asks the active frontend to render the current semantic menu state. */
void ui_menu_render_current(void)
{
    if (ui_front_menu_render)
        ui_front_menu_render();
}

/* Enables one frontend-neutral semantic message-history viewer path. */
void ui_message_recall_set_semantic_enabled(bool enabled)
{
    ui_message_recall_semantic = enabled ? TRUE : FALSE;
}

/* Returns whether message recall should use the semantic viewer path. */
bool ui_message_recall_semantic_enabled(void)
{
    return ui_message_recall_semantic;
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

/* Publishes the menu's bottom summary text block. */
void ui_menu_set_summary(cptr text, const byte* attrs, int attrs_len)
{
    ui_model_set_buffer_text(ui_menu_summary, ui_menu_summary_attrs,
        &ui_menu_summary_attrs_len, sizeof(ui_menu_summary), text, attrs,
        attrs_len);
    ui_menu_touch();
}

/* Sets the preferred details-pane width in character cells. */
void ui_menu_set_details_width(int chars)
{
    ui_menu_details_width = (chars > 0) ? chars : 0;
    ui_menu_touch();
}

/* Sets the preferred details-pane height in text rows. */
void ui_menu_set_details_rows(int rows)
{
    ui_menu_details_rows = (rows > 0) ? rows : 0;
    ui_menu_touch();
}

/* Sets the preferred summary-block height in text rows. */
void ui_menu_set_summary_rows(int rows)
{
    ui_menu_summary_rows = (rows > 0) ? rows : 0;
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

/* Returns the current menu summary text buffer. */
const char* ui_menu_get_summary(void)
{
    return ui_menu_summary;
}

/* Returns the current menu summary text length. */
int ui_menu_get_summary_len(void)
{
    return (int)strlen(ui_menu_summary);
}

/* Returns the attrs paired with the menu summary buffer. */
const byte* ui_menu_get_summary_attrs(void)
{
    return ui_menu_summary_attrs;
}

/* Returns how many attrs are valid in the menu summary buffer. */
int ui_menu_get_summary_attrs_len(void)
{
    return ui_menu_summary_attrs_len;
}

/* Returns the requested details-pane width in characters. */
int ui_menu_get_details_width(void)
{
    return ui_menu_details_width;
}

/* Returns the requested details-pane height in rows. */
int ui_menu_get_details_rows(void)
{
    return ui_menu_details_rows;
}

/* Returns the requested summary-block height in rows. */
int ui_menu_get_summary_rows(void)
{
    return ui_menu_summary_rows;
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
    ui_modal_set_kind(
        text, attrs, attrs_len, dismiss_key, UI_MODAL_KIND_GENERIC);
}

/* Publishes one modal text block plus its dismiss key and semantic kind. */
void ui_modal_set_kind(cptr text, const byte* attrs, int attrs_len,
    int dismiss_key, ui_modal_kind kind)
{
    ui_model_set_buffer_text(ui_modal_text, ui_modal_attrs, &ui_modal_attrs_len,
        sizeof(ui_modal_text), text, attrs, attrs_len);
    ui_modal_dismiss_key = dismiss_key;
    ui_modal_kind_value = kind;
    ui_modal_touch();
}

/* Clears the exported modal state. */
void ui_modal_clear(void)
{
    ui_modal_text[0] = '\0';
    ui_modal_attrs_len = 0;
    ui_modal_dismiss_key = 0;
    ui_modal_kind_value = UI_MODAL_KIND_GENERIC;
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

/* Returns the semantic kind exported for the current modal. */
ui_modal_kind ui_modal_get_kind(void)
{
    return ui_modal_kind_value;
}

/* Returns the modal revision used by the frontend cache. */
unsigned int ui_modal_get_revision(void)
{
    return ui_modal_revision;
}

/* Plans the semantic kind and inline-more hint for the next top-line prompt. */
void ui_prompt_plan(ui_prompt_kind kind, bool has_more_hint)
{
    ui_prompt_pending_kind = kind;
    ui_prompt_pending_more_hint = has_more_hint ? TRUE : FALSE;
}

/* Publishes one prompt text buffer with a uniform terminal attribute. */
void ui_prompt_set_text(
    cptr text, byte attr, ui_prompt_kind kind, bool has_more_hint)
{
    ui_prompt_set_state(text, attr, kind, has_more_hint);
    ui_prompt_pending_kind = UI_PROMPT_KIND_NONE;
    ui_prompt_pending_more_hint = FALSE;
}

/* Writes one colored text run into the semantic top-line message buffer. */
void ui_prompt_putstr(int col, byte attr, cptr text)
{
    size_t len;
    size_t text_len;
    size_t i;

    if (!text || !text[0])
        return;

    if (col < 0)
        col = 0;

    if ((size_t)col >= sizeof(ui_prompt_text) - 1)
        return;

    if (col == 0)
    {
        ui_prompt_text[0] = '\0';
        ui_prompt_attrs_len = 0;
    }

    len = strlen(ui_prompt_text);
    if ((size_t)col > len)
    {
        size_t fill_end = (size_t)col;

        if (fill_end >= sizeof(ui_prompt_text))
            fill_end = sizeof(ui_prompt_text) - 1;

        for (i = len; i < fill_end; i++)
        {
            ui_prompt_text[i] = ' ';
            ui_prompt_attrs[i] = TERM_WHITE;
        }
        len = fill_end;
        ui_prompt_text[len] = '\0';
    }

    text_len = strlen(text);
    if ((size_t)col + text_len >= sizeof(ui_prompt_text))
        text_len = sizeof(ui_prompt_text) - (size_t)col - 1;

    for (i = 0; i < text_len; i++)
    {
        ui_prompt_text[col + (int)i] = text[i];
        ui_prompt_attrs[col + (int)i] = attr;
    }

    len = strlen(ui_prompt_text);
    if ((size_t)col + text_len > len)
        len = (size_t)col + text_len;
    ui_prompt_text[len] = '\0';
    ui_prompt_attrs_len = (int)len;
    ui_prompt_kind_value = UI_PROMPT_KIND_MESSAGE;
    ui_prompt_more_hint = FALSE;
    ui_prompt_pending_kind = UI_PROMPT_KIND_NONE;
    ui_prompt_pending_more_hint = FALSE;
    ui_prompt_touch();
    ui_prompt_render_current_frontend(0, 0);
}

/* Clears the current semantic prompt state. */
void ui_prompt_clear(void)
{
    ui_prompt_text[0] = '\0';
    ui_prompt_attrs_len = 0;
    ui_prompt_kind_value = UI_PROMPT_KIND_NONE;
    ui_prompt_more_hint = FALSE;
    ui_prompt_pending_kind = UI_PROMPT_KIND_NONE;
    ui_prompt_pending_more_hint = FALSE;
    ui_prompt_touch();
}

/* Publishes and optionally renders one semantic top-line prompt. */
bool ui_prompt_render(int row, int col, byte attr, cptr text)
{
    cptr prompt_text = text ? text : "";

    if ((row != 0) || (col != 0))
        return FALSE;

    ui_prompt_set_text(prompt_text, attr, ui_prompt_pending_kind,
        ui_prompt_pending_more_hint);

    ui_prompt_render_current_frontend(row, col);

    return TRUE;
}

/* Publishes and optionally renders one semantic "-more-" prompt. */
void ui_prompt_show_more(int col, byte attr)
{
    (void)attr;

    ui_prompt_kind_value = UI_PROMPT_KIND_MORE;
    ui_prompt_more_hint = FALSE;
    ui_prompt_pending_kind = UI_PROMPT_KIND_NONE;
    ui_prompt_pending_more_hint = FALSE;
    ui_prompt_touch();
    ui_prompt_render_current_frontend(0, col);
}

/* Clears the semantic prompt state and any frontend-specific prompt line. */
void ui_prompt_clear_line(void)
{
    ui_prompt_clear();

    if (ui_front_prompt_clear)
        ui_front_prompt_clear();
}

/* Returns the current semantic prompt kind. */
ui_prompt_kind ui_prompt_get_kind(void)
{
    return ui_prompt_kind_value;
}

/* Returns whether the current prompt has extra inline detail to cycle. */
bool ui_prompt_get_more_hint(void)
{
    return ui_prompt_more_hint;
}

/* Returns the current semantic prompt text buffer. */
const char* ui_prompt_get_text(void)
{
    return ui_prompt_text;
}

/* Returns the current semantic prompt text length. */
int ui_prompt_get_text_len(void)
{
    return (int)strlen(ui_prompt_text);
}

/* Returns the attrs paired with the semantic prompt text. */
const byte* ui_prompt_get_attrs(void)
{
    return ui_prompt_attrs;
}

/* Returns how many attrs are valid in the semantic prompt text buffer. */
int ui_prompt_get_attrs_len(void)
{
    return ui_prompt_attrs_len;
}

/* Returns the prompt revision used by the frontend cache. */
unsigned int ui_prompt_get_revision(void)
{
    return ui_prompt_revision;
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
