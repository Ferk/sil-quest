/* File: ui-view.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Nearby monster and object view helpers.
 */

#include "angband.h"

#include "ui-model.h"

#define UI_VIEW_MAX_LINES 20
#define UI_VIEW_TEXT_MAX 8192

typedef struct ui_view_monster_line ui_view_monster_line;
typedef struct ui_view_object_line ui_view_object_line;

struct ui_view_monster_line
{
    int distance;
    char monster_character;
    int monster_color;
    int alert_color;
    char direction[12];
    char name[40];
    char stance[20];
};

struct ui_view_object_line
{
    int distance;
    char object_character;
    int object_color;
    char direction[12];
    char name[60];
};

/* Writes a cardinal direction from the player to one map grid. */
static void ui_view_write_direction(int y, int x, char* buffer, int buffer_size)
{
    bool north;
    bool south;
    bool east;
    bool west;
    int off = 0;

    if (!buffer || (buffer_size < 10))
        return;

    north = p_ptr->py > y;
    south = p_ptr->py < y;
    east = p_ptr->px < x;
    west = p_ptr->px > x;

    buffer[0] = '\0';

    if (north)
    {
        strncpy(buffer, "north", buffer_size);
        off += 5;
    }
    else if (south)
    {
        strncpy(buffer, "south", buffer_size);
        off += 5;
    }

    if (east)
    {
        strncpy(buffer + off, "east", buffer_size - off);
    }
    else if (west)
    {
        strncpy(buffer + off, "west", buffer_size - off);
    }
}

/* Appends a run of spaces to one text builder. */
static void ui_view_builder_append_spaces(
    ui_text_builder* builder, int count, byte attr)
{
    char spaces[33];

    memset(spaces, ' ', sizeof(spaces) - 1);
    spaces[sizeof(spaces) - 1] = '\0';

    while (count > 0)
    {
        int chunk = MIN(count, (int)sizeof(spaces) - 1);

        spaces[chunk] = '\0';
        ui_text_builder_append(builder, spaces, attr);
        spaces[chunk] = ' ';
        count -= chunk;
    }
}

/* Appends one text cell padded out to the requested width. */
static void ui_view_builder_append_padded(
    ui_text_builder* builder, cptr text, int width, byte attr)
{
    int len = 0;

    if (!builder)
        return;

    if (text)
    {
        ui_text_builder_append(builder, text, attr);
        len = strlen(text);
    }

    if (width > len)
        ui_view_builder_append_spaces(builder, width - len, attr);
}

/* Collects the currently viewable monsters for the nearby-monster screen. */
static int ui_view_collect_monsters(bool line_of_sight_only,
    ui_view_monster_line lines[UI_VIEW_MAX_LINES],
    int* longest_name_length_out)
{
    int i;
    int j = 0;
    int longest_name_length = 0;

    get_sorted_target_list(TARGET_LIST_MONSTER, 0);

    for (i = 0; i < temp_n; i++)
    {
        int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        char m_name[40];
        int name_length;

        if (j >= UI_VIEW_MAX_LINES)
            break;
        if (!m_ptr->ml)
            continue;
        if (!player_has_los_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(lines[j].stance, '\0', sizeof(lines[j].stance));

        monster_desc(m_name, sizeof(m_name), m_ptr, 0x80);
        name_length = strlen(m_name);
        longest_name_length = MAX(longest_name_length, name_length);

        ui_view_write_direction(
            temp_y[i], temp_x[i], lines[j].direction, sizeof(lines[j].direction));
        if (!get_alertness_text(m_ptr, sizeof(lines[j].stance), lines[j].stance,
                &lines[j].alert_color))
        {
            return j;
        }

        lines[j].monster_character = r_ptr->d_char;
        lines[j].monster_color = r_ptr->d_attr;
        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);
        strncpy(lines[j].name, m_name, sizeof(lines[j].name));

        j++;
    }

    if (longest_name_length_out)
        *longest_name_length_out = longest_name_length;

    return j;
}

/* Publishes the semantic modal text for the nearby-monster screen. */
static void ui_view_publish_monsters_modal(bool line_of_sight_only)
{
    ui_view_monster_line lines[UI_VIEW_MAX_LINES];
    char modal_text[UI_VIEW_TEXT_MAX];
    byte modal_attrs[UI_VIEW_TEXT_MAX];
    ui_text_builder builder;
    int longest_name_length = 0;
    int count;
    int i;

    count = ui_view_collect_monsters(
        line_of_sight_only, lines, &longest_name_length);
    ui_text_builder_init(&builder, modal_text, modal_attrs, sizeof(modal_text));

    ui_text_builder_append_line(&builder,
        line_of_sight_only ? "Monsters you can see" : "Monsters on screen",
        TERM_L_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    if (!count)
    {
        ui_text_builder_append_line(&builder, "No visible monsters.", TERM_WHITE);
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            int distance_color;
            char glyph[2];

            glyph[0] = lines[i].monster_character;
            glyph[1] = '\0';

            if (lines[i].distance < 5)
                distance_color = TERM_WHITE;
            else if (lines[i].distance < 10)
                distance_color = TERM_L_WHITE;
            else
                distance_color = TERM_L_DARK;

            ui_text_builder_append(&builder, glyph, lines[i].monster_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_view_builder_append_padded(
                &builder, lines[i].direction, 10, distance_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_view_builder_append_padded(
                &builder, lines[i].name, longest_name_length, TERM_WHITE);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_text_builder_append_line(
                &builder, lines[i].stance, lines[i].alert_color);
        }
    }

    ui_text_builder_newline(&builder, TERM_WHITE);
    ui_text_builder_append_line(
        &builder, "(press [ to toggle, Esc to exit)", TERM_L_BLUE);
    ui_modal_set(
        modal_text, modal_attrs, ui_text_builder_length(&builder), ESCAPE);
}

/* Draws the nearby-monster overlay directly onto the terminal. */
static void ui_view_show_monsters(bool line_of_sight_only)
{
    ui_view_monster_line lines[UI_VIEW_MAX_LINES];
    int col;
    int count;
    int i;
    int longest_name_length = 0;

    count = ui_view_collect_monsters(
        line_of_sight_only, lines, &longest_name_length);

    col = 79 - longest_name_length - sizeof(lines[0].direction)
        - sizeof(lines[0].stance) - 9;
    col = MAX(0, col);

    for (i = 0; i < count; ++i)
    {
        int distance_color;
        char monster_char[2];

        monster_char[0] = lines[i].monster_character;
        monster_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        prt("", i + 1, col);
        c_put_str(lines[i].monster_color, monster_char, i + 1, col + 2);
        c_put_str(distance_color, lines[i].direction, i + 1, col + 6);
        c_put_str(TERM_WHITE, lines[i].name, i + 1,
            col + sizeof(lines[0].direction) + 6);
        c_put_str(lines[i].alert_color, lines[i].stance, i + 1,
            col + sizeof(lines[0].direction) + longest_name_length + 8);
    }

    if (count)
    {
        prt("", count + 1, col);
    }
    else
    {
        prt("", 1, 40);
        c_put_str(TERM_WHITE, "No visible monsters.", 1, 50);
        prt("", 2, 40);
        prt("", 3, 40);
    }
}

/* Collects the currently viewable objects for the nearby-object screen. */
static int ui_view_collect_objects(bool line_of_sight_only,
    ui_view_object_line lines[UI_VIEW_MAX_LINES], int* longest_name_length_out)
{
    int i;
    int j = 0;
    int longest_name_length = 0;

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[60];
        int name_length;

        if (j >= UI_VIEW_MAX_LINES)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(o_name, '\0', sizeof(o_name));

        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
        name_length = strlen(o_name);
        longest_name_length = MAX(longest_name_length, name_length);

        ui_view_write_direction(
            temp_y[i], temp_x[i], lines[j].direction, sizeof(lines[j].direction));
        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        if (strlen(lines[j].direction) == 0)
            my_strcpy(lines[j].direction, "underfoot", sizeof(lines[j].direction));

        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);
        strncpy(lines[j].name, o_name, sizeof(lines[j].name));

        j++;
    }

    if (longest_name_length_out)
        *longest_name_length_out = longest_name_length;

    return j;
}

/* Publishes the semantic modal text for the nearby-object screen. */
static void ui_view_publish_objects_modal(bool line_of_sight_only)
{
    ui_view_object_line lines[UI_VIEW_MAX_LINES];
    char modal_text[UI_VIEW_TEXT_MAX];
    byte modal_attrs[UI_VIEW_TEXT_MAX];
    ui_text_builder builder;
    int longest_name_length = 0;
    int count;
    int i;

    count = ui_view_collect_objects(
        line_of_sight_only, lines, &longest_name_length);
    ui_text_builder_init(&builder, modal_text, modal_attrs, sizeof(modal_text));

    ui_text_builder_append_line(&builder,
        line_of_sight_only ? "Objects you can see" : "Objects on screen",
        TERM_L_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    if (!count)
    {
        ui_text_builder_append_line(&builder, "No visible objects.", TERM_WHITE);
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            int distance_color;
            char glyph[2];

            glyph[0] = lines[i].object_character;
            glyph[1] = '\0';

            if (lines[i].distance < 5)
                distance_color = TERM_WHITE;
            else if (lines[i].distance < 10)
                distance_color = TERM_L_WHITE;
            else
                distance_color = TERM_L_DARK;

            ui_text_builder_append(&builder, glyph, lines[i].object_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_view_builder_append_padded(
                &builder, lines[i].direction, 10, distance_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_text_builder_append_line(&builder, lines[i].name, TERM_WHITE);
        }
    }

    ui_text_builder_newline(&builder, TERM_WHITE);
    ui_text_builder_append_line(
        &builder, "(press ] to toggle, Esc to exit)", TERM_L_BLUE);
    ui_modal_set(
        modal_text, modal_attrs, ui_text_builder_length(&builder), ESCAPE);
}

/* Draws the nearby-object overlay directly onto the terminal. */
static void ui_view_show_objects(bool line_of_sight_only)
{
    ui_view_object_line lines[UI_VIEW_MAX_LINES];
    int col;
    int count;
    int i;
    int longest_name_length = 0;

    count = ui_view_collect_objects(
        line_of_sight_only, lines, &longest_name_length);

    col = 79 - longest_name_length - sizeof(lines[0].direction) - 9;
    col = MAX(0, col);

    prt("", 1, col);

    for (i = 0; i < count; ++i)
    {
        int distance_color;
        char object_char_buf[2];

        object_char_buf[0] = lines[i].object_character;
        object_char_buf[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        prt("", i + 1, col);
        c_put_str(lines[i].object_color, object_char_buf, i + 1, col + 2);
        c_put_str(distance_color, lines[i].direction, i + 1, col + 6);
        c_put_str(TERM_WHITE, lines[i].name, i + 1,
            col + sizeof(lines[0].direction) + 6);
    }

    if (count)
    {
        prt("", count + 1, col);
    }
    else
    {
        prt("", 1, 40);
        c_put_str(TERM_WHITE, "No visible objects.", 1, 50);
        prt("", 2, 40);
        prt("", 3, 40);
    }
}

/* Displays the nearby-monster command. */
void do_cmd_view_monsters(void)
{
    char key = '[';
    bool show_los = TRUE;

    while (key == '[')
    {
        ui_view_publish_monsters_modal(show_los);
        screen_save();
        ui_view_show_monsters(show_los);
        prt(show_los ? "Monsters you can see (press [ to toggle):"
                     : "Monsters on screen (press [ to toggle):",
            0, 0);
        key = inkey();
        show_los = !show_los;
        screen_load();
        ui_modal_clear();
    }
}

/* Displays the nearby-object command. */
void do_cmd_view_objects(void)
{
    char key = ']';
    bool show_los = TRUE;

    while (key == ']')
    {
        ui_view_publish_objects_modal(show_los);
        screen_save();
        ui_view_show_objects(show_los);
        prt(show_los ? "Objects you can see (press ] to toggle):"
                     : "Objects on screen (press ] to toggle):",
            0, 0);
        key = inkey();
        show_los = !show_los;
        screen_load();
        ui_modal_clear();
    }
}
