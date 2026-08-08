/* SPDX-License-Identifier: EUPL-1.2 */
/*
 * Copyright (c) 2026 Fernando Carmona Varo
 *
 * Dialogue runtime entry points for quest-authored conversations.
 */

#ifndef INCLUDED_DIALOGUE_H
#define INCLUDED_DIALOGUE_H

#include "angband.h"

/*
 * Return TRUE if one monster has a quest-authored conversation.
 */
extern bool dialogue_monster_has_bump_dialogue(const monster_type* m_ptr);

/*
 * Return TRUE if the player can currently talk to one quest-authored NPC.
 */
extern bool dialogue_monster_can_talk(const monster_type* m_ptr);

/*
 * Open one quest-authored conversation for a specific monster.
 */
extern bool dialogue_handle_monster(monster_type* m_ptr);

/*
 * Web/mobile semantic dialogue surface.  These functions are intentionally
 * non-blocking so they can be called from frontend event handlers.
 */
extern bool dialogue_web_open_monster(monster_type* m_ptr);
extern bool dialogue_web_is_active(void);
extern unsigned int dialogue_web_get_revision(void);
extern cptr dialogue_web_get_title(void);
extern cptr dialogue_web_get_body(void);
extern cptr dialogue_web_get_options_json(void);
extern bool dialogue_web_choose(int key);
extern void dialogue_web_close(void);

/*
 * Return TRUE while the runtime is resolving an explicit player-requested
 * attack against a dialogue-capable peaceful monster.
 */
extern bool dialogue_force_attack_active(void);

/*
 * Resolve one explicit attack against a dialogue-capable adjacent monster.
 */
extern bool dialogue_force_attack_adjacent(int dir);

/*
 * Handle bumping into one peaceful monster as a conversation.
 *
 * Returns TRUE if a dialogue consumed the interaction and the caller should not
 * continue with any legacy peaceful-monster handling.
 */
extern bool dialogue_handle_bump(monster_type* m_ptr);

#endif /* INCLUDED_DIALOGUE_H */
