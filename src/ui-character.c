/* File: ui-character.c
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

#include "angband.h"

#include "ui-character.h"
#include "ui-model.h"

static const char ui_character_skill_editor_title_text[] = "Increase your skills";
static const char ui_character_skill_editor_help_text[]
    = "Use Up/Down to choose a skill and Left/Right to spend experience. "
      "Enter accepts the current values and Escape goes back.";
static ui_character_sheet_state ui_character_state_value = { 0 };
static unsigned int ui_character_sheet_state_revision = 1;
static ui_character_render_hook ui_character_front_render = NULL;

/* Bumps the shared character-sheet revision. */
static void ui_character_touch(void)
{
    ui_character_sheet_state_revision++;
    if (ui_character_sheet_state_revision == 0)
        ui_character_sheet_state_revision = 1;
    ui_front_invalidate();
}

/* Stores one semantic label/value field. */
static void ui_character_set_field(
    ui_character_field* field, cptr label, cptr value, bool visible)
{
    if (!field)
        return;

    my_strcpy(field->label, label ? label : "", sizeof(field->label));
    my_strcpy(field->value, value ? value : "", sizeof(field->value));
    field->visible = visible;
}

/* Stores one semantic action descriptor. */
static void ui_character_set_action(
    ui_character_action* action, int key, cptr label)
{
    if (!action)
        return;

    action->key = key;
    my_strcpy(action->label, label ? label : "", sizeof(action->label));
}

/* Clears any temporary skill-editor metadata from the shared character sheet. */
static void ui_character_reset_skill_editor(ui_character_sheet_state* state)
{
    int i;

    if (!state)
        return;

    state->skill_editor_active = FALSE;
    state->skill_editor_title[0] = '\0';
    state->skill_editor_help[0] = '\0';
    state->skill_points_left = 0;

    for (i = 0; i < S_MAX; i++)
    {
        state->skills[i].cost = 0;
        state->skills[i].editable = FALSE;
        state->skills[i].selected = FALSE;
        state->skills[i].can_decrease = FALSE;
        state->skills[i].can_increase = FALSE;
    }
}

/* Formats one feet-and-inches height string. */
static void ui_character_format_height(char* buf, size_t size, int height)
{
    int feet;
    int inches;

    if (!buf || (size == 0))
        return;

    buf[0] = '\0';
    if (height <= 0)
        return;

    feet = height / 12;
    inches = height % 12;
    if (inches > 0)
        strnfmt(buf, size, "%d'%d", feet, inches);
    else
        strnfmt(buf, size, "%d'", feet);
}

/* Formats one decimal burden string. */
static void ui_character_format_weight_tenths(char* buf, size_t size, int value)
{
    if (!buf || (size == 0))
        return;

    strnfmt(buf, size, "%3d.%1d", value / 10L, value % 10L);
}

/* Formats one decimal-with-commas integer string. */
static void ui_character_format_number(char* buf, size_t size, s32b value)
{
    if (!buf || (size == 0))
        return;

    comma_number(buf, size, value);
}

/* Returns the total experience cost for one temporary skill increase. */
int ui_character_skill_gain_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = base * (base + 1) / 2;

    return (total_cost - prev_cost) * 100;
}

/* Builds one semantic stat row from the current player values. */
static void ui_character_build_stat(ui_character_stat* stat, int index)
{
    if (!stat || (index < 0) || (index >= A_MAX))
        return;

    WIPE(stat, ui_character_stat);

    my_strcpy(stat->label,
        (p_ptr->stat_drain[index] < 0) ? stat_names_reduced[index] : stat_names[index],
        sizeof(stat->label));
    cnv_stat(p_ptr->stat_use[index], stat->current, sizeof(stat->current));
    cnv_stat(p_ptr->stat_base[index], stat->base, sizeof(stat->base));
    stat->equip_mod = p_ptr->stat_equip_mod[index];
    stat->drain_mod = p_ptr->stat_drain[index];
    stat->misc_mod = p_ptr->stat_misc_mod[index];
    stat->reduced = (p_ptr->stat_drain[index] < 0) ? TRUE : FALSE;
    stat->show_equip = (stat->equip_mod != 0) ? TRUE : FALSE;
    stat->show_drain = (stat->drain_mod != 0) ? TRUE : FALSE;
    stat->show_misc = (stat->misc_mod != 0) ? TRUE : FALSE;
    stat->show_base = stat->show_equip || stat->show_drain || stat->show_misc;
}

/* Builds one semantic skill row from the current player values. */
static void ui_character_build_skill(ui_character_skill* skill, int index)
{
    if (!skill || (index < 0) || (index >= S_MAX))
        return;

    WIPE(skill, ui_character_skill);

    my_strcpy(skill->label, skill_names_full[index], sizeof(skill->label));
    skill->value = p_ptr->skill_use[index];
    skill->base = p_ptr->skill_base[index];
    skill->stat_mod = p_ptr->skill_stat_mod[index];
    skill->equip_mod = p_ptr->skill_equip_mod[index];
    skill->misc_mod = p_ptr->skill_misc_mod[index];
}

/* Builds the current combat and resource summary rows. */
static void ui_character_build_combat_fields(ui_character_sheet_state* state)
{
    char buf[32];
    int row = 0;

    if (!state)
        return;

    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    ui_character_set_field(&state->combat[row++], "Melee", buf, TRUE);

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
        ui_character_set_field(&state->combat[row++], "", buf, TRUE);

    if (p_ptr->mds2 > 0)
    {
        strnfmt(buf, sizeof(buf), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
        ui_character_set_field(&state->combat[row++], "", buf, TRUE);
    }

    strnfmt(buf, sizeof(buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC], p_ptr->add,
        p_ptr->ads);
    ui_character_set_field(&state->combat[row++], "Bows", buf, TRUE);

    strnfmt(buf, sizeof(buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, TRUE), p_max(GF_HURT, TRUE));
    ui_character_set_field(&state->combat[row++], "Armor", buf, TRUE);

    strnfmt(buf, sizeof(buf), "%d:%d", p_ptr->chp, p_ptr->mhp);
    ui_character_set_field(&state->combat[row++], "Health", buf, TRUE);

    strnfmt(buf, sizeof(buf), "%d:%d", p_ptr->csp, p_ptr->msp);
    ui_character_set_field(&state->combat[row++], "Voice", buf, TRUE);

    if (p_ptr->song1 != SNG_NOTHING)
    {
        cptr song_name =
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        ui_character_set_field(&state->combat[row++], "Song", song_name + 8, TRUE);
    }

    if ((row < UI_CHARACTER_COMBAT_COUNT) && (p_ptr->song2 != SNG_NOTHING))
    {
        cptr song_name =
            b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;
        ui_character_set_field(&state->combat[row++], "", song_name + 8, TRUE);
    }
}

/* Registers the frontend hook used to render the shared character sheet. */
void ui_character_set_render_hook(ui_character_render_hook render_hook)
{
    ui_character_front_render = render_hook;
}

/* Publishes the current semantic character-sheet state. */
void ui_character_publish_sheet(void)
{
    char buf[32];
    int i;

    WIPE(&ui_character_state_value, ui_character_sheet_state);

    ui_character_state_value.active = TRUE;
    my_strcpy(ui_character_state_value.title, "Character Sheet",
        sizeof(ui_character_state_value.title));

    ui_character_set_field(&ui_character_state_value.identity[0], "Name",
        op_ptr->full_name[0] ? op_ptr->full_name : "(unnamed)", TRUE);
    ui_character_set_field(&ui_character_state_value.identity[1], "Race",
        p_name + rp_ptr->name, TRUE);
    ui_character_set_field(&ui_character_state_value.identity[2], "House",
        p_ptr->phouse ? c_name + hp_ptr->short_name : "", p_ptr->phouse ? TRUE : FALSE);

    strnfmt(buf, sizeof(buf), "%d", (int)p_ptr->age);
    ui_character_set_field(&ui_character_state_value.physical[0], "Age",
        (p_ptr->age > 0) ? buf : "", TRUE);
    ui_character_format_height(buf, sizeof(buf), p_ptr->ht);
    ui_character_set_field(&ui_character_state_value.physical[1], "Height", buf, TRUE);
    strnfmt(buf, sizeof(buf), "%d", (int)p_ptr->wt);
    ui_character_set_field(&ui_character_state_value.physical[2], "Weight",
        (p_ptr->wt > 0) ? buf : "", TRUE);

    ui_character_format_number(buf, sizeof(buf), playerturn);
    ui_character_set_field(&ui_character_state_value.progress[0], "Game Turn",
        buf, TRUE);
    ui_character_format_number(buf, sizeof(buf), p_ptr->new_exp);
    ui_character_set_field(&ui_character_state_value.progress[1], "Exp Pool",
        buf, TRUE);
    ui_character_format_number(buf, sizeof(buf), p_ptr->exp);
    ui_character_set_field(&ui_character_state_value.progress[2], "Total Exp",
        buf, TRUE);
    ui_character_format_weight_tenths(buf, sizeof(buf), p_ptr->total_weight);
    ui_character_set_field(
        &ui_character_state_value.progress[3], "Burden", buf, TRUE);
    ui_character_format_weight_tenths(buf, sizeof(buf), weight_limit());
    ui_character_set_field(
        &ui_character_state_value.progress[4], "Max Burden", buf, TRUE);
    strnfmt(buf, sizeof(buf), "%3d'", p_ptr->depth * 50);
    ui_character_set_field(&ui_character_state_value.progress[5], "Depth", buf,
        turn > 0);
    strnfmt(buf, sizeof(buf), "%3d'", min_depth() * 50);
    ui_character_set_field(&ui_character_state_value.progress[6], "Min Depth",
        buf, turn > 0);
    strnfmt(buf, sizeof(buf), "%3d", p_ptr->cur_light);
    ui_character_set_field(
        &ui_character_state_value.progress[7], "Light Radius", buf, TRUE);

    for (i = 0; i < A_MAX; i++)
        ui_character_build_stat(&ui_character_state_value.stats[i], i);

    ui_character_build_combat_fields(&ui_character_state_value);

    for (i = 0; i < S_MAX; i++)
        ui_character_build_skill(&ui_character_state_value.skills[i], i);

    my_strcpy(ui_character_state_value.history, p_ptr->history,
        sizeof(ui_character_state_value.history));

    ui_character_set_action(&ui_character_state_value.actions[0], 'n', "Notes");
    ui_character_set_action(
        &ui_character_state_value.actions[1], 'c', "Change name");
    ui_character_set_action(
        &ui_character_state_value.actions[2], 's', "Save to file");
    ui_character_set_action(
        &ui_character_state_value.actions[3], 'a', "Abilities");
    ui_character_set_action(
        &ui_character_state_value.actions[4], 'i', "Increase skills");
    ui_character_set_action(&ui_character_state_value.actions[5], ESCAPE, "Close");
    ui_character_state_value.action_count = UI_CHARACTER_ACTION_COUNT;
    ui_character_reset_skill_editor(&ui_character_state_value);

    ui_character_touch();
}

/* Publishes skill-allocation metadata on top of the current character sheet. */
void ui_character_publish_skill_editor(const int* old_base,
    const int* skill_gain, int selected, int points_left)
{
    int i;

    if (!ui_character_state_value.active || !old_base || !skill_gain)
        return;

    ui_character_state_value.skill_editor_active = TRUE;
    my_strcpy(ui_character_state_value.skill_editor_title,
        ui_character_skill_editor_title_text,
        sizeof(ui_character_state_value.skill_editor_title));
    my_strcpy(ui_character_state_value.skill_editor_help,
        ui_character_skill_editor_help_text,
        sizeof(ui_character_state_value.skill_editor_help));
    ui_character_state_value.skill_points_left = points_left;

    for (i = 0; i < S_MAX; i++)
    {
        int cost = ui_character_skill_gain_cost(old_base[i], skill_gain[i]);
        int next_delta = ui_character_skill_gain_cost(old_base[i], skill_gain[i] + 1)
            - cost;

        ui_character_state_value.skills[i].cost = cost;
        ui_character_state_value.skills[i].editable = TRUE;
        ui_character_state_value.skills[i].selected = (i == selected) ? TRUE : FALSE;
        ui_character_state_value.skills[i].can_decrease =
            (skill_gain[i] > 0) ? TRUE : FALSE;
        ui_character_state_value.skills[i].can_increase =
            (next_delta <= points_left) ? TRUE : FALSE;
    }

    ui_character_touch();
}

/* Clears any active skill-allocation metadata from the shared character sheet. */
void ui_character_clear_skill_editor(void)
{
    if (!ui_character_state_value.active || !ui_character_state_value.skill_editor_active)
        return;

    ui_character_reset_skill_editor(&ui_character_state_value);
    ui_character_touch();
}

/* Clears the exported semantic character-sheet state. */
void ui_character_clear_sheet(void)
{
    if (!ui_character_state_value.active)
        return;

    WIPE(&ui_character_state_value, ui_character_sheet_state);
    ui_character_touch();
}

/* Asks the active frontend to render the current character sheet. */
void ui_character_render_sheet(void)
{
    if (ui_character_front_render)
        ui_character_front_render();
}

/* Returns whether the shared character sheet is currently active. */
bool ui_character_sheet_active(void)
{
    return ui_character_state_value.active;
}

/* Returns whether the shared character sheet currently carries skill editor state. */
bool ui_character_skill_editor_active(void)
{
    return ui_character_state_value.active
        && ui_character_state_value.skill_editor_active;
}

/* Returns the current character-sheet revision. */
unsigned int ui_character_sheet_revision(void)
{
    return ui_character_sheet_state_revision;
}

/* Returns the current semantic character-sheet payload. */
const ui_character_sheet_state* ui_character_get_sheet(void)
{
    return ui_character_state_value.active ? &ui_character_state_value : NULL;
}
