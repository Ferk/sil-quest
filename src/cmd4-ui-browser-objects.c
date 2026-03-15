/*
 * Additional object browser helpers kept separate from the Angband-derived
 * command flow in cmd4.c.
 */

static void object_list_entry_label(
    char* buf, size_t buf_size, const object_list_entry* obj)
{
    if (!buf || (buf_size == 0))
        return;

    buf[0] = '\0';

    if (!obj)
        return;

    switch (obj->type)
    {
    case OBJ_NORMAL:
    {
        strip_name(buf, obj->idx);
        break;
    }

    case OBJ_SPECIAL:
    {
        ego_item_type* e_ptr = &e_info[obj->e_idx];

        if (obj->sval == -1)
        {
            strnfmt(buf, buf_size, "  %s", &e_name[e_ptr->name]);
        }
        else
        {
            int j;
            char buf2[79];

            buf2[0] = '\0';
            for (j = 0; j < z_info->k_max; ++j)
            {
                if ((k_info[j].tval == obj->tval) && (k_info[j].sval == obj->sval))
                {
                    strip_name(buf2, j);
                    break;
                }
            }

            strnfmt(buf, buf_size, "%s %s", buf2, &e_name[e_ptr->name]);
        }
        break;
    }

    case OBJ_NONE:
    default:
        break;
    }
}

static byte object_list_entry_attr(const object_list_entry* obj, bool selected)
{
    bool aware = FALSE;

    if (!obj)
        return TERM_WHITE;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        aware = k_info[obj->idx].aware ? TRUE : FALSE;
        break;

    case OBJ_SPECIAL:
        aware = e_info[obj->e_idx].aware ? TRUE : FALSE;
        break;

    case OBJ_NONE:
    default:
        break;
    }

    if (selected)
        return aware ? TERM_L_BLUE : TERM_BLUE;

    return aware ? TERM_WHITE : TERM_SLATE;
}

static int object_list_entry_id(const object_list_entry* obj)
{
    if (!obj)
        return -1;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        return obj->idx + 1;

    case OBJ_SPECIAL:
        return -(obj->e_idx + 1);

    case OBJ_NONE:
    default:
        return 0;
    }
}

static bool object_list_entry_visual(const object_list_entry* obj, int* attr,
    int* chr, int* kind)
{
    int k_idx = -1;
    int i;
    object_kind* k_ptr;

    if (!obj || !attr || !chr || !kind)
        return FALSE;

    switch (obj->type)
    {
    case OBJ_NORMAL:
        k_idx = obj->idx;
        break;

    case OBJ_SPECIAL:
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

    case OBJ_NONE:
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

static void browser_recall_object(const object_list_entry* obj)
{
    char modal_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte modal_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    ui_text_builder modal_builder;

    if (!obj)
        return;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    object_list_entry_append_details(&modal_builder, obj);
    browser_modal_finish(&modal_builder);
    ui_modal_set(modal_text, modal_attrs, ui_text_builder_length(&modal_builder),
        '\r');

    if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
        desc_obj_fake(obj->idx);

    ui_modal_clear();
}

static void browser_build_nav_sequence(char* buf, size_t buf_size, int from_column,
    int from_group, int from_list, int to_column, int to_group, int to_list)
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

static void object_list_entry_append_details(
    ui_text_builder* builder, const object_list_entry* obj)
{
    char buf[80];
    const char* title;

    if (!builder || !obj || (obj->type == OBJ_NONE))
        return;

    if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
    {
        object_type object_type_body;
        object_type* i_ptr = &object_type_body;

        object_wipe(i_ptr);
        object_prep(i_ptr, obj->idx);
        apply_magic_fake(i_ptr);
        object_aware(i_ptr);
        object_known(i_ptr);
        if (cheat_know)
        {
            strnfmt(buf, sizeof(buf), "Idx: %d", obj->idx);
            ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
            ui_text_builder_newline(builder, TERM_WHITE);
        }
        object_info_append_ui_text(builder, i_ptr, TRUE);
        return;
    }

    object_list_entry_label(buf, sizeof(buf), obj);
    title = buf;
    while (*title == ' ')
        title++;

    ui_text_builder_append_line(builder, title, TERM_YELLOW);
    if ((obj->type == OBJ_NORMAL) && cheat_know)
    {
        strnfmt(buf, sizeof(buf), "Idx: %d", obj->idx);
        ui_text_builder_append_line(builder, buf, TERM_L_BLUE);
    }
    ui_text_builder_newline(builder, TERM_WHITE);

    if (obj->type == OBJ_SPECIAL)
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

static void knowledge_objects_publish_ui(int grp_idx[], int grp_cnt, int grp_cur,
    int grp_top, object_list_entry object_idx[], int object_cnt, int object_cur,
    int object_top, int column)
{
    char menu_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte menu_attrs[KNOWLEDGE_MENU_TEXT_MAX];
    char details_text[KNOWLEDGE_MENU_TEXT_MAX];
    byte details_attrs[KNOWLEDGE_MENU_TEXT_MAX];
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
    ui_text_builder_append_line(&menu_builder,
        "Browse groups on the left and objects on the right.", TERM_SLATE);
    ui_text_builder_append_line(
        &menu_builder, "Press r for full recall. Press Esc to exit.", TERM_SLATE);

    if ((object_cnt > 0) && (object_cur >= 0) && (object_cur < object_cnt))
        object_list_entry_append_details(&details_builder, &object_idx[object_cur]);

    ui_menu_begin();

    for (i = 0; i < BROWSER_ROWS && (grp_top + i < grp_cnt); i++)
    {
        char nav[UI_MENU_NAV_MAX];
        int group_index = grp_top + i;

        browser_build_nav_sequence(nav, sizeof(nav), column, grp_cur, object_cur,
            0, group_index, 0);
        ui_menu_add_with_nav(0, 6 + i,
            (int)strlen(object_group_text[grp_idx[group_index]]), 1, 0,
            group_index == grp_cur, TERM_WHITE,
            object_group_text[grp_idx[group_index]], nav);
    }

    for (i = 0; i < BROWSER_ROWS && (object_top + i < object_cnt); i++)
    {
        char buf[80];
        char nav[UI_MENU_NAV_MAX];
        int object_index = object_top + i;
        object_list_entry* obj = &object_idx[object_index];

        object_list_entry_label(buf, sizeof(buf), obj);
        browser_build_nav_sequence(nav, sizeof(nav), column, grp_cur, object_cur,
            1, grp_cur, object_index);
        ui_menu_add_with_nav(1, 6 + i, (int)strlen(buf), 1, 0,
            object_index == object_cur, object_list_entry_attr(obj, FALSE), buf,
            nav);
    }

    ui_menu_set_active_column(column ? 1 : 0);
    ui_menu_set_details_width(34);
    if ((object_cnt > 0) && (object_cur >= 0) && (object_cur < object_cnt)
        && object_list_entry_visual(&object_idx[object_cur], &details_visual_attr,
            &details_visual_char, &details_visual_kind))
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
