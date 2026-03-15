/*
 * Additional artefact and monster browser helpers kept separate from the
 * Angband-derived command flow in cmd4.c.
 */

static ui_text_builder* browser_menu_builder = NULL;

static void browser_text_out_to_ui_builder(byte attr, cptr str)
{
    if (!browser_menu_builder || !str)
        return;

    ui_text_builder_append(browser_menu_builder, str, attr);
}

static void browser_modal_finish(ui_text_builder* builder)
{
    if (!builder)
        return;

    if ((builder->off > 0) && (builder->text[builder->off - 1] != '\n'))
        ui_text_builder_newline(builder, TERM_WHITE);

    ui_text_builder_newline(builder, TERM_WHITE);
    ui_text_builder_append_line(builder, "(press any key)", TERM_L_BLUE);
}

static void artefact_name(int a_idx, char* o_name, size_t o_name_size)
{
    object_type object_type_body;

    if (!o_name || (o_name_size == 0))
        return;

    if (!artefact_info_prepare(a_idx, &object_type_body, o_name, o_name_size))
        o_name[0] = '\0';
}

static void browser_recall_artefact(int a_idx)
{
    char modal_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte modal_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder modal_builder;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    artefact_append_details(&modal_builder, a_idx);
    browser_modal_finish(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    desc_art_fake(a_idx);

    ui_modal_clear();
}

static void artefact_append_details(ui_text_builder* builder, int a_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    artefact_type* a_ptr;
    char buf[128];

    if (!builder)
        return;

    if (!artefact_info_prepare(a_idx, i_ptr, NULL, 0))
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

static void knowledge_artefacts_publish_ui(int grp_idx[], int grp_cnt, int grp_cur,
    int grp_top, int artefact_idx[], int artefact_cnt, int artefact_cur,
    int artefact_top, int column)
{
    char menu_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte menu_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    char details_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte details_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    int i;

    ui_text_builder_init(
        &menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));

    ui_text_builder_append_line(&menu_builder, "Knowledge - Artefacts", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Browse groups on the left and artefacts on the right.", TERM_SLATE);
    ui_text_builder_append_line(
        &menu_builder, "Press r for full recall. Press Esc to exit.", TERM_SLATE);

    if ((artefact_cnt > 0) && (artefact_cur >= 0) && (artefact_cur < artefact_cnt))
        artefact_append_details(&details_builder, artefact_idx[artefact_cur]);

    ui_menu_begin();

    for (i = 0; i < BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        browser_build_nav_sequence(nav, sizeof(nav), column, grp_cur,
            artefact_cur, 0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(object_group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE,
            object_group_text[grp_idx[group_index]], nav);
    }

    for (i = 0; i < BROWSER_ROWS && (artefact_top + i < artefact_cnt); i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int artefact_index = artefact_top + i;

        artefact_name(artefact_idx[artefact_index], buf, sizeof(buf));

        browser_build_nav_sequence(nav, sizeof(nav), column, grp_cur,
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

static void monster_append_details(ui_text_builder* builder, int r_idx)
{
    char race_name[80];
    char buf[80];
    void (*old_text_out_hook)(byte a, cptr str);
    int old_text_out_wrap;
    int old_text_out_indent;

    if (!builder || (r_idx <= 0))
        return;

    monster_desc_race(race_name, sizeof(race_name), r_idx);
    ui_text_builder_append_line(builder, race_name, TERM_YELLOW);
    if (cheat_know)
    {
        strnfmt(buf, sizeof(buf), "Idx: %d", r_idx);
        ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
    }
    ui_text_builder_newline(builder, TERM_WHITE);

    old_text_out_hook = text_out_hook;
    old_text_out_wrap = text_out_wrap;
    old_text_out_indent = text_out_indent;

    browser_menu_builder = builder;
    text_out_hook = browser_text_out_to_ui_builder;
    text_out_wrap = 0;
    text_out_indent = 0;

    describe_monster(r_idx, FALSE);

    text_out_hook = old_text_out_hook;
    text_out_wrap = old_text_out_wrap;
    text_out_indent = old_text_out_indent;
    browser_menu_builder = NULL;
}

static void monster_set_details_visual(int r_idx)
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

static void browser_recall_monster(int r_idx)
{
    char modal_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte modal_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder modal_builder;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    monster_append_details(&modal_builder, r_idx);
    browser_modal_finish(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    screen_roff(r_idx);
    (void)inkey();

    ui_modal_clear();
}

static void monster_group_append_summary(ui_text_builder* builder, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;
    char buf[128];

    if (!builder)
        return;

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

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(
            buf, sizeof(buf), "Total Creatures Slain: %u.", slay_count);
    }
    else
    {
        strnfmt(buf, sizeof(buf), "Known Uniques: %u, Slain Uniques: %u.",
            known_uniques, dead_uniques);
    }

    ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
}

static void knowledge_monsters_publish_ui(int grp_idx[], int grp_cnt, int grp_cur,
    int grp_top, monster_list_entry* mon_idx, int monster_count, int mon_cur,
    int mon_top, int column)
{
    char menu_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte menu_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    char details_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte details_attrs[KNOWLEDGE_MENU_TEXT_MAX];
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
    monster_group_append_summary(&menu_builder, grp_idx[grp_cur]);

    if ((monster_count > 0) && (mon_cur >= 0) && (mon_cur < monster_count))
    {
        monster_append_details(&details_builder, mon_idx[mon_cur].r_idx);
    }
    else
    {
        ui_text_builder_append_line(&details_builder,
            "No known monsters in this group yet.", TERM_SLATE);
    }

    ui_menu_begin();

    for (i = 0; i < BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        browser_build_nav_sequence(
            nav, sizeof(nav), column, grp_cur, mon_cur, 0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(monster_group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE,
            monster_group_text[grp_idx[group_index]], nav);
    }

    for (i = 0; i < BROWSER_ROWS && (mon_top + i < monster_count); i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int mon_index = mon_top + i;

        monster_desc_race(buf, sizeof(buf), mon_idx[mon_index].r_idx);
        browser_build_nav_sequence(
            nav, sizeof(nav), column, grp_cur, mon_cur, 1, grp_cur, mon_index);
        ui_menu_add_with_nav(1, 6 + i, (int)strlen(buf), 1, 0,
            mon_index == mon_cur, TERM_WHITE, buf, nav);
    }

    ui_menu_set_active_column(column ? 1 : 0);
    ui_menu_set_details_width(36);
    if ((monster_count > 0) && (mon_cur >= 0) && (mon_cur < monster_count))
        monster_set_details_visual(mon_idx[mon_cur].r_idx);
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}
