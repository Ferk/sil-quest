/* File: ui-marks.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral state for transient marks drawn over the map, such as
 * targeting paths and impact previews. Frontends can consume this semantic
 * state however they like.
 */

#ifndef INCLUDED_UI_MARKS_H
#define INCLUDED_UI_MARKS_H

#include "angband.h"

typedef struct ui_mark ui_mark;

struct ui_mark
{
    int y;
    int x;
    byte attr;
    byte chr;
};

/* Begins publishing one fresh transient map-mark frame. */
void ui_marks_begin(void);
/* Adds one transient mark at the given world-grid position. */
void ui_marks_add(int y, int x, byte attr, char chr);
/* Marks the current transient map-mark frame as complete. */
void ui_marks_end(void);
/* Clears all currently published transient map marks. */
void ui_marks_clear(void);
/* Returns the mark published for one world-grid cell, if any. */
bool ui_marks_lookup(int y, int x, byte* attr, byte* chr);
/* Returns the mark revision used by frontend caches. */
unsigned int ui_marks_get_revision(void);
/* Sets the preview visual used for projectile path marks. */
void ui_marks_set_projectile_visual(byte attr, char chr);
/* Clears any projectile preview visual override. */
void ui_marks_clear_projectile_visual(void);
/* Prepares projectile and impact visuals for fired ammunition previews. */
void ui_marks_prepare_fire_projectile_visual(object_type* o_ptr);
/* Prepares projectile and impact visuals for thrown-item previews. */
void ui_marks_prepare_throw_projectile_visual(object_type* o_ptr);
/* Resolves the live impact visual used for fired ammunition. */
void ui_marks_resolve_fire_impact_visual(int typ, byte* attr, char* chr);
/* Resolves the live impact visual used for thrown items. */
void ui_marks_resolve_throw_impact_visual(int typ, byte* attr, char* chr);
/* Resolves one preview mark for a projectile path or impact. */
void ui_marks_resolve_projectile_visual(int y, int x, int ny, int nx,
    bool impact, int typ, byte* attr, char* chr);
/* Marks whether interactive targeting mode is currently active. */
void ui_marks_set_targeting_active(bool active);
/* Returns whether interactive targeting mode is currently active. */
bool ui_marks_targeting_active(void);
/* Marks whether an aim prompt is currently offering target reuse. */
void ui_marks_set_target_prompt_active(bool active);
/* Places the cursor on the current target when target highlighting applies. */
void ui_marks_place_target_cursor(void);
/* Returns whether the stored target should currently be highlighted. */
bool ui_marks_should_highlight_target(void);
/* Queues one requested target-grid change from a frontend interaction. */
void ui_marks_request_target_grid(int y, int x);
/* Returns and clears one queued target-grid request, if any. */
bool ui_marks_take_target_grid_request(int* y, int* x);

#endif /* INCLUDED_UI_MARKS_H */
