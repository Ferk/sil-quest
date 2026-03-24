/* File: item-rules.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral item kind rules.  At the moment these rules only cover
 * automatic notes that should be applied to all items of a given kind.
 */

#include "angband.h"

#include "item-rules.h"

typedef struct item_rule item_rule;

struct item_rule
{
    s16b kind_idx;
    s16b note_idx;
};

static item_rule item_rules[ITEM_RULES_MAX];
static u16b item_rule_count = 0;

/* Returns the stored rule index for one object kind, if present. */
static int item_rules_find_kind_index(s16b kind_idx)
{
    int i;

    for (i = 0; i < item_rule_count; i++)
    {
        if (kind_idx == item_rules[i].kind_idx)
            return i;
    }

    return -1;
}

/* Returns the stored automatic note text for one object kind. */
static cptr item_rules_kind_note(s16b kind_idx)
{
    int index = item_rules_find_kind_index(kind_idx);

    if (index < 0)
        return NULL;

    return quark_str(item_rules[index].note_idx);
}

/* Removes one automatic note from an object if it still matches exactly. */
static void item_rules_clear_matching_note(object_type* o_ptr, cptr note)
{
    cptr existing_note = quark_str(o_ptr->obj_note);

    if (existing_note && note && streq(note, existing_note))
        o_ptr->obj_note = 0;
}

/* Applies all stored automatic notes to dungeon objects. */
static void item_rules_apply_dungeon(void)
{
    int i;

    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        if (!o_ptr->k_idx)
            continue;

        item_rules_apply_note(o_ptr);
    }
}

/* Formats the plural display name for one object kind. */
static void item_rules_format_kind_name(s16b kind_idx, char* buf, size_t len)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    object_wipe(i_ptr);
    object_prep(i_ptr, kind_idx);
    i_ptr->number = 2;
    object_desc(buf, len, i_ptr, FALSE, 0);
}

/* Clears all stored item kind rules. */
void item_rules_clear(void)
{
    item_rule_count = 0;
    C_WIPE(item_rules, ITEM_RULES_MAX, item_rule);
}

/* Prompts for the automatic note attached to one object kind. */
int item_rules_prompt_kind_note(s16b kind_idx)
{
    char note[80] = "";
    cptr existing_note = item_rules_kind_note(kind_idx);

    if (existing_note)
    {
        my_strcpy(note, existing_note, sizeof(note));
        note[sizeof(note) - 1] = 0;
    }

    if (!term_get_string("Autoinscription: ", note, sizeof(note)))
        return 0;

    return item_rules_set_kind_note(kind_idx, note);
}

/* Prompts to clear the automatic note stored for one object kind. */
bool item_rules_confirm_clear_kind_note(s16b kind_idx)
{
    char prompt[160];
    char kind_name[80];

    if (!item_rules_kind_has_note(kind_idx))
        return FALSE;

    item_rules_format_kind_name(kind_idx, kind_name, sizeof(kind_name));
    strnfmt(prompt, sizeof(prompt),
        "Remove automatic inscription for %s? ", kind_name);

    if (!get_check(prompt))
        return FALSE;

    item_rules_clear_kind_note(kind_idx);
    return TRUE;
}

/* Prompts to store one automatic note for all items of one kind. */
bool item_rules_confirm_set_kind_note(s16b kind_idx, cptr note)
{
    char prompt[160];
    char kind_name[80];

    if (!note || !note[0])
        return FALSE;

    item_rules_format_kind_name(kind_idx, kind_name, sizeof(kind_name));
    strnfmt(prompt, sizeof(prompt),
        "Automatically inscribe all %s with '%s'? ", kind_name, note);

    if (!get_check(prompt))
        return FALSE;

    return item_rules_set_kind_note(kind_idx, note) != 0;
}

/* Returns whether one object kind currently has an automatic note. */
bool item_rules_kind_has_note(s16b kind_idx)
{
    return item_rules_find_kind_index(kind_idx) >= 0;
}

/* Removes the automatic note for one object kind. */
void item_rules_clear_kind_note(s16b kind_idx)
{
    int i;
    int index = item_rules_find_kind_index(kind_idx);
    cptr note;

    if (index < 0)
        return;

    note = quark_str(item_rules[index].note_idx);

    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        if (o_ptr->k_idx != kind_idx)
            continue;

        item_rules_clear_matching_note(o_ptr, note);
    }

    for (i = INVEN_PACK; i > 0; i--)
    {
        if (inventory[i].k_idx != kind_idx)
            continue;

        item_rules_clear_matching_note(&inventory[i], note);
    }

    while (index < item_rule_count - 1)
    {
        item_rules[index] = item_rules[index + 1];
        index++;
    }

    item_rule_count--;
    WIPE(&item_rules[item_rule_count], item_rule);
}

/* Sets the automatic note for one object kind. */
int item_rules_set_kind_note(s16b kind_idx, cptr note)
{
    int index;

    if (kind_idx == 0)
        return 0;

    if (!note || !note[0])
    {
        item_rules_clear_kind_note(kind_idx);
        return 1;
    }

    index = item_rules_find_kind_index(kind_idx);
    if (index < 0)
        index = item_rule_count;

    if (index >= ITEM_RULES_MAX)
    {
        msg_format("This inscription (%s) cannot be added, "
                   "because the rule array is full!",
            note);
        return 0;
    }

    item_rules[index].kind_idx = kind_idx;
    item_rules[index].note_idx = quark_add(note);

    if (index == item_rule_count)
        item_rule_count++;

    item_rules_apply_pack();
    item_rules_apply_dungeon();

    return 1;
}

/* Applies the automatic note for one object instance, if any. */
int item_rules_apply_note(object_type* o_ptr)
{
    cptr note = item_rules_kind_note(o_ptr->k_idx);
    cptr existing_note = quark_str(o_ptr->obj_note);

    if (!note)
        return 0;

    if (existing_note && streq(note, existing_note))
        return 0;

    o_ptr->obj_note = note[0] ? quark_add(note) : 0;
    return 1;
}

/* Applies automatic notes to all objects on the player's current grid. */
void item_rules_apply_ground(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    s16b this_o_idx;
    s16b next_o_idx = 0;

    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        next_o_idx = o_list[this_o_idx].next_o_idx;
        item_rules_apply_note(&o_list[this_o_idx]);
    }
}

/* Applies automatic notes to all carried items. */
void item_rules_apply_pack(void)
{
    int i;

    for (i = INVEN_PACK; i > 0; i--)
    {
        if (!inventory[i].k_idx)
            continue;

        item_rules_apply_note(&inventory[i]);
    }
}

/* Returns how many automatic note rules are currently stored. */
u16b item_rules_note_count(void)
{
    return item_rule_count;
}

/* Returns the object kind index for one stored automatic note rule. */
s16b item_rules_note_kind_at(u16b index)
{
    if (index >= item_rule_count)
        return 0;

    return item_rules[index].kind_idx;
}

/* Returns the note text for one stored automatic note rule. */
cptr item_rules_note_text_at(u16b index)
{
    if (index >= item_rule_count)
        return NULL;

    return quark_str(item_rules[index].note_idx);
}
