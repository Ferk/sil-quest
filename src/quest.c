/* File: quest.c */

/*
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Quest selection and runtime rule helpers.
 */

#include "angband.h"

static s16b quest_pending_start_id = 0;

/* Return the quest currently attached to the live player state. */
static int quests_current_id(void)
{
    if (!p_ptr)
        return (0);

    return (int)(p_ptr->unused2);
}

/* Attach the selected quest rules to the current player. */
static void quests_activate(int quest_id)
{
    const quest_type* q_ptr = quests_get(quest_id);

    if (!q_ptr || !p_ptr)
        return;

    p_ptr->unused2 = quest_id;
    p_ptr->game_type = q_ptr->game_type;
}

/* Test whether a quest restriction mask allows the given value. */
static bool quest_mask_has(const u32b* mask, int value)
{
    int word = value / 32;
    int bit = value % 32;

    if ((value < 0) || (word >= QUEST_MASK_WORDS))
        return (FALSE);

    return ((mask[word] & (1UL << bit)) != 0);
}

/* Show quest completion text using the standard full-screen text view. */
static void quests_show_text(cptr text)
{
    void (*old_hook)(byte a, cptr str) = text_out_hook;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    char ch;

    screen_save();
    Term_clear();

    Term_gotoxy(3, 4);
    text_out_hook = text_out_to_screen;
    text_out_wrap = 76;
    text_out_indent = 3;

    text_out_c(TERM_WHITE, text ? text : "You have completed this quest.");
    text_out_c(TERM_L_BLUE, "\n\n(press any key)\n");
    Term_fresh();

    text_out_hook = old_hook;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    while (1)
    {
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        if (ch != EOF)
            break;

        message_flush();
    }

    screen_load();
}

/*
 * Show one quest-authored multiline splash screen using raw line breaks.
 */
static void quests_pause_with_text(cptr text, int row, int col)
{
    char ch;
    char line[160];
    int i = 0;
    cptr s = text ? text : "";

    screen_save();
    Term_clear();

    while (*s)
    {
        int len = 0;

        while (s[len] && (s[len] != '\n') && (len < (int)sizeof(line) - 1))
            len++;

        memcpy(line, s, len);
        line[len] = '\0';
        c_put_str(TERM_WHITE, line, row + i, col);

        if (s[len] == '\n')
            s += len + 1;
        else
            s += len;

        i++;
    }

    Term_fresh();

    while (1)
    {
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        if (ch != EOF)
            break;

        message_flush();
    }

    screen_load();
}

int quests_count(void)
{
    int i;
    int count = 0;

    if (!q_info || !z_info)
        return (0);

    for (i = 1; i < z_info->q_max; i++)
    {
        if (q_info[i].name)
            count++;
    }

    return (count);
}

int quests_get_id_by_position(int position)
{
    int i;
    int count = 0;

    if ((position <= 0) || !q_info || !z_info)
        return (0);

    for (i = 1; i < z_info->q_max; i++)
    {
        if (!q_info[i].name)
            continue;

        count++;
        if (count == position)
            return (i);
    }

    return (0);
}

const quest_type* quests_get(int quest_id)
{
    if (!q_info || !z_info)
        return (NULL);

    if ((quest_id <= 0) || (quest_id >= z_info->q_max))
        return (NULL);

    if (!q_info[quest_id].name)
        return (NULL);

    return (&q_info[quest_id]);
}

const quest_type* quests_current(void)
{
    return quests_get(quests_current_id());
}

cptr quests_name(int quest_id)
{
    const quest_type* q_ptr = quests_get(quest_id);

    if (!q_ptr || !q_ptr->name || !q_name)
        return ("");

    return (q_name + q_ptr->name);
}

cptr quests_description(int quest_id)
{
    const quest_type* q_ptr = quests_get(quest_id);

    if (!q_ptr || !q_ptr->text || !q_text)
        return ("");

    return (q_text + q_ptr->text);
}

/* Return the first-level entry text configured for a quest, if any. */
cptr quests_entry_text(int quest_id)
{
    const quest_type* q_ptr = quests_get(quest_id);

    if (!q_ptr || !q_ptr->entry_text || !q_text)
        return ("");

    return (q_text + q_ptr->entry_text);
}

/* Resolve how a quest should start and queue any pending scenario state. */
bool quests_prepare_start(
    int quest_id, bool* new_game, char* savefile_buf, size_t savefile_len)
{
    const quest_type* q_ptr = quests_get(quest_id);

    if (!q_ptr || !new_game || !savefile_buf)
        return (FALSE);

    quest_pending_start_id = quest_id;

    if (q_ptr->start_kind == QST_START_BIRTH)
    {
        scenario_clear_pending();
        savefile_buf[0] = '\0';
        *new_game = TRUE;
        return (TRUE);
    }

    if ((q_ptr->start_kind == QST_START_SAVEFILE) && q_ptr->start && q_name)
    {
        scenario_clear_pending();
        path_build(savefile_buf, savefile_len, ANGBAND_DIR_XTRA,
            q_name + q_ptr->start);
        *new_game = FALSE;
        return (TRUE);
    }

    if ((q_ptr->start_kind == QST_START_SCENARIO) && q_ptr->start && q_name)
    {
        savefile_buf[0] = '\0';
        *new_game = TRUE;

        if (!scenario_prepare_pending(q_name + q_ptr->start))
        {
            quest_pending_start_id = 0;
            return (FALSE);
        }

        return (TRUE);
    }

    quest_pending_start_id = 0;
    return (FALSE);
}

void quests_clear_pending_start(void)
{
    quest_pending_start_id = 0;
    scenario_clear_pending();
}

cptr quests_pending_savefile(void)
{
    const quest_type* q_ptr = quests_get(quest_pending_start_id);

    if (!q_ptr || !q_ptr->start || !q_name)
        return (NULL);

    if (q_ptr->start_kind != QST_START_SAVEFILE)
        return (NULL);

    return (q_name + q_ptr->start);
}

/* Start the pending quest as a new game, whether from birth or a scenario. */
bool quests_start_pending_new_game(void)
{
    const quest_type* q_ptr = quests_get(quest_pending_start_id);

    if (!q_ptr)
    {
        scenario_clear_pending();
        player_birth();
        p_ptr->depth = 1;
        return (TRUE);
    }

    quests_activate(quest_pending_start_id);
    quest_pending_start_id = 0;

    if (q_ptr->start_kind == QST_START_SCENARIO)
        return (scenario_start_pending_new_game());

    scenario_clear_pending();
    player_birth();
    p_ptr->depth = 1;
    return (TRUE);
}

void quests_activate_pending_for_loaded_game(void)
{
    scenario_clear_pending();

    if (quest_pending_start_id > 0)
    {
        quests_activate(quest_pending_start_id);
    }

    quest_pending_start_id = 0;
}

bool quests_pending_level_generation(void)
{
    return (scenario_pending_level_generation());
}

bool quests_generate_pending_level(void)
{
    return (scenario_generate_pending_level());
}

bool quests_allow_monster_race(int r_idx)
{
    const quest_type* q_ptr = quests_current();

    if (!q_ptr || !(q_ptr->flags & QST_F_LIMIT_MONSTERS))
        return (TRUE);

    if ((r_idx <= 0) || !r_info)
        return (FALSE);

    return (quest_mask_has(q_ptr->monster_char_mask, r_info[r_idx].d_char));
}

bool quests_allow_object_kind(int k_idx)
{
    const quest_type* q_ptr = quests_current();

    if (!q_ptr || !(q_ptr->flags & QST_F_LIMIT_OBJECTS))
        return (TRUE);

    if ((k_idx <= 0) || !k_info)
        return (FALSE);

    return (quest_mask_has(q_ptr->object_tval_mask, k_info[k_idx].tval));
}

bool quests_allow_stair_monsters(void)
{
    const quest_type* q_ptr = quests_current();

    if (q_ptr)
        return ((q_ptr->flags & QST_F_NO_STAIRS_MONSTERS) == 0);
    if (!p_ptr)
        return (TRUE);

    return (p_ptr->game_type == 0);
}

bool quests_save_disabled(void)
{
    const quest_type* q_ptr = quests_current();

    if (q_ptr)
        return ((q_ptr->flags & QST_F_NO_SAVE) != 0);
    if (!p_ptr)
        return (FALSE);

    return (p_ptr->game_type != 0);
}

/* Show the current quest's optional first-level entry text, if any. */
void quests_show_entry_text(void)
{
    cptr text = "";
    const quest_type* q_ptr = quests_current();

    if (q_ptr)
        text = quests_entry_text(quests_current_id());

    if (!text[0])
        return;

    quests_pause_with_text(text, 5, 15);
}

/* Complete quests whose objective is satisfied by taking the down stairs. */
bool quests_try_complete_on_down_stairs(void)
{
    const quest_type* q_ptr = quests_current();

    if (!q_ptr || (q_ptr->objective != QST_OBJECTIVE_STAIRS_EXIT))
        return (FALSE);

    if (q_ptr->completion_text && q_text)
        quests_show_text(q_text + q_ptr->completion_text);
    else
        quests_show_text("You have completed this quest.");

    if (p_ptr->game_type == 0)
        p_ptr->game_type = (q_ptr->game_type != 0) ? q_ptr->game_type : 1;

    p_ptr->is_dead = TRUE;
    p_ptr->energy_use = 100;
    p_ptr->leaving = TRUE;
    close_game();

    return (TRUE);
}
