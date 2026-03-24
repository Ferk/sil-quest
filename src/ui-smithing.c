/* File: ui-smithing.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Smithing menu presentation helpers kept separate from cmd4.c.
 */

#include "angband.h"

#include "ui-smithing.h"

static ui_text_builder* ui_smithing_builder = NULL;

/* Appends smithing recall text through the shared UI text builder hook. */
static void ui_smithing_text_out_to_builder(byte attr, cptr str)
{
    if (!ui_smithing_builder || !str)
        return;

    ui_text_builder_append(ui_smithing_builder, str, attr);
}

/* Appends the current smithing requirements to the details pane. */
static void ui_smithing_append_requirement_details(ui_text_builder* builder,
    const smithing_cost_type* cost, int dif,
    int (*forge_uses_fn)(int y, int x), int (*mithril_carried_fn)(void))
{
    char buf[80];
    int turn_multiplier = 10;
    byte attr;
    bool can_afford = TRUE;
    size_t title_pos;

    if (!builder || !cost || !forge_uses_fn || !mithril_carried_fn)
        return;

    ui_text_builder_newline(builder, TERM_WHITE);
    title_pos = builder->off;
    ui_text_builder_append_line(builder, "Cost:", TERM_SLATE);

    if (cost->weaponsmith)
    {
        ui_text_builder_append_line(builder, "Weaponsmith", TERM_RED);
        can_afford = FALSE;
    }
    if (cost->armoursmith)
    {
        ui_text_builder_append_line(builder, "Armoursmith", TERM_RED);
        can_afford = FALSE;
    }
    if (cost->jeweller)
    {
        ui_text_builder_append_line(builder, "Jeweller", TERM_RED);
        can_afford = FALSE;
    }
    if (cost->enchantment)
    {
        ui_text_builder_append_line(builder, "Enchantment", TERM_RED);
        can_afford = FALSE;
    }
    if (cost->artifice)
    {
        ui_text_builder_append_line(builder, "Artifice", TERM_RED);
        can_afford = FALSE;
    }
    if (cost->uses > 0)
    {
        if (forge_uses_fn(p_ptr->py, p_ptr->px) >= cost->uses)
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }

        if (cost->uses == 1)
            strnfmt(buf, sizeof(buf), "%d Use (of %d)", cost->uses,
                forge_uses_fn(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "%d Uses (of %d)", cost->uses,
                forge_uses_fn(p_ptr->py, p_ptr->px));
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->drain > 0)
    {
        if (cost->drain <= p_ptr->skill_base[S_SMT])
            attr = TERM_BLUE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Smithing", cost->drain);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->mithril > 0)
    {
        if (cost->mithril <= mithril_carried_fn())
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d.%d lb Mithril", cost->mithril / 10,
            cost->mithril % 10);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->str > 0)
    {
        if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - cost->str
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Str", cost->str);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->dex > 0)
    {
        if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - cost->dex
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Dex", cost->dex);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->con > 0)
    {
        if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - cost->con
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Con", cost->con);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->gra > 0)
    {
        if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - cost->gra
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Gra", cost->gra);
        ui_text_builder_append_line(builder, buf, attr);
    }
    if (cost->exp > 0)
    {
        if (p_ptr->new_exp >= cost->exp)
            attr = TERM_SLATE;
        else
        {
            attr = TERM_L_DARK;
            can_afford = FALSE;
        }
        strnfmt(buf, sizeof(buf), "%d Exp", cost->exp);
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

/* Starts one smithing menu text block with an optional title line. */
void ui_smithing_menu_text_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size, cptr title)
{
    ui_text_builder_init(builder, text, attrs, size);

    if (title && title[0])
    {
        ui_text_builder_append_line(builder, title, TERM_WHITE);
        ui_text_builder_newline(builder, TERM_WHITE);
    }
}

/* Publishes the current smithing menu text and details blocks. */
void ui_smithing_menu_publish(const ui_text_builder* menu_builder,
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

/* Appends the current smithing item summary and cost details. */
void ui_smithing_menu_append_details(ui_text_builder* builder,
    object_type* smith_item, const smithing_cost_type* cost,
    int (*too_difficult_fn)(object_type* o_ptr),
    int (*forge_uses_fn)(int y, int x), int (*mithril_carried_fn)(void),
    int (*forge_bonus_fn)(int y, int x))
{
    char o_desc[160];
    char buf[160];
    int display_flag;
    int dif;
    byte dif_attr;
    void (*old_text_out_hook)(byte a, cptr str);
    int old_text_out_indent;
    int old_text_out_wrap;

    if (!builder || !smith_item || !cost || !too_difficult_fn
        || !forge_uses_fn || !mithril_carried_fn || !forge_bonus_fn)
        return;

    if (p_ptr->smithing_leftover)
    {
        ui_text_builder_append_line(builder, "In progress:", TERM_L_BLUE);
        strnfmt(buf, sizeof(buf), "%3d turns left", p_ptr->smithing_leftover);
        ui_text_builder_append_line(builder, buf, TERM_BLUE);
        ui_text_builder_newline(builder, TERM_WHITE);
    }

    if (smith_item->tval == 0)
        return;

    display_flag = (smith_item->number > 1) ? TRUE : FALSE;
    object_desc(o_desc, sizeof(o_desc), smith_item, display_flag, 2);
    my_strcat(o_desc,
        format("   %d.%d lb", smith_item->weight * smith_item->number / 10,
            (smith_item->weight * smith_item->number) % 10),
        sizeof(o_desc));

    ui_text_builder_append_line(builder, o_desc, TERM_L_WHITE);

    dif = object_difficulty(smith_item);
    dif_attr = too_difficult_fn(smith_item) ? TERM_L_DARK : TERM_SLATE;
    if ((cost->drain > 0)
        && (cost->drain <= p_ptr->skill_base[S_SMT]))
    {
        dif_attr = TERM_BLUE;
    }

    strnfmt(buf, sizeof(buf), "Difficulty: %d (max %d)", dif,
        p_ptr->skill_use[S_SMT] + forge_bonus_fn(p_ptr->py, p_ptr->px));
    ui_text_builder_append_line(builder, buf, dif_attr);
    ui_smithing_append_requirement_details(
        builder, cost, dif, forge_uses_fn, mithril_carried_fn);
    ui_text_builder_newline(builder, TERM_WHITE);

    old_text_out_hook = text_out_hook;
    old_text_out_indent = text_out_indent;
    old_text_out_wrap = text_out_wrap;

    object_info_out_flags = object_flags;
    ui_smithing_builder = builder;
    text_out_hook = ui_smithing_text_out_to_builder;
    text_out_indent = 0;
    text_out_wrap = 0;

    if ((k_text + k_info[smith_item->k_idx].text)[0] != '\0')
    {
        text_out_c(TERM_WHITE, k_text + k_info[smith_item->k_idx].text);
        text_out(" ");
    }

    if (object_info_out(smith_item))
        text_out("\n");

    text_out_hook = old_text_out_hook;
    text_out_indent = old_text_out_indent;
    text_out_wrap = old_text_out_wrap;
    ui_smithing_builder = NULL;
}
