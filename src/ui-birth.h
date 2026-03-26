/* File: ui-birth.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Semantic character-creation menus shared by all frontends.
 */

#ifndef INCLUDED_UI_BIRTH_H
#define INCLUDED_UI_BIRTH_H

typedef void (*ui_birth_stats_render_hook)(void);
typedef enum ui_birth_history_prompt_result ui_birth_history_prompt_result;
typedef enum ui_birth_ahw_prompt_result ui_birth_ahw_prompt_result;

#define UI_BIRTH_HISTORY_TEXT_MAX 1024
#define UI_BIRTH_NAME_TEXT_MAX 32
#define UI_BIRTH_CHOICE_CANCELLED (-1)

enum ui_birth_screen_kind
{
    UI_BIRTH_SCREEN_NONE = 0,
    UI_BIRTH_SCREEN_STATS,
    UI_BIRTH_SCREEN_HISTORY,
    UI_BIRTH_SCREEN_AHW,
    UI_BIRTH_SCREEN_NAME
};

enum ui_birth_history_prompt_result
{
    UI_BIRTH_HISTORY_PROMPT_CANCEL = 0,
    UI_BIRTH_HISTORY_PROMPT_ACCEPT,
    UI_BIRTH_HISTORY_PROMPT_REROLL
};

enum ui_birth_ahw_prompt_result
{
    UI_BIRTH_AHW_PROMPT_CANCEL = 0,
    UI_BIRTH_AHW_PROMPT_ACCEPT,
    UI_BIRTH_AHW_PROMPT_REROLL
};

enum ui_birth_prompt_result
{
    UI_BIRTH_PROMPT_CANCEL = 0,
    UI_BIRTH_PROMPT_ACCEPT,
    UI_BIRTH_PROMPT_REROLL
};

typedef enum ui_birth_prompt_result (*ui_birth_prompt_hook)(
    enum ui_birth_screen_kind kind, char* text, size_t text_size, int* age,
    int* height, int* weight, int age_min, int age_max, int height_min,
    int height_max, int weight_min, int weight_max);

typedef bool (*ui_birth_submit_hook)(enum ui_birth_screen_kind kind,
    char* text, size_t text_size, int* age, int* height, int* weight);

struct ui_birth_stat_snapshot
{
    cptr name;
    cptr value;
    int cost;
    bool selected;
    bool can_decrease;
    bool can_increase;
};

struct ui_birth_state_snapshot
{
    bool active;
    enum ui_birth_screen_kind kind;
    cptr title;
    cptr help;
    cptr text;
    int max_length;
    int age;
    int height;
    int weight;
    int age_min;
    int age_max;
    int height_min;
    int height_max;
    int weight_min;
    int weight_max;
    int points_left;
    int stats_count;
    struct ui_birth_stat_snapshot stats[A_MAX];
};

/* Returns the point-buy cost for one raw birth stat value. */
int ui_birth_stat_cost_for_value(int value);
/* Returns the total point budget available during birth stat allocation. */
int ui_birth_stat_budget(void);
/* Opens the race-selection menu and returns the chosen race or cancel. */
int ui_birth_choose_race(int current_race);
/* Opens the house-selection menu and returns the chosen house or cancel. */
int ui_birth_choose_house(int race, int current_house);
/* Registers the frontend hook used to render the shared stat-allocation screen. */
void ui_birth_set_stats_render_hook(ui_birth_stats_render_hook render_hook);
/* Registers the frontend hook used to edit one shared birth draft. */
void ui_birth_set_prompt_hook(ui_birth_prompt_hook prompt_hook);
/* Registers the frontend hook used to submit one accepted shared birth draft. */
void ui_birth_set_submit_hook(ui_birth_submit_hook submit_hook);
/* Runs one shared history prompt and updates the working text in place. */
ui_birth_history_prompt_result ui_birth_prompt_history(
    char* history, size_t size);
/* Runs one shared age/height/weight prompt and updates the working values. */
ui_birth_ahw_prompt_result ui_birth_prompt_ahw(int* age, int* height,
    int* weight, int age_min, int age_max, int height_min, int height_max,
    int weight_min, int weight_max);
/* Runs one shared naming prompt and updates the working name in place. */
bool ui_birth_prompt_name(char* name, size_t size);
/* Publishes the current shared stat-allocation screen state. */
void ui_birth_publish_stats_screen(const int* stats, int selected);
/* Clears the exported stat-allocation screen state. */
void ui_birth_clear_stats_screen(void);
/* Publishes the current shared history-editing screen state. */
void ui_birth_publish_history_screen(cptr history);
/* Clears the exported history-editing screen state. */
void ui_birth_clear_history_screen(void);
/* Publishes the current shared age/height/weight screen state. */
void ui_birth_publish_ahw_screen(int age, int height, int weight, int age_min,
    int age_max, int height_min, int height_max, int weight_min, int weight_max);
/* Clears the exported age/height/weight screen state. */
void ui_birth_clear_ahw_screen(void);
/* Publishes the current shared name-editing screen state. */
void ui_birth_publish_name_screen(cptr name);
/* Clears the exported name-editing screen state. */
void ui_birth_clear_name_screen(void);
/* Asks the active frontend to render the current stat-allocation screen. */
void ui_birth_render_stats_screen(void);
/* Returns the current shared birth-screen revision. */
unsigned int ui_birth_revision(void);
/* Copies the current semantic birth-screen state into one snapshot. */
void ui_birth_get_state_snapshot(struct ui_birth_state_snapshot* snapshot);

#endif /* INCLUDED_UI_BIRTH_H */
