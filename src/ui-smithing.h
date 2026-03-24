/* File: ui-smithing.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Smithing menu presentation helpers.
 */

#ifndef INCLUDED_UI_SMITHING_H
#define INCLUDED_UI_SMITHING_H

#include "angband.h"

#include "ui-model.h"

typedef struct smithing_cost_type smithing_cost_type;

struct smithing_cost_type
{
    int str;
    int dex;
    int con;
    int gra;
    int exp;
    int smt;
    int mithril;
    int uses;
    int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artifice;
};

/* Starts one smithing menu text block with an optional title line. */
void ui_smithing_menu_text_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size, cptr title);
/* Publishes the current smithing menu text and details blocks. */
void ui_smithing_menu_publish(const ui_text_builder* menu_builder,
    const ui_text_builder* details_builder);
/* Appends the current smithing item summary and cost details. */
void ui_smithing_menu_append_details(ui_text_builder* builder,
    object_type* smith_item, const smithing_cost_type* cost,
    int (*too_difficult_fn)(object_type* o_ptr),
    int (*forge_uses_fn)(int y, int x), int (*mithril_carried_fn)(void),
    int (*forge_bonus_fn)(int y, int x));

#endif
