/* File: item-rules.h
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

#ifndef INCLUDED_ITEM_RULES_H
#define INCLUDED_ITEM_RULES_H

#include "angband.h"

/* Clears all stored item kind rules. */
void item_rules_clear(void);
/* Prompts for the automatic note attached to one object kind. */
int item_rules_prompt_kind_note(s16b kind_idx);
/* Prompts to clear the automatic note stored for one object kind. */
bool item_rules_confirm_clear_kind_note(s16b kind_idx);
/* Prompts to store one automatic note for all items of one kind. */
bool item_rules_confirm_set_kind_note(s16b kind_idx, cptr note);
/* Returns whether one object kind currently has an automatic note. */
bool item_rules_kind_has_note(s16b kind_idx);
/* Removes the automatic note for one object kind. */
void item_rules_clear_kind_note(s16b kind_idx);
/* Sets the automatic note for one object kind. */
int item_rules_set_kind_note(s16b kind_idx, cptr note);
/* Applies the automatic note for one object instance, if any. */
int item_rules_apply_note(object_type* o_ptr);
/* Applies automatic notes to all objects on the player's current grid. */
void item_rules_apply_ground(void);
/* Applies automatic notes to all carried items. */
void item_rules_apply_pack(void);
/* Returns how many automatic note rules are currently stored. */
u16b item_rules_note_count(void);
/* Returns the object kind index for one stored automatic note rule. */
s16b item_rules_note_kind_at(u16b index);
/* Returns the note text for one stored automatic note rule. */
cptr item_rules_note_text_at(u16b index);

#endif /* INCLUDED_ITEM_RULES_H */
