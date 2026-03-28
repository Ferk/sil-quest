/* File: ui-birth.c
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

#include "angband.h"

#include "ui-birth.h"
#include "ui-character.h"
#include "ui-input.h"
#include "ui-model.h"

#define UI_BIRTH_MENU_TEXT_MAX 2048
#define UI_BIRTH_DETAILS_TEXT_MAX 4096
#define UI_BIRTH_STATS_VALUE_MAX 16
#define UI_BIRTH_RACE_DETAILS_WIDTH 38
#define UI_BIRTH_RACE_DETAILS_ROWS 12
#define UI_BIRTH_RACE_SUMMARY_ROWS 8
#define UI_BIRTH_HOUSE_DETAILS_WIDTH 30
#define UI_BIRTH_HOUSE_DETAILS_ROWS 12
#define UI_BIRTH_HOUSE_SUMMARY_ROWS 8
#define UI_BIRTH_STAT_MIN 0
#define UI_BIRTH_STAT_MAX 6
#define UI_BIRTH_STAT_BUDGET 13

struct ui_birth_stats_state
{
    bool active;
    int selected;
    int points_left;
    int costs[A_MAX];
    bool can_decrease[A_MAX];
    bool can_increase[A_MAX];
    char display_values[A_MAX][UI_BIRTH_STATS_VALUE_MAX];
};

struct ui_birth_history_state
{
    bool active;
    char text[UI_BIRTH_HISTORY_TEXT_MAX];
};

struct ui_birth_ahw_state
{
    bool active;
    int age;
    int height;
    int weight;
    int age_min;
    int age_max;
    int height_min;
    int height_max;
    int weight_min;
    int weight_max;
};

struct ui_birth_name_state
{
    bool active;
    char text[UI_BIRTH_NAME_TEXT_MAX];
};

struct ui_birth_trait_def
{
    u32b flag;
    cptr label;
    byte attr;
};

struct ui_birth_mastery_def
{
    u32b flag;
    cptr label;
    cptr mastery_label;
};

struct ui_birth_skill_def
{
    u32b affinity_flag;
    u32b penalty_flag;
    cptr affinity_label;
    cptr mastery_label;
    cptr penalty_label;
};

static const int ui_birth_stat_costs[] = { 0, 1, 3, 6, 10, 15, 21 };
static const char ui_birth_stats_title_text[] = "Allocate your attributes";
static const char ui_birth_history_title_text[] = "Shape your history";
static const char ui_birth_history_help_text[]
    = "Edit the text directly, use Reroll for a random history, Accept when "
      "you are happy, or Escape to go back.";
static const char ui_birth_ahw_title_text[] = "Set your age, height, and weight";
static const char ui_birth_ahw_help_text[]
    = "Use Reroll for fresh random values, adjust anything you want, and "
      "Accept when you are happy.";
static const char ui_birth_name_title_text[] = "Choose your name";
static const char ui_birth_name_help_text[]
    = "Edit the name directly, use Tab for a random one, Accept when you are "
      "happy, or Escape to go back.";
static struct ui_birth_stats_state ui_birth_stats_state = { 0 };
static struct ui_birth_history_state ui_birth_history_state = { 0 };
static struct ui_birth_ahw_state ui_birth_ahw_state = { 0 };
static struct ui_birth_name_state ui_birth_name_state = { 0 };
static unsigned int ui_birth_state_value_revision = 1;
static ui_birth_stats_render_hook ui_birth_front_stats_render = NULL;
static ui_birth_prompt_hook ui_birth_front_prompt = NULL;
static ui_birth_submit_hook ui_birth_front_submit = NULL;

/* Bumps the shared birth-screen revision. */
static void ui_birth_touch(void)
{
    ui_birth_state_value_revision++;
    if (ui_birth_state_value_revision == 0)
        ui_birth_state_value_revision = 1;
    ui_front_invalidate();
}

/* Redraws the shared character-sheet context before one birth editor prompt. */
static void ui_birth_render_character_sheet_context(void)
{
    p_ptr->redraw |= PR_MISC;
    display_player(0);
    ui_character_publish_sheet();
    ui_character_render_sheet();
}

/* Clears the shared character-sheet overlay after one birth editor prompt. */
static void ui_birth_clear_character_sheet_context(void)
{
    ui_character_clear_sheet();
}

/* Returns whether one raw birth stat value can be represented safely. */
static bool ui_birth_stat_value_valid(int value)
{
    return (value >= UI_BIRTH_STAT_MIN) && (value <= UI_BIRTH_STAT_MAX);
}

/* Returns the total point-buy cost for one set of raw birth stats. */
static int ui_birth_stats_total_cost(const int* stats)
{
    int i;
    int cost = 0;

    if (!stats)
        return 0;

    for (i = 0; i < A_MAX; i++)
        cost += ui_birth_stat_cost_for_value(stats[i]);

    return cost;
}

/* Returns whether one exported stat row index is valid. */
static bool ui_birth_stats_index_valid(int index)
{
    return (index >= 0) && (index < A_MAX);
}

/* Returns whether one raw stat can be increased without breaking the budget. */
static bool ui_birth_stats_value_can_increase(
    const int* stats, int index, int total_cost)
{
    int next_value;
    int next_cost;

    if (!stats || !ui_birth_stats_index_valid(index))
        return FALSE;

    next_value = stats[index] + 1;
    if (!ui_birth_stat_value_valid(next_value))
        return FALSE;

    next_cost = total_cost - ui_birth_stat_cost_for_value(stats[index])
        + ui_birth_stat_cost_for_value(next_value);

    return next_cost <= ui_birth_stat_budget();
}

/* Returns the point-buy cost for one raw birth stat value. */
int ui_birth_stat_cost_for_value(int value)
{
    if (value < UI_BIRTH_STAT_MIN)
        value = UI_BIRTH_STAT_MIN;
    else if (value > UI_BIRTH_STAT_MAX)
        value = UI_BIRTH_STAT_MAX;

    return ui_birth_stat_costs[value];
}

/* Returns the total point budget available during birth stat allocation. */
int ui_birth_stat_budget(void)
{
    return UI_BIRTH_STAT_BUDGET;
}

/* Returns the semantic hotkey for one birth-menu entry index. */
static int ui_birth_choice_key(int index)
{
    if (index < 26)
        return I2A(index);

    return 'A' + (index - 26);
}

/* Returns the display label for one birth-menu entry index. */
static void ui_birth_choice_label(char* buf, size_t size, int index, cptr name)
{
    int key = ui_birth_choice_key(index);

    strnfmt(buf, size, "%c) %s", key, name ? name : "");
}

/* Maps one race stat modifier to the color used in the details pane. */
static byte ui_birth_stat_attr(int adj)
{
    if (adj < 0)
        return TERM_RED;
    if (adj == 0)
        return TERM_L_DARK;
    if (adj == 1)
        return TERM_GREEN;
    if (adj == 2)
        return TERM_L_GREEN;

    return TERM_L_BLUE;
}

/* Appends one stat-modifier table for the selected race. */
static void ui_birth_append_race_stats(ui_text_builder* builder, int race)
{
    size_t i;

    ui_text_builder_append_line(builder, "Stat modifiers", TERM_L_BLUE);

    for (i = 0; i < A_MAX; i++)
    {
        char value[8];

        strnfmt(value, sizeof(value), "%+d", p_info[race].r_adj[i]);
        ui_text_builder_append(builder, stat_names[i], TERM_WHITE);
        ui_text_builder_append(builder, "  ", TERM_WHITE);
        ui_text_builder_append(
            builder, value, ui_birth_stat_attr(p_info[race].r_adj[i]));
        ui_text_builder_newline(builder, TERM_WHITE);
    }
}

/* Appends one trait list for the selected race. */
static void ui_birth_append_race_traits(ui_text_builder* builder, int race)
{
    static const struct ui_birth_trait_def traits[] = {
        { RHF_BOW_PROFICIENCY, "Bow proficiency", TERM_GREEN },
        { RHF_AXE_PROFICIENCY, "Axe proficiency", TERM_GREEN },
        { RHF_MEL_AFFINITY, "Melee affinity", TERM_GREEN },
        { RHF_MEL_PENALTY, "Melee penalty", TERM_RED },
        { RHF_ARC_AFFINITY, "Archery affinity", TERM_GREEN },
        { RHF_ARC_PENALTY, "Archery penalty", TERM_RED },
        { RHF_EVN_AFFINITY, "Evasion affinity", TERM_GREEN },
        { RHF_EVN_PENALTY, "Evasion penalty", TERM_RED },
        { RHF_STL_AFFINITY, "Stealth affinity", TERM_GREEN },
        { RHF_STL_PENALTY, "Stealth penalty", TERM_RED },
        { RHF_PER_AFFINITY, "Perception affinity", TERM_GREEN },
        { RHF_PER_PENALTY, "Perception penalty", TERM_RED },
        { RHF_WIL_AFFINITY, "Will affinity", TERM_GREEN },
        { RHF_WIL_PENALTY, "Will penalty", TERM_RED },
        { RHF_SMT_AFFINITY, "Smithing affinity", TERM_GREEN },
        { RHF_SMT_PENALTY, "Smithing penalty", TERM_RED },
        { RHF_SNG_AFFINITY, "Song affinity", TERM_GREEN },
        { RHF_SNG_PENALTY, "Song penalty", TERM_RED }
    };
    size_t i;
    bool any = FALSE;

    ui_text_builder_append_line(builder, "Traits", TERM_L_BLUE);

    for (i = 0; i < N_ELEMENTS(traits); i++)
    {
        if (!(p_info[race].flags & traits[i].flag))
            continue;

        ui_text_builder_append_line(builder, traits[i].label, traits[i].attr);
        any = TRUE;
    }

    if (!any)
        ui_text_builder_append_line(builder, "No innate traits", TERM_L_DARK);
}

/* Collects the legal houses for one race and returns how many were found. */
static int ui_birth_collect_house_choices(int race, int* houses, int max_houses)
{
    int i;
    int count = 0;
    s16b choice;

    if ((race < 0) || (race >= z_info->p_max) || !houses || (max_houses <= 0))
        return 0;

    choice = p_info[race].choice;

    for (i = 0; (i < z_info->c_max) && (count < max_houses); i++)
    {
        if (!(choice & (1L << i)))
            continue;

        houses[count++] = i;
    }

    return count;
}

/* Appends the total stat modifiers from the selected race plus house. */
static void ui_birth_append_house_totals(
    ui_text_builder* builder, int race, int house)
{
    size_t i;

    ui_text_builder_append_line(builder, "Total modifiers", TERM_L_BLUE);

    for (i = 0; i < A_MAX; i++)
    {
        char value[8];
        int total = p_info[race].r_adj[i] + c_info[house].h_adj[i];

        strnfmt(value, sizeof(value), "%+d", total);
        ui_text_builder_append(builder, stat_names[i], TERM_WHITE);
        ui_text_builder_append(builder, "  ", TERM_WHITE);
        ui_text_builder_append(builder, value, ui_birth_stat_attr(total));
        ui_text_builder_newline(builder, TERM_WHITE);
    }
}

/* Appends the combined race-plus-house traits for the selected house. */
static void ui_birth_append_house_traits(
    ui_text_builder* builder, int race, int house)
{
    static const struct ui_birth_mastery_def proficiencies[] = {
        { RHF_BOW_PROFICIENCY, "Bow proficiency", "Bow mastery" },
        { RHF_AXE_PROFICIENCY, "Axe proficiency", "Axe mastery" }
    };
    static const struct ui_birth_skill_def skills[] = {
        { RHF_MEL_AFFINITY, RHF_MEL_PENALTY, "Melee affinity", "Melee mastery",
            "Melee penalty" },
        { RHF_ARC_AFFINITY, RHF_ARC_PENALTY, "Archery affinity",
            "Archery mastery", "Archery penalty" },
        { RHF_EVN_AFFINITY, RHF_EVN_PENALTY, "Evasion affinity",
            "Evasion mastery", "Evasion penalty" },
        { RHF_STL_AFFINITY, RHF_STL_PENALTY, "Stealth affinity",
            "Stealth mastery", "Stealth penalty" },
        { RHF_PER_AFFINITY, RHF_PER_PENALTY, "Perception affinity",
            "Perception mastery", "Perception penalty" },
        { RHF_WIL_AFFINITY, RHF_WIL_PENALTY, "Will affinity", "Will mastery",
            "Will penalty" },
        { RHF_SMT_AFFINITY, RHF_SMT_PENALTY, "Smithing affinity",
            "Smithing mastery", "Smithing penalty" },
        { RHF_SNG_AFFINITY, RHF_SNG_PENALTY, "Song affinity", "Song mastery",
            "Song penalty" }
    };
    u32b race_flags = p_info[race].flags;
    u32b house_flags = c_info[house].flags;
    size_t i;
    bool any = FALSE;

    ui_text_builder_append_line(builder, "Combined traits", TERM_L_BLUE);

    for (i = 0; i < N_ELEMENTS(proficiencies); i++)
    {
        bool race_has = (race_flags & proficiencies[i].flag) ? TRUE : FALSE;
        bool house_has = (house_flags & proficiencies[i].flag) ? TRUE : FALSE;

        if (race_has && house_has)
        {
            ui_text_builder_append_line(
                builder, proficiencies[i].mastery_label, TERM_L_GREEN);
            any = TRUE;
        }
        else if (race_has || house_has)
        {
            ui_text_builder_append_line(
                builder, proficiencies[i].label, TERM_GREEN);
            any = TRUE;
        }
    }

    for (i = 0; i < N_ELEMENTS(skills); i++)
    {
        bool race_affinity = (race_flags & skills[i].affinity_flag) ? TRUE : FALSE;
        bool house_affinity = (house_flags & skills[i].affinity_flag) ? TRUE : FALSE;
        bool has_penalty =
            (race_flags & skills[i].penalty_flag) || (house_flags & skills[i].penalty_flag);

        if (race_affinity && house_affinity)
        {
            ui_text_builder_append_line(
                builder, skills[i].mastery_label, TERM_L_GREEN);
            any = TRUE;
        }
        else if (race_affinity || house_affinity)
        {
            ui_text_builder_append_line(
                builder, skills[i].affinity_label, TERM_GREEN);
            any = TRUE;
        }

        if (has_penalty)
        {
            ui_text_builder_append_line(
                builder, skills[i].penalty_label, TERM_RED);
            any = TRUE;
        }
    }

    if (!any)
        ui_text_builder_append_line(builder, "No house modifiers", TERM_L_DARK);
}

/* Publishes the semantic race menu and its details for the current selection. */
static void ui_birth_publish_race_menu(int current_race)
{
    char menu_text[UI_BIRTH_MENU_TEXT_MAX];
    byte menu_attrs[UI_BIRTH_MENU_TEXT_MAX];
    char details_text[UI_BIRTH_DETAILS_TEXT_MAX];
    byte details_attrs[UI_BIRTH_DETAILS_TEXT_MAX];
    char summary_text[UI_BIRTH_DETAILS_TEXT_MAX];
    byte summary_attrs[UI_BIRTH_DETAILS_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    ui_text_builder summary_builder;
    int i;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));
    ui_text_builder_init(
        &summary_builder, summary_text, summary_attrs, sizeof(summary_text));

    ui_text_builder_append_line(&menu_builder, "Choose your race", TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Press * for random, O for options, or Escape to go back.", TERM_SLATE);

    ui_text_builder_append_line(
        &details_builder, p_name + p_info[current_race].name, TERM_YELLOW);
    ui_text_builder_newline(&details_builder, TERM_WHITE);
    ui_birth_append_race_stats(&details_builder, current_race);
    ui_text_builder_newline(&details_builder, TERM_WHITE);
    ui_birth_append_race_traits(&details_builder, current_race);

    if (p_text[p_info[current_race].text] != '\0')
    {
        ui_text_builder_append_line(&summary_builder, "Description", TERM_L_BLUE);
        ui_text_builder_append(
            &summary_builder, p_text + p_info[current_race].text, TERM_SLATE);
    }

    ui_menu_begin();
    ui_menu_set_text(menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details_width(UI_BIRTH_RACE_DETAILS_WIDTH);
    ui_menu_set_details_rows(UI_BIRTH_RACE_DETAILS_ROWS);
    ui_menu_set_summary_rows(UI_BIRTH_RACE_SUMMARY_ROWS);
    ui_menu_set_details_visual(UI_MENU_VISUAL_NONE, TERM_WHITE, 0);

    for (i = 0; i < z_info->p_max; i++)
    {
        char label[80];

        ui_birth_choice_label(label, sizeof(label), i, p_name + p_info[i].name);
        ui_menu_add(0, i + 4, (int)strlen(label), 1, ui_birth_choice_key(i),
            i == current_race, TERM_WHITE, label);
    }

    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_set_summary(
        summary_text, summary_attrs, ui_text_builder_length(&summary_builder));
    ui_menu_end();
}

/* Publishes the semantic house menu and its details for the current selection. */
static void ui_birth_publish_house_menu(
    int race, const int* houses, int house_count, int selection)
{
    char menu_text[UI_BIRTH_MENU_TEXT_MAX];
    byte menu_attrs[UI_BIRTH_MENU_TEXT_MAX];
    char details_text[UI_BIRTH_DETAILS_TEXT_MAX];
    byte details_attrs[UI_BIRTH_DETAILS_TEXT_MAX];
    char summary_text[UI_BIRTH_DETAILS_TEXT_MAX];
    byte summary_attrs[UI_BIRTH_DETAILS_TEXT_MAX];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;
    ui_text_builder summary_builder;
    int current_house = houses[selection];
    int i;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, details_text, details_attrs, sizeof(details_text));
    ui_text_builder_init(
        &summary_builder, summary_text, summary_attrs, sizeof(summary_text));

    ui_text_builder_append_line(&menu_builder, "Choose your house", TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        p_name + p_info[race].name, TERM_YELLOW);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Press * for random, O for options, or Escape to go back.", TERM_SLATE);

    ui_text_builder_append_line(
        &details_builder, c_name + c_info[current_house].name, TERM_YELLOW);
    ui_text_builder_newline(&details_builder, TERM_WHITE);
    ui_birth_append_house_totals(&details_builder, race, current_house);
    ui_text_builder_newline(&details_builder, TERM_WHITE);
    ui_birth_append_house_traits(&details_builder, race, current_house);

    if (c_text[c_info[current_house].text] != '\0')
    {
        ui_text_builder_append_line(&summary_builder, "Description", TERM_L_BLUE);
        ui_text_builder_append(
            &summary_builder, c_text + c_info[current_house].text, TERM_SLATE);
    }

    ui_menu_begin();
    ui_menu_set_text(menu_text, menu_attrs, ui_text_builder_length(&menu_builder));
    ui_menu_set_details_width(UI_BIRTH_HOUSE_DETAILS_WIDTH);
    ui_menu_set_details_rows(UI_BIRTH_HOUSE_DETAILS_ROWS);
    ui_menu_set_summary_rows(UI_BIRTH_HOUSE_SUMMARY_ROWS);
    ui_menu_set_details_visual(UI_MENU_VISUAL_NONE, TERM_WHITE, 0);

    for (i = 0; i < house_count; i++)
    {
        char label[80];
        int house = houses[i];

        ui_birth_choice_label(label, sizeof(label), i, c_name + c_info[house].name);
        ui_menu_add(0, i + 5, (int)strlen(label), 1, ui_birth_choice_key(i),
            i == selection, TERM_WHITE, label);
    }

    ui_menu_set_details(
        details_text, details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_set_summary(
        summary_text, summary_attrs, ui_text_builder_length(&summary_builder));
    ui_menu_end();
}

/* Clears the semantic birth menu so the next screen can redraw cleanly. */
static void ui_birth_clear_menu(void)
{
    ui_menu_clear();
    ui_menu_render_current();
}

/* Registers the frontend hook used to render the shared stat-allocation screen. */
void ui_birth_set_stats_render_hook(ui_birth_stats_render_hook render_hook)
{
    ui_birth_front_stats_render = render_hook;
}

/* Registers the frontend hook used to edit one shared birth draft. */
void ui_birth_set_prompt_hook(ui_birth_prompt_hook prompt_hook)
{
    ui_birth_front_prompt = prompt_hook;
}

/* Registers the frontend hook used to submit one accepted shared birth draft. */
void ui_birth_set_submit_hook(ui_birth_submit_hook submit_hook)
{
    ui_birth_front_submit = submit_hook;
}

/* Submits one accepted shared birth draft through the active frontend hook. */
static bool ui_birth_submit(enum ui_birth_screen_kind kind, char* text,
    size_t text_size, int* age, int* height, int* weight)
{
    return !ui_birth_front_submit
        || ui_birth_front_submit(kind, text, text_size, age, height, weight);
}

/* Runs one shared history prompt and updates the working text in place. */
ui_birth_history_prompt_result ui_birth_prompt_history(
    char* history, size_t size)
{
    if (ui_birth_front_prompt)
        return (ui_birth_history_prompt_result)ui_birth_front_prompt(
            UI_BIRTH_SCREEN_HISTORY, history, size, NULL, NULL, NULL, 0, 0, 0,
            0, 0, 0);

    while (TRUE)
    {
        char query2;

        ui_birth_render_character_sheet_context();
        ui_birth_publish_history_screen(history);

        hide_cursor = TRUE;
        query2 = inkey();
        hide_cursor = FALSE;

        if (ui_input_is_accept_key(query2))
        {
            if (!ui_birth_submit(
                    UI_BIRTH_SCREEN_HISTORY, history, size, NULL, NULL, NULL))
                continue;
            p_ptr->redraw |= PR_MISC;
            ui_birth_clear_history_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_HISTORY_PROMPT_ACCEPT;
        }

        if (query2 == ' ')
        {
            ui_birth_clear_history_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_HISTORY_PROMPT_REROLL;
        }

        if (ui_input_is_cancel_key(query2))
        {
            ui_birth_clear_history_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_HISTORY_PROMPT_CANCEL;
        }

        if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }
}

/* Runs one shared age/height/weight prompt and updates the working values. */
ui_birth_ahw_prompt_result ui_birth_prompt_ahw(int* age, int* height,
    int* weight, int age_min, int age_max, int height_min, int height_max,
    int weight_min, int weight_max)
{
    int current_age = age ? *age : 0;
    int current_height = height ? *height : 0;
    int current_weight = weight ? *weight : 0;

    if (ui_birth_front_prompt)
    {
        ui_birth_ahw_prompt_result result
            = (ui_birth_ahw_prompt_result)ui_birth_front_prompt(
                UI_BIRTH_SCREEN_AHW, NULL, 0, &current_age, &current_height,
                &current_weight, age_min, age_max, height_min, height_max,
                weight_min, weight_max);

        if (result == UI_BIRTH_AHW_PROMPT_ACCEPT)
        {
            if (age)
                *age = current_age;
            if (height)
                *height = current_height;
            if (weight)
                *weight = current_weight;
        }

        return result;
    }

    while (TRUE)
    {
        char query2;

        ui_birth_render_character_sheet_context();
        ui_birth_publish_ahw_screen(current_age, current_height, current_weight,
            age_min, age_max, height_min, height_max, weight_min, weight_max);

        hide_cursor = TRUE;
        query2 = inkey();
        hide_cursor = FALSE;

        if (ui_input_is_accept_key(query2))
        {
            if (!ui_birth_submit(UI_BIRTH_SCREEN_AHW, NULL, 0, &current_age,
                    &current_height, &current_weight))
                continue;
            if (age)
                *age = current_age;
            if (height)
                *height = current_height;
            if (weight)
                *weight = current_weight;
            ui_birth_clear_ahw_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_AHW_PROMPT_ACCEPT;
        }

        if (query2 == ' ')
        {
            ui_birth_clear_ahw_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_AHW_PROMPT_REROLL;
        }

        if (ui_input_is_cancel_key(query2))
        {
            ui_birth_clear_ahw_screen();
            ui_birth_clear_character_sheet_context();
            return UI_BIRTH_AHW_PROMPT_CANCEL;
        }

        if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }
}

/* Runs one shared naming prompt and updates the working name in place. */
bool ui_birth_prompt_name(char* name, size_t size)
{
    if (ui_birth_front_prompt)
        return ui_birth_front_prompt(UI_BIRTH_SCREEN_NAME, name, size, NULL,
            NULL, NULL, 0, 0, 0, 0, 0, 0)
            == UI_BIRTH_PROMPT_ACCEPT;

    while (TRUE)
    {
        char query2;

        ui_birth_render_character_sheet_context();
        ui_birth_publish_name_screen(name);

        hide_cursor = TRUE;
        query2 = inkey();
        hide_cursor = FALSE;

        if (ui_input_is_accept_key(query2))
        {
            if (!ui_birth_submit(
                    UI_BIRTH_SCREEN_NAME, name, size, NULL, NULL, NULL))
                continue;
            p_ptr->redraw |= PR_MISC;
            ui_birth_clear_name_screen();
            ui_birth_clear_character_sheet_context();
            return TRUE;
        }

        if (query2 == '\t')
        {
            make_random_name(name, size);
            continue;
        }

        if (ui_input_is_cancel_key(query2))
        {
            ui_birth_clear_name_screen();
            ui_birth_clear_character_sheet_context();
            return FALSE;
        }

        if (((query2 == 'Q') || (query2 == 'q')) && (turn == 0))
            quit(NULL);
    }
}

/* Publishes the current shared stat-allocation screen state. */
void ui_birth_publish_stats_screen(const int* stats, int selected)
{
    int i;
    int total_cost;

    if (!stats)
    {
        ui_birth_clear_stats_screen();
        return;
    }

    total_cost = ui_birth_stats_total_cost(stats);

    ui_birth_stats_state.active = TRUE;
    ui_birth_stats_state.selected = ui_birth_stats_index_valid(selected) ? selected : 0;
    ui_birth_stats_state.points_left = ui_birth_stat_budget() - total_cost;

    for (i = 0; i < A_MAX; i++)
    {
        int total_value = stats[i] + rp_ptr->r_adj[i] + hp_ptr->h_adj[i];

        ui_birth_stats_state.costs[i] = ui_birth_stat_cost_for_value(stats[i]);
        ui_birth_stats_state.can_decrease[i] = stats[i] > UI_BIRTH_STAT_MIN;
        ui_birth_stats_state.can_increase[i]
            = ui_birth_stats_value_can_increase(stats, i, total_cost);
        cnv_stat(total_value, ui_birth_stats_state.display_values[i],
            sizeof(ui_birth_stats_state.display_values[i]));
    }

    ui_birth_touch();
}

/* Clears the exported stat-allocation screen state. */
void ui_birth_clear_stats_screen(void)
{
    if (!ui_birth_stats_state.active)
        return;

    WIPE(&ui_birth_stats_state, struct ui_birth_stats_state);
    ui_birth_touch();
}

/* Publishes the current shared history-editing screen state. */
void ui_birth_publish_history_screen(cptr history)
{
    ui_birth_history_state.active = TRUE;
    my_strcpy(
        ui_birth_history_state.text, history ? history : "", sizeof(ui_birth_history_state.text));
    ui_birth_touch();
}

/* Clears the exported history-editing screen state. */
void ui_birth_clear_history_screen(void)
{
    bool had_active_state = ui_birth_history_state.active ? TRUE : FALSE;

    WIPE(&ui_birth_history_state, struct ui_birth_history_state);

    if (had_active_state)
        ui_birth_touch();
}

/* Publishes the current shared age/height/weight screen state. */
void ui_birth_publish_ahw_screen(int age, int height, int weight, int age_min,
    int age_max, int height_min, int height_max, int weight_min, int weight_max)
{
    ui_birth_ahw_state.active = TRUE;
    ui_birth_ahw_state.age = age;
    ui_birth_ahw_state.height = height;
    ui_birth_ahw_state.weight = weight;
    ui_birth_ahw_state.age_min = age_min;
    ui_birth_ahw_state.age_max = age_max;
    ui_birth_ahw_state.height_min = height_min;
    ui_birth_ahw_state.height_max = height_max;
    ui_birth_ahw_state.weight_min = weight_min;
    ui_birth_ahw_state.weight_max = weight_max;
    ui_birth_touch();
}

/* Clears the exported age/height/weight screen state. */
void ui_birth_clear_ahw_screen(void)
{
    bool had_active_state = ui_birth_ahw_state.active ? TRUE : FALSE;

    WIPE(&ui_birth_ahw_state, struct ui_birth_ahw_state);

    if (had_active_state)
        ui_birth_touch();
}

/* Publishes the current shared name-editing screen state. */
void ui_birth_publish_name_screen(cptr name)
{
    ui_birth_name_state.active = TRUE;
    my_strcpy(
        ui_birth_name_state.text, name ? name : "", sizeof(ui_birth_name_state.text));
    ui_birth_touch();
}

/* Clears the exported name-editing screen state. */
void ui_birth_clear_name_screen(void)
{
    bool had_active_state = ui_birth_name_state.active ? TRUE : FALSE;

    WIPE(&ui_birth_name_state, struct ui_birth_name_state);

    if (had_active_state)
        ui_birth_touch();
}

/* Asks the active frontend to render the current stat-allocation screen. */
void ui_birth_render_stats_screen(void)
{
    if (ui_birth_front_stats_render)
        ui_birth_front_stats_render();
}

/* Returns the current shared birth-screen revision. */
unsigned int ui_birth_revision(void)
{
    return ui_birth_state_value_revision;
}

/* Copies the current semantic birth-screen state into one snapshot. */
void ui_birth_get_state_snapshot(struct ui_birth_state_snapshot* snapshot)
{
    int i;

    if (!snapshot)
        return;

    WIPE(snapshot, struct ui_birth_state_snapshot);

    if (ui_birth_name_state.active)
    {
        snapshot->active = TRUE;
        snapshot->kind = UI_BIRTH_SCREEN_NAME;
        snapshot->title = ui_birth_name_title_text;
        snapshot->help = ui_birth_name_help_text;
        snapshot->text = ui_birth_name_state.text;
        snapshot->max_length = UI_BIRTH_NAME_TEXT_MAX - 1;
        return;
    }

    if (ui_birth_ahw_state.active)
    {
        snapshot->active = TRUE;
        snapshot->kind = UI_BIRTH_SCREEN_AHW;
        snapshot->title = ui_birth_ahw_title_text;
        snapshot->help = ui_birth_ahw_help_text;
        snapshot->age = ui_birth_ahw_state.age;
        snapshot->height = ui_birth_ahw_state.height;
        snapshot->weight = ui_birth_ahw_state.weight;
        snapshot->age_min = ui_birth_ahw_state.age_min;
        snapshot->age_max = ui_birth_ahw_state.age_max;
        snapshot->height_min = ui_birth_ahw_state.height_min;
        snapshot->height_max = ui_birth_ahw_state.height_max;
        snapshot->weight_min = ui_birth_ahw_state.weight_min;
        snapshot->weight_max = ui_birth_ahw_state.weight_max;
        return;
    }

    if (ui_birth_history_state.active)
    {
        snapshot->active = TRUE;
        snapshot->kind = UI_BIRTH_SCREEN_HISTORY;
        snapshot->title = ui_birth_history_title_text;
        snapshot->help = ui_birth_history_help_text;
        snapshot->text = ui_birth_history_state.text;
        snapshot->max_length = UI_BIRTH_HISTORY_TEXT_MAX - 1;
        return;
    }

    if (!ui_birth_stats_state.active)
        return;

    snapshot->active = TRUE;
    snapshot->kind = UI_BIRTH_SCREEN_STATS;
    snapshot->title = ui_birth_stats_title_text;
    snapshot->points_left = ui_birth_stats_state.points_left;
    snapshot->stats_count = A_MAX;

    for (i = 0; i < A_MAX; i++)
    {
        snapshot->stats[i].name = stat_names[i];
        snapshot->stats[i].value = ui_birth_stats_state.display_values[i];
        snapshot->stats[i].cost = ui_birth_stats_state.costs[i];
        snapshot->stats[i].selected = (i == ui_birth_stats_state.selected);
        snapshot->stats[i].can_decrease = ui_birth_stats_state.can_decrease[i];
        snapshot->stats[i].can_increase = ui_birth_stats_state.can_increase[i];
    }
}

/* Opens the shared semantic race chooser used by terminal and web. */
int ui_birth_choose_race(int current_race)
{
    int selection;

    if (z_info->p_max <= 0)
        return UI_BIRTH_CHOICE_CANCELLED;

    selection = current_race;
    if ((selection < 0) || (selection >= z_info->p_max))
        selection = 0;

    while (TRUE)
    {
        int key;
        int alpha_choice;
        ui_input_simple_menu_action action;

        ui_birth_publish_race_menu(selection);
        ui_menu_render_current();

        hide_cursor = TRUE;
        key = inkey();
        hide_cursor = FALSE;

        if ((key == 'Q') || (key == 'q'))
            quit(NULL);

        if ((key == 'O') || (key == 'o'))
        {
            do_cmd_options();
            continue;
        }

        if (key == '*')
        {
            ui_birth_clear_menu();
            return rand_int(z_info->p_max);
        }

        alpha_choice = ui_input_alpha_choice_index(key);
        if (alpha_choice >= 0)
        {
            if (alpha_choice < z_info->p_max)
            {
                ui_birth_clear_menu();
                return alpha_choice;
            }

            bell("Illegal response to question!");
            continue;
        }

        action = ui_input_parse_simple_menu_key(key);
        if (action == UI_INPUT_SIMPLE_MENU_ACTION_CANCEL)
        {
            ui_birth_clear_menu();
            return UI_BIRTH_CHOICE_CANCELLED;
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_CHOOSE)
        {
            ui_birth_clear_menu();
            return selection;
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_PREV)
        {
            if (selection > 0)
                selection--;
            continue;
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_NEXT)
        {
            if (selection + 1 < z_info->p_max)
                selection++;
            continue;
        }

        bell("Illegal response to question!");
    }
}

/* Opens the shared semantic house chooser used by terminal and web. */
int ui_birth_choose_house(int race, int current_house)
{
    int houses[32];
    int house_count;
    int selection;

    if ((race < 0) || (race >= z_info->p_max))
        return UI_BIRTH_CHOICE_CANCELLED;

    house_count = ui_birth_collect_house_choices(race, houses, N_ELEMENTS(houses));
    if (house_count <= 0)
        return 0;

    if ((house_count == 1) && (houses[0] == 0))
        return 0;

    selection = 0;
    while (selection < house_count)
    {
        if (houses[selection] == current_house)
            break;
        selection++;
    }
    if (selection >= house_count)
        selection = 0;

    while (TRUE)
    {
        int key;
        int alpha_choice;
        ui_input_simple_menu_action action;

        ui_birth_publish_house_menu(race, houses, house_count, selection);
        ui_menu_render_current();

        hide_cursor = TRUE;
        key = inkey();
        hide_cursor = FALSE;

        if ((key == 'Q') || (key == 'q'))
            quit(NULL);

        if ((key == 'O') || (key == 'o'))
        {
            do_cmd_options();
            continue;
        }

        if (key == '*')
        {
            ui_birth_clear_menu();
            return houses[rand_int(house_count)];
        }

        alpha_choice = ui_input_alpha_choice_index(key);
        if (alpha_choice >= 0)
        {
            if (alpha_choice < house_count)
            {
                ui_birth_clear_menu();
                return houses[alpha_choice];
            }

            bell("Illegal response to question!");
            continue;
        }

        action = ui_input_parse_simple_menu_key(key);
        if (action == UI_INPUT_SIMPLE_MENU_ACTION_CANCEL)
        {
            ui_birth_clear_menu();
            return UI_BIRTH_CHOICE_CANCELLED;
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_CHOOSE)
        {
            ui_birth_clear_menu();
            return houses[selection];
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_PREV)
        {
            if (selection > 0)
                selection--;
            continue;
        }

        if (action == UI_INPUT_SIMPLE_MENU_ACTION_NEXT)
        {
            if (selection + 1 < house_count)
                selection++;
            continue;
        }

        bell("Illegal response to question!");
    }
}
