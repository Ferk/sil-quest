/* File: ui-character.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Semantic character-sheet state shared by terminal and web frontends.
 */

#ifndef INCLUDED_UI_CHARACTER_H
#define INCLUDED_UI_CHARACTER_H

#include "angband.h"

#define UI_CHARACTER_FIELD_COUNT 8
#define UI_CHARACTER_COMBAT_COUNT 8
#define UI_CHARACTER_ACTION_COUNT 6

typedef struct ui_character_field ui_character_field;
typedef struct ui_character_stat ui_character_stat;
typedef struct ui_character_skill ui_character_skill;
typedef struct ui_character_action ui_character_action;
typedef struct ui_character_sheet_state ui_character_sheet_state;
typedef void (*ui_character_render_hook)(void);

struct ui_character_field
{
    char label[24];
    char value[32];
    bool visible;
};

struct ui_character_stat
{
    char label[8];
    char current[16];
    char base[16];
    int equip_mod;
    int drain_mod;
    int misc_mod;
    bool reduced;
    bool show_base;
    bool show_equip;
    bool show_drain;
    bool show_misc;
};

struct ui_character_skill
{
    char label[24];
    int value;
    int base;
    int stat_mod;
    int equip_mod;
    int misc_mod;
    int cost;
    bool editable;
    bool selected;
    bool can_decrease;
    bool can_increase;
};

struct ui_character_action
{
    int key;
    char label[32];
};

struct ui_character_sheet_state
{
    bool active;
    bool skill_editor_active;
    char title[32];
    char skill_editor_title[40];
    char skill_editor_help[128];
    int skill_points_left;
    ui_character_field identity[3];
    ui_character_field physical[3];
    ui_character_field progress[UI_CHARACTER_FIELD_COUNT];
    ui_character_stat stats[A_MAX];
    ui_character_field combat[UI_CHARACTER_COMBAT_COUNT];
    ui_character_skill skills[S_MAX];
    char history[1024];
    ui_character_action actions[UI_CHARACTER_ACTION_COUNT];
    int action_count;
};

/* Registers the frontend hook used to render the shared character sheet. */
void ui_character_set_render_hook(ui_character_render_hook render_hook);
/* Returns the total experience cost for one temporary skill increase. */
int ui_character_skill_gain_cost(int base, int points);
/* Publishes the current semantic character-sheet state. */
void ui_character_publish_sheet(void);
/* Publishes skill-allocation metadata on top of the current character sheet. */
void ui_character_publish_skill_editor(const int* old_base,
    const int* skill_gain, int selected, int points_left);
/* Clears any active skill-allocation metadata from the shared character sheet. */
void ui_character_clear_skill_editor(void);
/* Clears the exported semantic character-sheet state. */
void ui_character_clear_sheet(void);
/* Asks the active frontend to render the current character sheet. */
void ui_character_render_sheet(void);
/* Returns whether the shared character sheet is currently active. */
bool ui_character_sheet_active(void);
/* Returns whether the shared character sheet currently carries skill editor state. */
bool ui_character_skill_editor_active(void);
/* Returns the current character-sheet revision. */
unsigned int ui_character_sheet_revision(void);
/* Returns the current semantic character-sheet payload. */
const ui_character_sheet_state* ui_character_get_sheet(void);

#endif /* INCLUDED_UI_CHARACTER_H */
