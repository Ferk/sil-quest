/* File: ui-term.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Terminal-frontend rendering hooks for shared semantic UI state.
 */

#include "angband.h"

#include "ui-birth.h"
#include "ui-character.h"
#include "ui-input.h"
#include "ui-model.h"
#include "ui-term.h"

#define UI_TERM_MENU_MAX_COLUMNS 8
#define UI_TERM_MENU_MIN_LIST_GAP 4
#define UI_TERM_MENU_MIN_SECOND_COL 24
#define UI_TERM_MENU_MIN_DETAILS_COL 42
#define UI_TERM_LOCAL_MENU_MAX_ITEMS 16
#define UI_TERM_LOCAL_MENU_MAX_WIDTH 52
#define UI_TERM_LOCAL_MENU_FRAME_PAD_X 2
#define UI_TERM_LOCAL_MENU_FRAME_PAD_Y 1
#define UI_TERM_LOCAL_MENU_FRAME_EXTRA_W \
    (2 * UI_TERM_LOCAL_MENU_FRAME_PAD_X)
#define UI_TERM_LOCAL_MENU_FRAME_EXTRA_H \
    (2 * UI_TERM_LOCAL_MENU_FRAME_PAD_Y)
#define UI_TERM_BIRTH_STATS_COL 42
#define UI_TERM_BIRTH_STATS_ROW 2
#define UI_TERM_BIRTH_COST_COL (UI_TERM_BIRTH_STATS_COL + 32)
#define UI_TERM_BIRTH_POINTS_COL (UI_TERM_BIRTH_STATS_COL + 21)
#define UI_TERM_BIRTH_HELP_COL 2
#define UI_TERM_BIRTH_HELP_ROW 22
#define UI_TERM_BIRTH_PROMPT_COL 2
#define UI_TERM_BIRTH_INSTRUCT_ROW 21
#define UI_TERM_SKILL_EDITOR_ROW 7
#define UI_TERM_SKILL_EDITOR_SELECT_COL 40
#define UI_TERM_SKILL_EDITOR_COST_COL 72
#define UI_TERM_SKILL_EDITOR_POINTS_ROW 5
#define UI_TERM_SKILL_EDITOR_POINTS_COL 59
#define UI_TERM_SKILL_EDITOR_HELP_COL 2
#define UI_TERM_SKILL_EDITOR_HELP_ROW 22

/* Returns the width available for one centered local modal within the map area. */
static int ui_term_local_menu_max_width(int term_wid)
{
    int available_wid = term_wid - COL_MAP - UI_TERM_LOCAL_MENU_FRAME_EXTRA_W;

    if (available_wid < 1)
        available_wid = term_wid - UI_TERM_LOCAL_MENU_FRAME_EXTRA_W;

    if (available_wid < 1)
        return term_wid;

    return available_wid;
}

static bool ui_term_local_menu_valid = FALSE;
static int ui_term_local_menu_left = 0;
static int ui_term_local_menu_top = 0;
static int ui_term_local_menu_right = 0;
static int ui_term_local_menu_bottom = 0;

/* Returns whether one terminal write targets the shared top-line prompt slot. */
static bool ui_term_is_prompt_location(int row, int col)
{
    return (row == 0) && (col == 0);
}

/* Draws the semantic prompt buffer with its exported per-character attrs. */
static int ui_term_draw_prompt_text(int row)
{
    cptr prompt_text = ui_prompt_get_text();
    const byte* prompt_attrs = ui_prompt_get_attrs();
    int prompt_attrs_len = ui_prompt_get_attrs_len();
    int last = -1;
    int x;

    if (!prompt_text)
        return -1;

    for (x = 0; prompt_text[x] != '\0'; x++)
    {
        byte attr = (x < prompt_attrs_len) ? prompt_attrs[x] : TERM_WHITE;

        Term_putch(x, row, attr, prompt_text[x]);
        if (!isspace((unsigned char)prompt_text[x]))
            last = x;
    }

    return last;
}

/* Renders one semantic top-line prompt on the terminal frontend. */
static bool ui_term_prompt_render(int row, int col)
{
    int last_nonblank;
    ui_prompt_kind kind;
    bool has_more_hint;

    if (!ui_term_is_prompt_location(row, 0))
        return FALSE;

    kind = ui_prompt_get_kind();
    has_more_hint = ui_prompt_get_more_hint();

    Term_erase(0, row, 255);
    last_nonblank = ui_term_draw_prompt_text(row);

    if (kind == UI_PROMPT_KIND_MORE)
    {
        int prompt_col = col;

        if (prompt_col <= last_nonblank)
            prompt_col = last_nonblank + 1;
        if (prompt_col < 0)
            prompt_col = 0;

        Term_putstr(prompt_col, row, -1, TERM_L_BLUE, "-more-");
    }
    else if (has_more_hint)
    {
        cptr suffix = (last_nonblank >= 0) ? " -more-" : "-more-";
        int prompt_col = (last_nonblank >= 0) ? last_nonblank + 1 : 0;

        Term_putstr(prompt_col, row, -1, TERM_L_BLUE, suffix);
    }

    return TRUE;
}

/* Clears the terminal frontend's top-line prompt and semantic state. */
static void ui_term_prompt_clear(void)
{
    Term_erase(0, 0, 255);
}

/* Erases one bounded terminal rectangle. */
static void ui_term_erase_rect(int left, int top, int right, int bottom)
{
    int term_wid = 80;
    int term_hgt = 24;
    int row;

    (void)Term_get_size(&term_wid, &term_hgt);

    if (left < 0)
        left = 0;
    if (top < 0)
        top = 0;
    if (right >= term_wid)
        right = term_wid - 1;
    if (bottom >= term_hgt)
        bottom = term_hgt - 1;

    if ((left > right) || (top > bottom))
        return;

    for (row = top; row <= bottom; row++)
        Term_erase(left, row, right - left + 1);
}

/* Clears the last compact terminal menu panel before redrawing it elsewhere. */
static void ui_term_clear_previous_local_menu(void)
{
    if (!ui_term_local_menu_valid)
        return;

    ui_term_erase_rect(ui_term_local_menu_left, ui_term_local_menu_top,
        ui_term_local_menu_right, ui_term_local_menu_bottom);
    ui_term_local_menu_valid = FALSE;
}

/* Centers one compact modal panel within the map area when there is room. */
static void ui_term_local_menu_origin(int panel_width, int panel_height,
    int term_wid, int term_hgt, int* origin_x, int* origin_y)
{
    int available_left = COL_MAP;
    int available_top = ROW_MAP;
    int available_wid = term_wid - available_left;
    int available_hgt = term_hgt - available_top - 1;
    int x = 0;
    int y = 0;

    if (available_wid < 1)
        available_wid = term_wid;
    if (available_hgt < 1)
        available_hgt = term_hgt;

    if (panel_width <= available_wid)
        x = available_left + (available_wid - panel_width) / 2;
    else
        x = (term_wid - panel_width) / 2;

    if (panel_height <= available_hgt)
        y = available_top + (available_hgt - panel_height) / 2;
    else
        y = (term_hgt - panel_height) / 2;

    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;

    if (origin_x)
        *origin_x = x;
    if (origin_y)
        *origin_y = y;
}

/* Returns whether one semantic menu should stay as a local modal panel. */
static bool ui_term_menu_is_local_modal(int column_count, int item_count,
    int panel_right, int panel_bottom, int term_wid, int term_hgt)
{
    (void)panel_right;
    (void)panel_bottom;
    (void)term_wid;
    (void)term_hgt;

    if (column_count > 1)
        return FALSE;

    if (item_count > UI_TERM_LOCAL_MENU_MAX_ITEMS)
        return FALSE;

    return TRUE;
}

/* Returns a darker terminal attr for one inactive semantic menu item. */
static byte ui_term_dim_menu_attr(byte attr)
{
    int base = GET_BASE_COLOR(attr);
    int shade = GET_SHADE(attr);

    switch (base)
    {
    case TERM_L_WHITE:
        return MAKE_EXTENDED_COLOR(TERM_WHITE, shade);

    case TERM_WHITE:
        return MAKE_EXTENDED_COLOR(TERM_SLATE, shade);

    case TERM_SLATE:
        return MAKE_EXTENDED_COLOR(TERM_L_DARK, shade);

    case TERM_L_DARK:
        return MAKE_EXTENDED_COLOR(TERM_DARK, shade);

    case TERM_L_BLUE:
        return MAKE_EXTENDED_COLOR(TERM_BLUE, shade);

    case TERM_L_GREEN:
        return MAKE_EXTENDED_COLOR(TERM_GREEN, shade);

    case TERM_L_RED:
        return MAKE_EXTENDED_COLOR(TERM_RED, shade);

    case TERM_L_UMBER:
        return MAKE_EXTENDED_COLOR(TERM_UMBER, shade);

    case TERM_ORANGE:
    case TERM_YELLOW:
        return MAKE_EXTENDED_COLOR(TERM_UMBER, shade);

    default:
        if (shade < MAX_SHADES - 1)
            shade++;
        return MAKE_EXTENDED_COLOR(base, shade);
    }
}

/* Returns whether one semantic menu item belongs to the active menu column. */
static bool ui_term_menu_item_is_active(
    const ui_menu_item* item, int active_column)
{
    if (!item || (active_column < 0))
        return TRUE;

    return item->x == active_column;
}

/* Returns the terminal attr that should be used for one semantic menu item. */
static byte ui_term_menu_item_attr(const ui_menu_item* item, int active_column)
{
    byte attr;

    if (!item)
        return TERM_WHITE;

    if (!item->selected)
        attr = (byte)item->attr;
    else if (item->attr == TERM_SLATE)
        attr = TERM_BLUE;
    else
        attr = TERM_L_BLUE;

    if (!ui_term_menu_item_is_active(item, active_column))
        attr = ui_term_dim_menu_attr(attr);

    return attr;
}

/* Chooses the semantic menu item that should own the terminal cursor. */
static int ui_term_menu_cursor_index(
    const ui_menu_item* items, int count, int active_column)
{
    int selected = ui_menu_get_selected_index();
    int i;

    if (!items || (count <= 0) || (active_column < 0))
        return selected;

    if ((selected >= 0) && (selected < count)
        && ui_term_menu_item_is_active(&items[selected], active_column))
    {
        return selected;
    }

    for (i = 0; i < count; i++)
    {
        if (items[i].selected && ui_term_menu_item_is_active(&items[i], active_column))
            return i;
    }

    for (i = 0; i < count; i++)
    {
        if (ui_term_menu_item_is_active(&items[i], active_column))
            return i;
    }

    return selected;
}

/* Finds the next wrapped span inside one logical text line. */
static int ui_term_wrap_span_end(
    cptr text, int start, int end, int width, int* next_start)
{
    int i;
    int hard_end;
    int wrap_end;

    if (next_start)
        *next_start = end;

    if (!text || (start >= end) || (width <= 0))
        return start;

    hard_end = start + width;
    if (hard_end >= end)
    {
        if (next_start)
            *next_start = end;
        return end;
    }

    wrap_end = hard_end;
    for (i = hard_end; i > start; i--)
    {
        if (isspace((unsigned char)text[i - 1]))
        {
            wrap_end = i - 1;
            break;
        }
    }

    if (wrap_end <= start)
        wrap_end = hard_end;

    if (next_start)
    {
        int next = wrap_end;

        while ((next < end) && isspace((unsigned char)text[next]))
            next++;
        *next_start = next;
    }

    return wrap_end;
}

/* Draws one attributed multiline text block with terminal-side word wrapping. */
static void ui_term_draw_wrapped_text_block(
    int col, int row, cptr text, const byte* attrs, int attr_len, int width)
{
    int start = 0;
    int i;

    if (!text || !text[0] || (width <= 0))
        return;

    while (text[start] != '\0')
    {
        int end = start;

        while ((text[end] != '\0') && (text[end] != '\n'))
            end++;

        if (start == end)
        {
            row++;
            if (text[end] == '\n')
                start = end + 1;
            else
                start = end;
            continue;
        }

        while (start < end)
        {
            int next_start = end;
            int span_end =
                ui_term_wrap_span_end(text, start, end, width, &next_start);
            int x = col;

            for (i = start; i < span_end; i++)
            {
                Term_putch(x++, row, (i < attr_len) ? attrs[i] : TERM_WHITE, text[i]);
            }

            row++;
            if (next_start <= start)
                break;
            start = next_start;
        }

        if (text[end] == '\n')
            start = end + 1;
        else
            start = end;
    }
}

/* Counts wrapped terminal rows for one multiline text block. */
static int ui_term_wrapped_text_line_count(cptr text, int width)
{
    int lines = 0;
    int start = 0;

    if (!text || !text[0] || (width <= 0))
        return 0;

    while (text[start] != '\0')
    {
        int end = start;

        while ((text[end] != '\0') && (text[end] != '\n'))
            end++;

        if (start == end)
        {
            lines++;
            if (text[end] == '\n')
                start = end + 1;
            else
                start = end;
            continue;
        }

        while (start < end)
        {
            int next_start = end;

            ui_term_wrap_span_end(text, start, end, width, &next_start);
            lines++;
            if (next_start <= start)
                break;
            start = next_start;
        }

        if (text[end] == '\n')
            start = end + 1;
        else
            start = end;
    }

    return lines;
}

/* Collects the semantic x columns currently exported by the shared menu. */
static int ui_term_collect_menu_columns(
    const ui_menu_item* items, int count, int* columns, int* widths)
{
    int column_count = 0;
    int i;

    for (i = 0; i < count; i++)
    {
        int j;
        int width = items[i].w;

        if (width <= 0)
            width = (int)strlen(items[i].label);

        for (j = 0; j < column_count; j++)
        {
            if (columns[j] == items[i].x)
            {
                if (width > widths[j])
                    widths[j] = width;
                break;
            }
        }

        if (j < column_count)
            continue;

        if (column_count >= UI_TERM_MENU_MAX_COLUMNS)
            continue;

        columns[column_count] = items[i].x;
        widths[column_count] = width;
        column_count++;
    }

    return column_count;
}

/* Returns the collected column index for one semantic menu x value. */
static int ui_term_find_menu_column(
    const int* columns, int column_count, int semantic_x)
{
    int i;

    for (i = 0; i < column_count; i++)
    {
        if (columns[i] == semantic_x)
            return i;
    }

    return 0;
}

/* Returns whether one semantic menu item is a direct action rather than a list row. */
static bool ui_term_menu_item_is_direct(const ui_menu_item* item)
{
    return item && (item->key > 0) && (item->key != '\r');
}

/* Returns whether the current semantic menu uses the tabbed layout. */
static bool ui_term_menu_is_tabbed(void)
{
    return ui_menu_get_layout_kind() == UI_MENU_LAYOUT_TABBED;
}

/* Lays out the tab buttons for one tabbed semantic menu. */
static int ui_term_layout_menu_tabs(const ui_menu_item* items, int count,
    int* tab_indices, int* tab_positions)
{
    int direct_indices[UI_TERM_MENU_MAX_COLUMNS];
    int direct_count = 0;
    int i;
    int next_col = 0;

    for (i = 0; i < count; i++)
    {
        if (!ui_term_menu_item_is_direct(&items[i]))
            continue;

        if (direct_count >= UI_TERM_MENU_MAX_COLUMNS)
            break;

        direct_indices[direct_count++] = i;
    }

    if (direct_count <= 0)
        return 0;

    for (i = 0; i < direct_count - 1; i++)
    {
        int j;

        for (j = i + 1; j < direct_count; j++)
        {
            if (items[direct_indices[j]].x < items[direct_indices[i]].x)
            {
                int swap = direct_indices[i];
                direct_indices[i] = direct_indices[j];
                direct_indices[j] = swap;
            }
        }
    }

    for (i = 0; i < direct_count; i++)
    {
        int width = items[direct_indices[i]].w;

        if (width <= 0)
            width = (int)strlen(items[direct_indices[i]].label);

        tab_indices[i] = direct_indices[i];
        tab_positions[i] = next_col;
        next_col += width + UI_TERM_MENU_MIN_LIST_GAP;
    }

    return next_col - UI_TERM_MENU_MIN_LIST_GAP;
}

/* Chooses terminal x positions for the current semantic menu columns. */
static void ui_term_layout_menu_columns(
    const int* widths, int column_count, int* positions)
{
    int i;
    int next_col = 0;

    if (column_count <= 0)
        return;

    positions[0] = 0;
    next_col = widths[0] + UI_TERM_MENU_MIN_LIST_GAP;
    if ((column_count > 1) && (next_col < UI_TERM_MENU_MIN_SECOND_COL))
        next_col = UI_TERM_MENU_MIN_SECOND_COL;

    for (i = 1; i < column_count; i++)
    {
        positions[i] = next_col;
        next_col += widths[i] + UI_TERM_MENU_MIN_LIST_GAP;
    }
}

/* Draws the details-pane visual preview for the current semantic menu. */
static int ui_term_draw_menu_details_visual(int col, int row)
{
    int kind = ui_menu_get_details_visual_kind();
    int attr = ui_menu_get_details_visual_attr();
    int chr = ui_menu_get_details_visual_char();

    if (kind == UI_MENU_VISUAL_NONE)
        return row;

    Term_putch(col, row, attr, chr);
    if (use_bigtile)
    {
        if (((byte)attr) & 0x80)
            Term_putch(col + 1, row, 255, -1);
        else
            Term_putch(col + 1, row, 0, ' ');
    }

    return row + 2;
}

/* Draws the current semantic menu state on the terminal frontend. */
static void ui_term_menu_render(void)
{
    const ui_menu_item* items = ui_menu_get_items();
    int count = ui_menu_get_item_count();
    int active_column = ui_menu_get_active_column();
    cptr menu_text = ui_menu_get_text();
    cptr menu_details = ui_menu_get_details();
    cptr menu_summary = ui_menu_get_summary();
    int columns[UI_TERM_MENU_MAX_COLUMNS];
    int widths[UI_TERM_MENU_MAX_COLUMNS];
    int positions[UI_TERM_MENU_MAX_COLUMNS];
    int column_count;
    int term_wid = 80;
    int term_hgt = 24;
    int text_width;
    int text_lines;
    int details_lines;
    int details_rows;
    int details_width;
    int details_available_width;
    int summary_lines;
    int summary_rows;
    int summary_width;
    int min_row = 0;
    int max_row = 0;
    int row_shift = 0;
    int i;
    int selected;
    int details_col = UI_TERM_MENU_MIN_DETAILS_COL;
    int details_row = 0;
    int summary_row = 0;
    int details_visual_height = 0;
    int details_bottom = -1;
    int menu_right = 0;
    int panel_right = 0;
    int panel_bottom = 0;
    int panel_origin_x = 0;
    int panel_origin_y = 0;
    int frame_left = 0;
    int frame_top = 0;
    int frame_right = 0;
    int frame_bottom = 0;
    int tab_indices[UI_TERM_MENU_MAX_COLUMNS];
    int tab_positions[UI_TERM_MENU_MAX_COLUMNS];
    int tab_count = 0;
    int tabbed_menu = FALSE;
    bool local_modal = FALSE;
    bool has_details_pane = FALSE;
    bool has_summary = FALSE;
    bool details_side = FALSE;
    int local_modal_max_width;
    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;

    has_details_pane =
        ((ui_menu_get_details_visual_kind() != UI_MENU_VISUAL_NONE)
            || (menu_details && menu_details[0] != '\0'));
    has_summary = menu_summary && (menu_summary[0] != '\0');
    local_modal_max_width = ui_term_local_menu_max_width(term_wid);

    selected = ui_term_menu_cursor_index(items, count, active_column);

    tabbed_menu = ui_term_menu_is_tabbed();

    if (tabbed_menu)
    {
        int active_width = 0;

        for (i = 0; i < count; i++)
        {
            int width;

            if (ui_term_menu_item_is_direct(&items[i]))
                continue;

            width = items[i].w;
            if (width <= 0)
                width = (int)strlen(items[i].label);
            if (width > active_width)
                active_width = width;
        }

        tab_count = ui_term_layout_menu_tabs(
            items, count, tab_indices, tab_positions);
        column_count = 1;
        columns[0] = active_column;
        widths[0] = active_width;
        positions[0] = 0;
        menu_right = active_width;
        if (tab_count > menu_right)
            menu_right = tab_count;
    }
    else
    {
        column_count = ui_term_collect_menu_columns(items, count, columns, widths);
        ui_term_layout_menu_columns(widths, column_count, positions);

        if (column_count > 0)
            menu_right = positions[column_count - 1] + widths[column_count - 1];
    }

    local_modal = ui_term_menu_is_local_modal(
        column_count, count, menu_right, 0, term_wid, term_hgt);

    text_width = term_wid;
    if (local_modal)
    {
        text_width = UI_TERM_LOCAL_MENU_MAX_WIDTH;
        if (text_width > local_modal_max_width)
            text_width = local_modal_max_width;
        if (text_width > term_wid)
            text_width = term_wid;
        if (menu_right + 1 > text_width)
            text_width = menu_right + 1;
    }

    text_lines = ui_term_wrapped_text_line_count(menu_text, text_width);

    if (count > 0)
    {
        min_row = items[0].y;
        max_row = items[0].y;
        for (i = 1; i < count; i++)
        {
            if (items[i].y < min_row)
                min_row = items[i].y;
            if (items[i].y > max_row)
                max_row = items[i].y;
        }

        if (min_row < text_lines)
            row_shift = text_lines - min_row;
    }

    if (has_details_pane && !local_modal && (column_count > 0))
    {
        details_col = menu_right + UI_TERM_MENU_MIN_LIST_GAP;
        if (details_col < UI_TERM_MENU_MIN_DETAILS_COL)
            details_col = UI_TERM_MENU_MIN_DETAILS_COL;
    }
    else if (local_modal)
    {
        details_col = 0;
    }

    details_available_width = local_modal ? text_width : (term_wid - details_col);
    if (!local_modal && has_details_pane && (details_available_width < 12))
    {
        details_col = term_wid - 12;
        if (details_col < 0)
            details_col = 0;
        details_available_width = term_wid - details_col;
    }

    if (details_available_width <= 0)
        details_available_width = term_wid;

    details_width = ui_menu_get_details_width();
    if ((details_width <= 0) || (details_width > details_available_width))
        details_width = details_available_width;

    details_visual_height =
        (ui_menu_get_details_visual_kind() == UI_MENU_VISUAL_NONE) ? 0 : 2;
    details_lines = ui_term_wrapped_text_line_count(menu_details, details_width);
    details_rows = ui_menu_get_details_rows();
    summary_width = local_modal ? text_width : (panel_right + 1);
    summary_lines = 0;
    summary_rows = ui_menu_get_summary_rows();

    if (has_details_pane && local_modal)
    {
        int desired_right = menu_right + UI_TERM_MENU_MIN_LIST_GAP + details_width;

        if (desired_right + 1 <= local_modal_max_width)
        {
            details_side = TRUE;
            details_col = menu_right + UI_TERM_MENU_MIN_LIST_GAP;
            text_width = desired_right + 1;
        }
    }
    else if (has_details_pane)
    {
        details_side = TRUE;
    }

    panel_right = local_modal ? (text_width - 1) : menu_right;
    if (details_side && has_details_pane)
    {
        int details_right = details_col + details_width - 1;

        if (details_right > panel_right)
            panel_right = details_right;
    }

    panel_bottom = text_lines - 1;
    if ((count > 0) && (max_row + row_shift > panel_bottom))
        panel_bottom = max_row + row_shift;

    details_row = details_side
        ? ((count > 0) ? (min_row + row_shift) : text_lines)
        : ((count > 0) ? (max_row + row_shift + 2) : text_lines);
    if (has_details_pane && ((details_visual_height > 0) || (details_lines > 0)))
    {
        int details_height = details_visual_height + details_lines;

        if ((details_rows > 0) && (details_height < details_rows))
            details_height = details_rows;

        details_bottom = details_row + details_height - 1;

        if (details_bottom > panel_bottom)
            panel_bottom = details_bottom;
    }

    summary_width = panel_right + 1;
    summary_lines = ui_term_wrapped_text_line_count(menu_summary, summary_width);
    if ((summary_rows > 0) && (summary_lines < summary_rows))
        summary_lines = summary_rows;
    if (has_summary && (summary_lines > 0))
    {
        int content_bottom = panel_bottom;

        if ((details_bottom > content_bottom))
            content_bottom = details_bottom;

        summary_row = content_bottom + 2;
        panel_bottom = summary_row + summary_lines - 1;
    }

    if (local_modal)
    {
        ui_term_local_menu_origin(panel_right + 1 + UI_TERM_LOCAL_MENU_FRAME_EXTRA_W,
            panel_bottom + 1 + UI_TERM_LOCAL_MENU_FRAME_EXTRA_H,
            term_wid, term_hgt, &frame_left, &frame_top);
        frame_right = frame_left + panel_right + UI_TERM_LOCAL_MENU_FRAME_EXTRA_W;
        frame_bottom = frame_top + panel_bottom + UI_TERM_LOCAL_MENU_FRAME_EXTRA_H;
        panel_origin_x = frame_left + UI_TERM_LOCAL_MENU_FRAME_PAD_X;
        panel_origin_y = frame_top + UI_TERM_LOCAL_MENU_FRAME_PAD_Y;
    }

    if (local_modal)
    {
        ui_term_clear_previous_local_menu();
        ui_term_erase_rect(frame_left, frame_top, frame_right, frame_bottom);
        ui_term_local_menu_left = frame_left;
        ui_term_local_menu_top = frame_top;
        ui_term_local_menu_right = frame_right;
        ui_term_local_menu_bottom = frame_bottom;
        ui_term_local_menu_valid = TRUE;
    }
    else
    {
        ui_term_clear_previous_local_menu();
        Term_clear();
    }

    ui_term_draw_wrapped_text_block(
        panel_origin_x, panel_origin_y, menu_text, ui_menu_get_attrs(),
        ui_menu_get_attrs_len(), text_width);

    for (i = 0; i < count; i++)
    {
        int col = panel_origin_x;
        int row = panel_origin_y + items[i].y + row_shift;

        if (tabbed_menu && ui_term_menu_item_is_direct(&items[i]))
        {
            int tab_slot;

            for (tab_slot = 0; tab_slot < 2; tab_slot++)
            {
                if (tab_indices[tab_slot] == i)
                {
                    col += tab_positions[tab_slot];
                    break;
                }
            }
        }
        else if (!tabbed_menu)
        {
            int column_index =
                ui_term_find_menu_column(columns, column_count, items[i].x);

            col += positions[column_index];
        }

        Term_putstr(col, row, -1,
            ui_term_menu_item_attr(&items[i], active_column), items[i].label);
    }

    if (has_details_pane)
    {
        details_row = ui_term_draw_menu_details_visual(
            panel_origin_x + details_col, panel_origin_y + details_row);
        ui_term_draw_wrapped_text_block(panel_origin_x + details_col, details_row,
            menu_details,
            ui_menu_get_details_attrs(), ui_menu_get_details_attrs_len(),
            details_width);
    }

    if (has_summary && (summary_lines > 0))
    {
        ui_term_draw_wrapped_text_block(panel_origin_x, panel_origin_y + summary_row,
            menu_summary, ui_menu_get_summary_attrs(),
            ui_menu_get_summary_attrs_len(), summary_width);
    }

    Term_fresh();

    if ((selected >= 0) && (selected < count))
    {
        int cursor_col = panel_origin_x;

        if (tabbed_menu && ui_term_menu_item_is_direct(&items[selected]))
        {
            int tab_slot;

            for (tab_slot = 0; tab_slot < 2; tab_slot++)
            {
                if (tab_indices[tab_slot] == selected)
                {
                    cursor_col += tab_positions[tab_slot];
                    break;
                }
            }
        }
        else if (!tabbed_menu)
        {
            int column_index =
                ui_term_find_menu_column(columns, column_count, items[selected].x);

            cursor_col += positions[column_index];
        }

        Term_gotoxy(cursor_col, panel_origin_y + items[selected].y + row_shift);
    }
}

/* Draws the shared birth stat-allocation screen on top of the character sheet. */
static void ui_term_birth_stats_render(void)
{
    struct ui_birth_state_snapshot snapshot;
    int i;
    char buf[32];

    ui_birth_get_state_snapshot(&snapshot);

    if (!snapshot.active || (snapshot.kind != UI_BIRTH_SCREEN_STATS))
        return;

    Term_erase(UI_TERM_BIRTH_POINTS_COL, 0, 18);
    c_put_str(TERM_WHITE, "Points Left:", 0, UI_TERM_BIRTH_POINTS_COL);
    strnfmt(buf, sizeof(buf), "%2d", snapshot.points_left);
    c_put_str(TERM_L_GREEN, buf, 0, UI_TERM_BIRTH_POINTS_COL + 13);

    for (i = 0; i < snapshot.stats_count; i++)
    {
        bool selected = snapshot.stats[i].selected ? TRUE : FALSE;
        byte attr = selected ? TERM_L_BLUE : TERM_L_WHITE;

        Term_erase(UI_TERM_BIRTH_STATS_COL - 3, UI_TERM_BIRTH_STATS_ROW + i, 1);
        if (selected)
            c_put_str(attr, ">", UI_TERM_BIRTH_STATS_ROW + i,
                UI_TERM_BIRTH_STATS_COL - 3);

        Term_erase(UI_TERM_BIRTH_COST_COL, UI_TERM_BIRTH_STATS_ROW + i, 8);
        strnfmt(buf, sizeof(buf), "%4d", snapshot.stats[i].cost);
        c_put_str(attr, buf, UI_TERM_BIRTH_STATS_ROW + i, UI_TERM_BIRTH_COST_COL);
    }

    Term_erase(0, UI_TERM_BIRTH_HELP_ROW, 255);
    Term_erase(0, UI_TERM_BIRTH_HELP_ROW + 1, 255);
    c_put_str(TERM_SLATE, "Arrow keys move, left/right spend points",
        UI_TERM_BIRTH_HELP_ROW, UI_TERM_BIRTH_HELP_COL);
    c_put_str(TERM_SLATE, "Enter accepts current values, Escape goes back",
        UI_TERM_BIRTH_HELP_ROW + 1, UI_TERM_BIRTH_HELP_COL);
    c_put_str(TERM_L_WHITE, "Arrow keys", UI_TERM_BIRTH_HELP_ROW,
        UI_TERM_BIRTH_HELP_COL);
    c_put_str(TERM_L_WHITE, "left/right", UI_TERM_BIRTH_HELP_ROW,
        UI_TERM_BIRTH_HELP_COL + 17);
    c_put_str(TERM_L_WHITE, "Enter", UI_TERM_BIRTH_HELP_ROW + 1,
        UI_TERM_BIRTH_HELP_COL);
    c_put_str(TERM_L_WHITE, "Escape", UI_TERM_BIRTH_HELP_ROW + 1,
        UI_TERM_BIRTH_HELP_COL + 29);
}

/* Runs the terminal-native history prompt for one shared working draft. */
static ui_birth_history_prompt_result ui_term_birth_history_prompt(
    char* history, size_t size)
{
    int i;
    char line[70];

    while (TRUE)
    {
        char query2;

        display_player(0);
        display_player_xtra_info(2);

        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
            TERM_SLATE, "Enter accept history");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
            TERM_SLATE, "Space reroll history");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 3, -1,
            TERM_SLATE, "    m manually enter history");

        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
            TERM_L_WHITE, "Enter");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
            TERM_L_WHITE, "Space");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL + 4, UI_TERM_BIRTH_INSTRUCT_ROW + 3,
            -1, TERM_L_WHITE, "m");

        Term_gotoxy(0, UI_TERM_BIRTH_INSTRUCT_ROW + 1);
        query2 = inkey();

        if (ui_input_is_accept_key(query2))
            return UI_BIRTH_HISTORY_PROMPT_ACCEPT;

        if (query2 == ' ')
            return UI_BIRTH_HISTORY_PROMPT_REROLL;

        if ((query2 == 'm') || (query2 == 'M'))
        {
            history[0] = '\0';

            display_player(0);

            for (i = 1; i <= 3; i++)
            {
                line[0] = '\0';
                Term_gotoxy(1, 15 + i);

                if (askfor_aux(line, sizeof(line)))
                {
                    my_strcat(history, line, size);
                    my_strcat(history, "\n", size);
                    p_ptr->redraw |= PR_MISC;
                    display_player(0);
                }
                else
                {
                    return UI_BIRTH_HISTORY_PROMPT_CANCEL;
                }
            }

            continue;
        }

        if (ui_input_is_cancel_key(query2))
            return UI_BIRTH_HISTORY_PROMPT_CANCEL;

        if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }
}

/* Runs the terminal-native age/height/weight prompt for one working draft. */
static ui_birth_ahw_prompt_result ui_term_birth_ahw_prompt(int* age,
    int* height, int* weight, int age_min, int age_max, int height_min,
    int height_max, int weight_min, int weight_max)
{
    char prompt[50];
    char line[70];

    if (!age || !height || !weight)
        return UI_BIRTH_AHW_PROMPT_CANCEL;

    while (TRUE)
    {
        char query2;

        p_ptr->age = *age;
        p_ptr->ht = *height;
        p_ptr->wt = *weight;

        display_player(0);
        display_player_xtra_info(1);

        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
            TERM_SLATE, "Enter accept age/height/weight");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
            TERM_SLATE, "Space reroll");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 3, -1,
            TERM_SLATE, "    m manually enter");

        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
            TERM_L_WHITE, "Enter");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
            TERM_L_WHITE, "Space");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL + 4, UI_TERM_BIRTH_INSTRUCT_ROW + 3,
            -1, TERM_L_WHITE, "m");

        Term_gotoxy(0, UI_TERM_BIRTH_INSTRUCT_ROW + 1);
        query2 = inkey();

        if (ui_input_is_accept_key(query2))
            return UI_BIRTH_AHW_PROMPT_ACCEPT;

        if (query2 == ' ')
            return UI_BIRTH_AHW_PROMPT_REROLL;

        if ((query2 == 'm') || (query2 == 'M'))
        {
            line[0] = '\0';

            while ((*age < age_min) || (*age > age_max))
            {
                strnfmt(prompt, sizeof(prompt), "Enter age (%d-%d): ",
                    age_min, age_max);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return UI_BIRTH_AHW_PROMPT_CANCEL;
                *age = atoi(line);
                p_ptr->age = *age;
            }

            p_ptr->redraw |= PR_MISC;
            display_player(0);
            line[0] = '\0';

            while ((*height < height_min) || (*height > height_max))
            {
                strnfmt(prompt, sizeof(prompt),
                    "Enter height in inches (%d-%d): ", height_min,
                    height_max);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return UI_BIRTH_AHW_PROMPT_CANCEL;
                *height = atoi(line);
                p_ptr->ht = *height;
            }

            p_ptr->redraw |= PR_MISC;
            display_player(0);
            line[0] = '\0';

            while ((*weight < weight_min) || (*weight > weight_max))
            {
                strnfmt(prompt, sizeof(prompt),
                    "Enter weight in pounds (%d-%d): ", weight_min,
                    weight_max);
                if (!term_get_string(prompt, line, sizeof(line)))
                    return UI_BIRTH_AHW_PROMPT_CANCEL;
                *weight = atoi(line);
                p_ptr->wt = *weight;
            }

            p_ptr->redraw |= PR_MISC;
            display_player(0);
            continue;
        }

        if (ui_input_is_cancel_key(query2))
            return UI_BIRTH_AHW_PROMPT_CANCEL;

        if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }
}

/* Runs the terminal-native naming prompt for one shared working draft. */
static bool ui_term_birth_name_prompt(char* name, size_t size)
{
    bool name_selected = FALSE;

    if (!name || !size)
        return FALSE;

    display_player(0);

    Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
        TERM_SLATE, "Enter accept name");
    Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
        TERM_SLATE, "  Tab random name");

    Term_putstr(UI_TERM_BIRTH_PROMPT_COL, UI_TERM_BIRTH_INSTRUCT_ROW + 1, -1,
        TERM_L_WHITE, "Enter");
    Term_putstr(UI_TERM_BIRTH_PROMPT_COL + 2, UI_TERM_BIRTH_INSTRUCT_ROW + 2, -1,
        TERM_L_WHITE, "Tab");

    if (character_dungeon)
    {
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL + 40, UI_TERM_BIRTH_INSTRUCT_ROW + 1,
            -1, TERM_SLATE, "ESC abort name change                  ");
        Term_putstr(UI_TERM_BIRTH_PROMPT_COL + 40, UI_TERM_BIRTH_INSTRUCT_ROW + 1,
            -1, TERM_L_WHITE, "ESC");
    }

    Term_gotoxy(8, 2);

    while (!name_selected)
    {
        if (askfor_name(name, size))
        {
            p_ptr->redraw |= PR_MISC;
            if (name[0] != '\0')
                name_selected = TRUE;
            else
                bell("You must choose a name.");
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}

/* Runs the terminal-native editor for one shared birth prompt kind. */
static enum ui_birth_prompt_result ui_term_birth_prompt(
    enum ui_birth_screen_kind kind, char* text, size_t text_size, int* age,
    int* height, int* weight, int age_min, int age_max, int height_min,
    int height_max, int weight_min, int weight_max)
{
    switch (kind)
    {
    case UI_BIRTH_SCREEN_NONE:
    case UI_BIRTH_SCREEN_STATS:
        return UI_BIRTH_PROMPT_CANCEL;

    case UI_BIRTH_SCREEN_HISTORY:
        return (enum ui_birth_prompt_result)ui_term_birth_history_prompt(
            text, text_size);

    case UI_BIRTH_SCREEN_AHW:
        return (enum ui_birth_prompt_result)ui_term_birth_ahw_prompt(age,
            height, weight, age_min, age_max, height_min, height_max,
            weight_min, weight_max);

    case UI_BIRTH_SCREEN_NAME:
        return ui_term_birth_name_prompt(text, text_size)
            ? UI_BIRTH_PROMPT_ACCEPT
            : UI_BIRTH_PROMPT_CANCEL;

    default:
        return UI_BIRTH_PROMPT_CANCEL;
    }
}

/* Draws one label/value field on the semantic character sheet. */
static void ui_term_draw_character_field(int row, int label_col, int value_col,
    const ui_character_field* field, byte value_attr)
{
    if (!field || !field->visible)
        return;

    c_put_str(TERM_WHITE, field->label, row, label_col);
    c_put_str(value_attr, field->value, row, value_col);
}

/* Draws the semantic character-sheet footer actions. */
static void ui_term_draw_character_actions(
    const ui_character_sheet_state* state)
{
    int row = 23;
    int col = 1;
    int i;

    if (!state)
        return;

    Term_erase(0, row, 255);

    for (i = 0; i < state->action_count; i++)
    {
        cptr label;
        int key;
        int highlight_width;

        if (i > 0)
        {
            Term_putstr(col, row, -1, TERM_SLATE, "   ");
            col += 3;
        }

        key = state->actions[i].key;
        if (key == ESCAPE)
            label = "ESC";
        else
            label = state->actions[i].label;

        Term_putstr(col, row, -1, TERM_SLATE, label);
        highlight_width = (key == ESCAPE) ? 3 : 1;
        Term_putstr(col, row, highlight_width, TERM_L_WHITE, label);
        col += (int)strlen(label);
    }
}

/* Draws the shared skill-allocation overlay on top of the character sheet. */
static void ui_term_draw_skill_editor(const ui_character_sheet_state* state)
{
    int i;
    char buf[32];

    if (!state || !state->skill_editor_active)
        return;

    Term_erase(UI_TERM_SKILL_EDITOR_POINTS_COL, UI_TERM_SKILL_EDITOR_POINTS_ROW, 20);
    c_put_str(TERM_WHITE, "Exp Left:", UI_TERM_SKILL_EDITOR_POINTS_ROW,
        UI_TERM_SKILL_EDITOR_POINTS_COL);
    strnfmt(buf, sizeof(buf), "%6d", state->skill_points_left);
    c_put_str(TERM_L_GREEN, buf, UI_TERM_SKILL_EDITOR_POINTS_ROW,
        UI_TERM_SKILL_EDITOR_POINTS_COL + 10);

    for (i = 0; i < S_MAX; i++)
    {
        byte attr = state->skills[i].selected ? TERM_L_BLUE : TERM_L_WHITE;

        Term_erase(UI_TERM_SKILL_EDITOR_SELECT_COL,
            UI_TERM_SKILL_EDITOR_ROW + i, 1);
        if (state->skills[i].selected)
            c_put_str(attr, ">", UI_TERM_SKILL_EDITOR_ROW + i,
                UI_TERM_SKILL_EDITOR_SELECT_COL);

        Term_erase(UI_TERM_SKILL_EDITOR_COST_COL,
            UI_TERM_SKILL_EDITOR_ROW + i, 8);
        strnfmt(buf, sizeof(buf), "%6d", state->skills[i].cost);
        c_put_str(attr, buf, UI_TERM_SKILL_EDITOR_ROW + i,
            UI_TERM_SKILL_EDITOR_COST_COL);
    }

    Term_erase(0, UI_TERM_SKILL_EDITOR_HELP_ROW, 255);
    Term_erase(0, UI_TERM_SKILL_EDITOR_HELP_ROW + 1, 255);
    c_put_str(TERM_SLATE, "Arrow keys move, left/right spend experience",
        UI_TERM_SKILL_EDITOR_HELP_ROW, UI_TERM_SKILL_EDITOR_HELP_COL);
    c_put_str(TERM_SLATE, "Enter accepts current values, Escape goes back",
        UI_TERM_SKILL_EDITOR_HELP_ROW + 1, UI_TERM_SKILL_EDITOR_HELP_COL);
    c_put_str(TERM_L_WHITE, "Arrow keys", UI_TERM_SKILL_EDITOR_HELP_ROW,
        UI_TERM_SKILL_EDITOR_HELP_COL);
    c_put_str(TERM_L_WHITE, "left/right", UI_TERM_SKILL_EDITOR_HELP_ROW,
        UI_TERM_SKILL_EDITOR_HELP_COL + 17);
    c_put_str(TERM_L_WHITE, "Enter", UI_TERM_SKILL_EDITOR_HELP_ROW + 1,
        UI_TERM_SKILL_EDITOR_HELP_COL);
    c_put_str(TERM_L_WHITE, "Escape", UI_TERM_SKILL_EDITOR_HELP_ROW + 1,
        UI_TERM_SKILL_EDITOR_HELP_COL + 29);
}

/* Draws the semantic character sheet on the terminal frontend. */
static void ui_term_character_sheet_render(void)
{
    const ui_character_sheet_state* state = ui_character_get_sheet();
    int i;
    int combat_row = 7;

    if (!state)
        return;

    clear_from(0);

    ui_term_draw_character_field(2, 1, 8, &state->identity[0], TERM_L_BLUE);
    ui_term_draw_character_field(3, 1, 8, &state->identity[1], TERM_L_BLUE);
    ui_term_draw_character_field(4, 1, 8, &state->identity[2], TERM_L_BLUE);

    ui_term_draw_character_field(2, 22, 29, &state->physical[0], TERM_L_BLUE);
    ui_term_draw_character_field(3, 22, 30, &state->physical[1], TERM_L_BLUE);
    ui_term_draw_character_field(4, 22, 30, &state->physical[2], TERM_L_BLUE);

    ui_term_draw_character_field(7, 1, 11, &state->progress[0], TERM_L_GREEN);
    ui_term_draw_character_field(8, 1, 11, &state->progress[1], TERM_L_GREEN);
    ui_term_draw_character_field(9, 1, 11, &state->progress[2], TERM_L_GREEN);
    ui_term_draw_character_field(10, 1, 14, &state->progress[3], TERM_L_GREEN);
    ui_term_draw_character_field(11, 1, 14, &state->progress[4], TERM_L_GREEN);
    ui_term_draw_character_field(12, 1, 14, &state->progress[5], TERM_L_GREEN);
    ui_term_draw_character_field(13, 1, 14, &state->progress[6], TERM_L_GREEN);
    ui_term_draw_character_field(14, 1, 16, &state->progress[7], TERM_L_GREEN);

    for (i = 0; i < A_MAX; i++)
    {
        byte current_attr = state->stats[i].reduced ? TERM_YELLOW : TERM_L_GREEN;

        c_put_str(TERM_WHITE, state->stats[i].label, 2 + i, 41);
        c_put_str(current_attr, state->stats[i].current, 2 + i, 46);
        if (!state->stats[i].show_base)
            continue;

        c_put_str(TERM_SLATE, "=", 2 + i, 49);
        c_put_str(TERM_GREEN, state->stats[i].base, 2 + i, 51);
        if (state->stats[i].show_equip)
            c_put_str(TERM_SLATE, format("%+3d", state->stats[i].equip_mod),
                2 + i, 54);
        if (state->stats[i].show_drain)
            c_put_str(TERM_SLATE, format("%+3d", state->stats[i].drain_mod),
                2 + i, 58);
        if (state->stats[i].show_misc)
            c_put_str(TERM_SLATE, format("%+3d", state->stats[i].misc_mod),
                2 + i, 62);
    }

    for (i = 0; i < UI_CHARACTER_COMBAT_COUNT; i++)
    {
        if (!state->combat[i].visible)
            continue;

        ui_term_draw_character_field(
            combat_row++, 22, 30, &state->combat[i], TERM_L_BLUE);
    }

    for (i = 0; i < S_MAX; i++)
    {
        c_put_str(TERM_WHITE, state->skills[i].label, 7 + i, 41);
        c_put_str(TERM_L_GREEN, format("%3d", state->skills[i].value), 7 + i, 52);
        c_put_str(TERM_SLATE, "=", 7 + i, 56);
        c_put_str(TERM_GREEN, format("%2d", state->skills[i].base), 7 + i, 58);
        if (state->skills[i].stat_mod != 0)
            c_put_str(TERM_SLATE, format("%+3d", state->skills[i].stat_mod),
                7 + i, 61);
        if (state->skills[i].equip_mod != 0)
            c_put_str(TERM_SLATE, format("%+3d", state->skills[i].equip_mod),
                7 + i, 65);
        if (state->skills[i].misc_mod != 0)
            c_put_str(TERM_SLATE, format("%+3d", state->skills[i].misc_mod),
                7 + i, 69);
    }

    ui_term_draw_wrapped_text_block(1, 16, state->history, NULL, 0, 72);
    ui_term_draw_character_actions(state);
    ui_term_draw_skill_editor(state);
}

/* Registers the terminal frontend's prompt and semantic-menu render hooks. */
void ui_term_init(void)
{
    ui_front_set_hooks(
        ui_term_prompt_render, ui_term_prompt_clear, ui_term_menu_render);
    ui_birth_set_stats_render_hook(ui_term_birth_stats_render);
    ui_birth_set_prompt_hook(ui_term_birth_prompt);
    ui_character_set_render_hook(ui_term_character_sheet_render);
}
