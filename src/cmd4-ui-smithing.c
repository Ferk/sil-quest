/*
 * Additional smithing UI-model helpers kept separate from the Angband-derived
 * command flow in cmd4.c.
 */

static ui_text_builder* smithing_menu_builder = NULL;

static void text_out_to_ui_builder(byte attr, cptr str)
{
    if (!smithing_menu_builder || !str)
        return;

    ui_text_builder_append(smithing_menu_builder, str, attr);
}

static void smithing_menu_text_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size, cptr title)
{
    ui_text_builder_init(builder, text, attrs, size);

    if (title && title[0])
    {
        ui_text_builder_append_line(builder, title, TERM_WHITE);
        ui_text_builder_newline(builder, TERM_WHITE);
    }
}

static void smithing_menu_publish(const ui_text_builder* menu_builder,
    const ui_text_builder* details_builder)
{
    if (menu_builder)
    {
        ui_menu_set_text(menu_builder->text, menu_builder->attrs,
            ui_text_builder_length(menu_builder));
    }

    if (details_builder)
    {
        ui_menu_set_details(details_builder->text, details_builder->attrs,
            ui_text_builder_length(details_builder));
    }

    ui_menu_end();
}

static void smithing_menu_append_requirement_details(
    ui_text_builder* builder, int dif)
{
    char buf[80];
    int turn_multiplier = 10;
    byte attr;
    bool can_afford = TRUE;
    size_t title_pos;

    if (!builder)
        return;

    ui_text_builder_newline(builder, TERM_WHITE);
    title_pos = builder->off;
    ui_text_builder_append_line(builder, "Cost:", TERM_SLATE);

    if (smithing_cost.weaponsmith)
    {
        ui_text_builder_append_line(builder, "Weaponsmith", TERM_RED);
        can_afford = FALSE;
    }
    if (smithing_cost.armoursmith)
    {
        ui_text_builder_append_line(builder, "Armoursmith", TERM_RED);
        can_afford = FALSE;
    }
    if (smithing_cost.jeweller)
    {
        ui_text_builder_append_line(builder, "Jeweller", TERM_RED);
        can_afford = FALSE;
    }
    if (smithing_cost.enchantment)
    {
        ui_text_builder_append_line(builder, "Enchantment", TERM_RED);
        can_afford = FALSE;
    }
    if (smithing_cost.artifice)
    {
        ui_text_builder_append_line(builder, "Artifice", TERM_RED);
        can_afford = FALSE;
    }
    if (smithing_cost.uses > 0)
    {
        if (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }

        if (smithing_cost.uses == 1)
            strnfmt(buf, sizeof(buf), "%d Use (of %d)", smithing_cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "%d Uses (of %d)", smithing_cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.drain > 0)
    {
        if (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
            attr = TERM_BLUE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Smithing", smithing_cost.drain);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.mithril > 0)
    {
        if (smithing_cost.mithril <= mithril_carried())
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d.%d lb Mithril", smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.str > 0)
    {
        if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Str", smithing_cost.str);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.dex > 0)
    {
        if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Dex", smithing_cost.dex);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.con > 0)
    {
        if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Con", smithing_cost.con);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.gra > 0)
    {
        if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Gra", smithing_cost.gra);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (smithing_cost.exp > 0)
    {
        if (p_ptr->new_exp >= smithing_cost.exp)
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Exp", smithing_cost.exp);
        ui_text_builder_append_line(builder, buf, attr);
    }

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        turn_multiplier /= 2;

    strnfmt(buf, sizeof(buf), "%d Turns", MAX(10, dif * turn_multiplier));
    ui_text_builder_append_line(builder, buf, TERM_SLATE);
    if (!can_afford)
    {
        int i;

        for (i = 0; i < 5; i++)
            builder->attrs[title_pos + i] = TERM_L_DARK;
    }
}

static void smithing_menu_append_details(ui_text_builder* builder)
{
    char o_desc[160];
    char buf[160];
    int display_flag;
    int dif;
    byte dif_attr;
    void (*old_text_out_hook)(byte a, cptr str);
    int old_text_out_indent;
    int old_text_out_wrap;

    if (!builder)
        return;

    if (p_ptr->smithing_leftover)
    {
        ui_text_builder_append_line(builder, "In progress:", TERM_L_BLUE);
        strnfmt(buf, sizeof(buf), "%3d turns left", p_ptr->smithing_leftover);
        ui_text_builder_append_line(builder, buf, TERM_BLUE);
        ui_text_builder_newline(builder, TERM_WHITE);
    }

    if (smith_o_ptr->tval == 0)
        return;

    display_flag = (smith_o_ptr->number > 1) ? TRUE : FALSE;
    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);
    my_strcat(o_desc,
        format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10,
            (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        sizeof(o_desc));

    ui_text_builder_append_line(builder, o_desc, TERM_L_WHITE);

    dif = object_difficulty(smith_o_ptr);
    dif_attr = too_difficult(smith_o_ptr) ? TERM_L_DARK : TERM_SLATE;
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        dif_attr = TERM_BLUE;
    }

    strnfmt(buf, sizeof(buf), "Difficulty: %d (max %d)", dif,
        p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
    ui_text_builder_append_line(builder, buf, dif_attr);
    smithing_menu_append_requirement_details(builder, dif);
    ui_text_builder_newline(builder, TERM_WHITE);

    old_text_out_hook = text_out_hook;
    old_text_out_indent = text_out_indent;
    old_text_out_wrap = text_out_wrap;

    object_info_out_flags = object_flags;
    smithing_menu_builder = builder;
    text_out_hook = text_out_to_ui_builder;
    text_out_indent = 0;
    text_out_wrap = 0;

    if ((k_text + k_info[smith_o_ptr->k_idx].text)[0] != '\0')
    {
        text_out_c(TERM_WHITE, k_text + k_info[smith_o_ptr->k_idx].text);
        text_out(" ");
    }

    if (object_info_out(smith_o_ptr))
        text_out("\n");

    text_out_hook = old_text_out_hook;
    text_out_indent = old_text_out_indent;
    text_out_wrap = old_text_out_wrap;
    smithing_menu_builder = NULL;
}
