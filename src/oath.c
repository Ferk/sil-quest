/* File: oath.c */

/*
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Active quest oath catalog and runtime helpers.
 */

#include "angband.h"

#define OATH_MAX_DEFS 8
#define OATH_TEXT_LEN 160

typedef struct oath_def oath_def;

struct oath_def
{
    char name[32];
    char vow[OATH_TEXT_LEN];
    char restriction[OATH_TEXT_LEN];
    byte reward_stat;
    s16b reward_amount;
};

static oath_def oath_defs[OATH_MAX_DEFS];
static int oath_defs_count = 0;

/* Convert one oath index to a live catalog slot, if it exists. */
static int oath_slot(int oath)
{
    if ((oath <= 0) || (oath > oath_defs_count))
        return (-1);

    return (oath - 1);
}

/* Return the broken-bit mask for one oath index. */
static byte oath_mask(int oath)
{
    int slot = oath_slot(oath);

    if ((slot < 0) || (slot >= 8))
        return (0);

    return ((byte)(1U << slot));
}

/* Lower-case one short oath name for player-facing messages. */
static void oath_lower_name(int oath, char* buf, size_t len)
{
    int slot = oath_slot(oath);
    cptr name = (slot < 0) ? "nothing" : oath_defs[slot].name;
    size_t i;

    if (!buf || !len)
        return;

    my_strcpy(buf, name ? name : "", len);
    for (i = 0; buf[i]; i++)
        buf[i] = (char)tolower((unsigned char)buf[i]);
}

/* Clear the active oath catalog for the current quest. */
void oath_clear_catalog(void)
{
    memset(oath_defs, 0, sizeof(oath_defs));
    oath_defs_count = 0;
}

/* Append one oath definition to the active quest catalog. */
bool oath_add_definition(
    cptr name, cptr vow, cptr restriction, int reward_stat, int reward_amount)
{
    oath_def* oath;

    if (!name || !name[0] || !vow || !vow[0] || !restriction || !restriction[0])
        return (FALSE);
    if ((reward_stat < 0) || (reward_stat >= A_MAX) || (reward_amount == 0))
        return (FALSE);
    if (oath_defs_count >= OATH_MAX_DEFS)
        return (FALSE);

    oath = &oath_defs[oath_defs_count++];
    my_strcpy(oath->name, name, sizeof(oath->name));
    my_strcpy(oath->vow, vow, sizeof(oath->vow));
    my_strcpy(oath->restriction, restriction, sizeof(oath->restriction));
    oath->reward_stat = (byte)reward_stat;
    oath->reward_amount = (s16b)reward_amount;
    return (TRUE);
}

/* Return how many quest-authored oath choices are currently available. */
int oath_count(void)
{
    return (oath_defs_count);
}

/* Look up one active oath by name, returning 0 for None and -1 on failure. */
int oath_lookup(cptr token)
{
    int i;

    if (!token || !token[0] || !my_stricmp(token, "None"))
        return (0);

    for (i = 0; i < oath_defs_count; i++)
    {
        if (!my_stricmp(token, oath_defs[i].name))
            return (i + 1);
    }

    return (-1);
}

/* Return the display name for one oath, or "Nothing" for the empty choice. */
cptr oath_name(int oath)
{
    int slot = oath_slot(oath);

    if (slot < 0)
        return ("Nothing");

    return (oath_defs[slot].name);
}

/* Return the long-form vow text for one oath. */
cptr oath_desc1(int oath)
{
    int slot = oath_slot(oath);

    if (slot < 0)
        return ("Nothing");

    return (oath_defs[slot].vow);
}

/* Return the restriction summary for one oath. */
cptr oath_desc2(int oath)
{
    int slot = oath_slot(oath);

    if (slot < 0)
        return ("Nothing");

    return (oath_defs[slot].restriction);
}

/* Return one short player-facing reward summary for one oath. */
cptr oath_reward(int oath)
{
    static char buf[4][32];
    static int next = 0;
    int slot = oath_slot(oath);
    char* out;

    if (slot < 0)
        return ("Nothing");

    out = buf[next];
    next = (next + 1) % (int)N_ELEMENTS(buf);

    strnfmt(out, sizeof(buf[0]), "%+d %s", oath_defs[slot].reward_amount,
        stat_names_full[oath_defs[slot].reward_stat]);
    return (out);
}

/* Return whether one chosen oath has already been broken this run. */
bool oath_invalid(int oath)
{
    byte mask;

    if (!p_ptr)
        return (FALSE);

    mask = oath_mask(oath);
    return ((mask != 0) && ((p_ptr->oaths_broken & mask) != 0));
}

/* Return whether the player currently follows the given oath. */
bool chosen_oath(int oath)
{
    return (p_ptr && (p_ptr->oath_type == oath));
}

/* Return whether the currently chosen oath, if any, is broken. */
bool oath_broken_current(void)
{
    return (p_ptr && (p_ptr->oath_type > 0) && oath_invalid(p_ptr->oath_type));
}

/* Break the currently chosen intact oath and apply its side effects. */
bool oath_break_current(void)
{
    byte mask;
    char lower_name[32];

    if (!p_ptr || (p_ptr->oath_type <= 0))
        return (FALSE);
    if (oath_invalid(p_ptr->oath_type))
        return (FALSE);

    mask = oath_mask(p_ptr->oath_type);
    if (!mask)
        return (FALSE);

    oath_lower_name(p_ptr->oath_type, lower_name, sizeof(lower_name));
    msg_format("You break your oath of %s.", lower_name);
    do_cmd_note("Broke your oath", p_ptr->depth);

    p_ptr->oaths_broken |= mask;
    p_ptr->redraw |= PR_BASIC;
    p_ptr->update |= (PU_BONUS | PU_MANA);

    return (TRUE);
}

/* Apply the active intact oath's stat bonus to the current player. */
void oath_apply_bonus(void)
{
    int slot;

    if (!p_ptr || !p_ptr->active_ability[S_WIL][WIL_OATH])
        return;
    if (oath_invalid(p_ptr->oath_type))
        return;

    slot = oath_slot(p_ptr->oath_type);
    if (slot < 0)
        return;

    p_ptr->stat_misc_mod[oath_defs[slot].reward_stat]
        += oath_defs[slot].reward_amount;
}
