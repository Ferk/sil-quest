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

#include "ui-model.h"
#include "ui-term.h"

#define UI_TERM_MENU_MAX_COLUMNS 8
#define UI_TERM_MENU_MIN_LIST_GAP 4
#define UI_TERM_MENU_MIN_SECOND_COL 24
#define UI_TERM_MENU_MIN_DETAILS_COL 42
#define UI_TERM_LOCAL_MENU_MAX_ITEMS 16
#define UI_TERM_LOCAL_MENU_MAX_WIDTH 52

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
        int width = (int)strlen(items[i].label);

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
    int columns[UI_TERM_MENU_MAX_COLUMNS];
    int widths[UI_TERM_MENU_MAX_COLUMNS];
    int positions[UI_TERM_MENU_MAX_COLUMNS];
    int column_count;
    int term_wid = 80;
    int term_hgt = 24;
    int text_width;
    int text_lines;
    int details_lines;
    int details_width;
    int details_available_width;
    int min_row = 0;
    int max_row = 0;
    int row_shift = 0;
    int i;
    int selected;
    int details_col = UI_TERM_MENU_MIN_DETAILS_COL;
    int details_row = 0;
    int details_visual_height = 0;
    int menu_right = 0;
    int panel_right = 0;
    int panel_bottom = 0;
    int panel_origin_x = 0;
    int panel_origin_y = 0;
    bool local_modal = FALSE;
    Term_get_size(&term_wid, &term_hgt);
    (void)term_hgt;

    selected = ui_term_menu_cursor_index(items, count, active_column);

    column_count = ui_term_collect_menu_columns(items, count, columns, widths);
    ui_term_layout_menu_columns(widths, column_count, positions);

    if (column_count > 0)
        menu_right = positions[column_count - 1] + widths[column_count - 1];

    local_modal = ui_term_menu_is_local_modal(
        column_count, count, menu_right, 0, term_wid, term_hgt);

    text_width = term_wid;
    if (local_modal)
    {
        text_width = UI_TERM_LOCAL_MENU_MAX_WIDTH;
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

    if (!local_modal && (column_count > 0))
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
    if (!local_modal && (details_available_width < 12))
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

    panel_right = local_modal ? (text_width - 1) : menu_right;
    if (!local_modal
        && (ui_menu_get_details_visual_kind() != UI_MENU_VISUAL_NONE
            || details_lines > 0))
    {
        int details_right = details_col + details_width - 1;

        if (details_right > panel_right)
            panel_right = details_right;
    }

    panel_bottom = text_lines - 1;
    if ((count > 0) && (max_row + row_shift > panel_bottom))
        panel_bottom = max_row + row_shift;

    details_row = local_modal
        ? ((count > 0) ? (max_row + row_shift + 2) : text_lines)
        : ((count > 0) ? (min_row + row_shift) : text_lines);
    if ((details_visual_height > 0) || (details_lines > 0))
    {
        int details_bottom =
            details_row + details_visual_height + details_lines - 1;

        if (details_bottom > panel_bottom)
            panel_bottom = details_bottom;
    }

    if (local_modal)
    {
        ui_term_local_menu_origin(panel_right + 1, panel_bottom + 1,
            term_wid, term_hgt, &panel_origin_x, &panel_origin_y);
    }

    if (local_modal)
    {
        ui_term_clear_previous_local_menu();
        ui_term_erase_rect(panel_origin_x, panel_origin_y,
            panel_origin_x + panel_right, panel_origin_y + panel_bottom);
        ui_term_local_menu_left = panel_origin_x;
        ui_term_local_menu_top = panel_origin_y;
        ui_term_local_menu_right = panel_origin_x + panel_right;
        ui_term_local_menu_bottom = panel_origin_y + panel_bottom;
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
        int column_index = ui_term_find_menu_column(columns, column_count, items[i].x);
        int col = panel_origin_x + positions[column_index];
        int row = panel_origin_y + items[i].y + row_shift;

        Term_putstr(col, row, -1,
            ui_term_menu_item_attr(&items[i], active_column), items[i].label);
    }

    details_row = ui_term_draw_menu_details_visual(
        panel_origin_x + details_col, panel_origin_y + details_row);
    ui_term_draw_wrapped_text_block(panel_origin_x + details_col, details_row,
        menu_details,
        ui_menu_get_details_attrs(), ui_menu_get_details_attrs_len(),
        details_width);

    Term_fresh();

    if ((selected >= 0) && (selected < count))
    {
        int column_index =
            ui_term_find_menu_column(columns, column_count, items[selected].x);

        Term_gotoxy(panel_origin_x + positions[column_index],
            panel_origin_y + items[selected].y + row_shift);
    }
}

/* Registers the terminal frontend's prompt and semantic-menu render hooks. */
void ui_term_init(void)
{
    ui_front_set_hooks(
        ui_term_prompt_render, ui_term_prompt_clear, ui_term_menu_render);
}
