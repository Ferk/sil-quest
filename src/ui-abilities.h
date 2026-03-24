/* File: ui-abilities.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Ability and song menu presentation helpers.
 */

#ifndef INCLUDED_UI_ABILITIES_H
#define INCLUDED_UI_ABILITIES_H

#include "angband.h"

#include "ui-model.h"

/* Returns the preferred terminal attr for one ability menu entry. */
byte ui_ability_menu_attr(int skilltype, ability_type* b_ptr,
    bool (*prereqs_fn)(int skilltype, int abilitynum));
/* Formats the visible label for one ability menu entry. */
void ui_ability_menu_label(
    int skilltype, ability_type* b_ptr, char* buf, size_t buf_size);
/* Builds the semantic details text for one ability menu entry. */
int ui_ability_menu_build_details(int skilltype, ability_type* b_ptr, byte attr,
    char* text, byte* attrs, size_t size,
    bool (*prereqs_fn)(int skilltype, int abilitynum),
    int (*abilities_in_skill_fn)(int skilltype),
    int (*bane_type_killed_fn)(int bane_type),
    int (*bane_bonus_aux_fn)(void));
/* Picks the default highlight for the change-song menu. */
int ui_song_menu_default_highlight(void);
/* Builds the extra status text shown beside the change-song menu. */
void ui_song_menu_build_extra_details(char* details, size_t details_size);
/* Builds the simple-menu entries used by the change-song menu. */
int ui_song_menu_build_entries(ui_simple_menu_entry* entries, char labels[][80],
    char details[][1024], int max_entries);

#endif
