/*
 * Additional nearby-view helpers kept separate from the Angband-derived
 * command flow in cmd4.c.
 */

static void write_direction_from_player_to_buffer(
    int y, int x, char* buffer, int buffer_size)
{
    bool north, south, east, west;
    int buffer_offset = 0;

    if (buffer_size < 10)
        return;

    north = p_ptr->py > y;
    south = p_ptr->py < y;
    east = p_ptr->px < x;
    west = p_ptr->px > x;

    if (north)
    {
        strncpy(buffer, "north", 6);
        strcpy(buffer, "north");
        buffer_offset += 5;
    }
    else if (south)
    {
        strncpy(buffer, "south", 6);
        buffer_offset += 5;
    }

    if (east)
    {
        strncpy(buffer + buffer_offset, "east", 5);
        buffer_offset += 4;
    }
    else if (west)
    {
        strncpy(buffer + buffer_offset, "west", 5);
        buffer_offset += 4;
    }
}

#define MAX_VIEW_LINES 20

typedef struct view_monster_data_line view_monster_data_line;
struct view_monster_data_line
{
    int distance;
    char monster_character;
    int monster_color;
    int alert_color;
    char direction[12];
    char name[40];
    char stance[20];
};

typedef struct view_object_data_line view_object_data_line;
struct view_object_data_line
{
    int distance;
    char object_character;
    int object_color;
    char direction[12];
    char name[60];
};

static void ui_text_builder_append_spaces(
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

static void ui_text_builder_append_padded(ui_text_builder* builder, cptr text,
    int width, byte attr)
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
        ui_text_builder_append_spaces(builder, width - len, attr);
}

static int collect_nearby_monsters(bool line_of_sight_only,
    view_monster_data_line lines[MAX_VIEW_LINES], int* longest_name_length_out)
{
    int i, j;
    int longest_name_length = 0;

    get_sorted_target_list(TARGET_LIST_MONSTER, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int m_idx = cave_m_idx[temp_y[i]][temp_x[i]];
        monster_type* m_ptr = &mon_list[m_idx];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        char m_name[40];
        int name_length;

        if (j >= 20)
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

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));
        if (!get_alertness_text(m_ptr, sizeof(lines[j].stance), lines[j].stance,
                &lines[j].alert_color))
            return j;

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

static void publish_view_monsters_modal(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];
    char modal_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte modal_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder builder;
    int longest_name_length = 0;
    int count;
    int i;

    count = collect_nearby_monsters(
        line_of_sight_only, lines, &longest_name_length);
    ui_text_builder_init(&builder, modal_text, modal_attrs, sizeof(modal_text));

    ui_text_builder_append_line(&builder,
        line_of_sight_only ? "Monsters you can see"
                           : "Monsters on screen",
        TERM_L_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    if (!count)
    {
        ui_text_builder_append_line(
            &builder, "No visible monsters.", TERM_WHITE);
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
            ui_text_builder_append_padded(
                &builder, lines[i].direction, 10, distance_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_text_builder_append_padded(
                &builder, lines[i].name, longest_name_length, TERM_WHITE);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_text_builder_append_line(
                &builder, lines[i].stance, lines[i].alert_color);
        }
    }

    ui_text_builder_newline(&builder, TERM_WHITE);
    ui_text_builder_append_line(&builder,
        "(press [ to toggle, Esc to exit)", TERM_L_BLUE);
    ui_modal_set(
        modal_text, modal_attrs, ui_text_builder_length(&builder), ESCAPE);
}

static void show_nearby_monsters(bool line_of_sight_only)
{
    view_monster_data_line lines[MAX_VIEW_LINES];
    int col;
    int count;
    int i;
    int longest_name_length = 0;

    count = collect_nearby_monsters(
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

        /* Clear the line */
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

static int collect_nearby_objects(bool line_of_sight_only,
    view_object_data_line lines[MAX_VIEW_LINES], int* longest_name_length_out)
{
    int i, j;
    int longest_name_length = 0;

    get_sorted_target_list(TARGET_LIST_OBJECT, 0);

    j = 0;
    for (i = 0; i < temp_n; i++)
    {
        int o_idx = cave_o_idx[temp_y[i]][temp_x[i]];
        object_type* o_ptr = &o_list[o_idx];
        char o_name[60];
        int name_length;

        if (j >= 20)
            break;
        if (!player_can_see_bold(temp_y[i], temp_x[i]) && line_of_sight_only)
            continue;

        memset(lines[j].direction, '\0', sizeof(lines[j].direction));
        memset(lines[j].name, '\0', sizeof(lines[j].name));
        memset(o_name, '\0', sizeof(o_name));

        object_desc(o_name, sizeof(o_name), o_ptr, TRUE, 3);
        name_length = strlen(o_name);

        longest_name_length = MAX(longest_name_length, name_length);

        write_direction_from_player_to_buffer(temp_y[i], temp_x[i],
            lines[j].direction, sizeof(lines[j].direction));

        lines[j].distance
            = distance(p_ptr->py, p_ptr->px, temp_y[i], temp_x[i]);

        if (strlen(lines[j].direction) == 0)
            snprintf(lines[j].direction, sizeof(lines[j].direction), "%s", "underfoot");

        lines[j].object_character = object_char(o_ptr);
        lines[j].object_color = object_attr(o_ptr);

        strncpy(lines[j].name, o_name, sizeof(lines[j].name));

        j++;
    }

    if (longest_name_length_out)
        *longest_name_length_out = longest_name_length;

    return j;
}

static void publish_view_objects_modal(bool line_of_sight_only)
{
    view_object_data_line lines[MAX_VIEW_LINES];
    char modal_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte modal_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder builder;
    int longest_name_length = 0;
    int count;
    int i;

    count = collect_nearby_objects(
        line_of_sight_only, lines, &longest_name_length);
    ui_text_builder_init(&builder, modal_text, modal_attrs, sizeof(modal_text));

    ui_text_builder_append_line(&builder,
        line_of_sight_only ? "Objects you can see" : "Objects on screen",
        TERM_L_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    if (!count)
    {
        ui_text_builder_append_line(
            &builder, "No visible objects.", TERM_WHITE);
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
            ui_text_builder_append_padded(
                &builder, lines[i].direction, 10, distance_color);
            ui_text_builder_append(&builder, "  ", TERM_WHITE);
            ui_text_builder_append_line(&builder, lines[i].name, TERM_WHITE);
        }
    }

    ui_text_builder_newline(&builder, TERM_WHITE);
    ui_text_builder_append_line(&builder,
        "(press ] to toggle, Esc to exit)", TERM_L_BLUE);
    ui_modal_set(
        modal_text, modal_attrs, ui_text_builder_length(&builder), ESCAPE);
}

static void show_nearby_objects(bool line_of_sight_only)
{
    view_object_data_line lines[MAX_VIEW_LINES];
    int col;
    int count;
    int i;
    int longest_name_length = 0;

    count = collect_nearby_objects(
        line_of_sight_only, lines, &longest_name_length);

    col = 79 - longest_name_length - sizeof(lines[0].direction) - 9;
    col = MAX(0, col);

    prt("", 1, col);

    for (i = 0; i < count; ++i)
    {
        int distance_color;

        char o_char[2];

        o_char[0] = lines[i].object_character;
        o_char[1] = '\0';

        if (lines[i].distance < 5)
            distance_color = TERM_WHITE;
        else if (lines[i].distance < 10)
            distance_color = TERM_L_WHITE;
        else
            distance_color = TERM_L_DARK;

        /* Clear the line */
        prt("", i + 1, col);

        c_put_str(lines[i].object_color, o_char, i + 1, col + 2);
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
