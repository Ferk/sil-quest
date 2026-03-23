/* File: ui-map-marks.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral transient map-mark state for overlays such as targeting
 * paths. Frontends can consume this semantic state however they like.
 */

#ifndef INCLUDED_UI_MAP_MARKS_H
#define INCLUDED_UI_MAP_MARKS_H

#include "angband.h"

typedef struct ui_map_mark ui_map_mark;

struct ui_map_mark
{
    int y;
    int x;
    byte attr;
    byte chr;
};

void ui_map_marks_begin(void);
void ui_map_mark_add(int y, int x, byte attr, char chr);
void ui_map_marks_end(void);
void ui_map_marks_clear(void);
bool ui_map_mark_lookup(int y, int x, byte* attr, byte* chr);
unsigned int ui_map_marks_get_revision(void);

#endif /* INCLUDED_UI_MAP_MARKS_H */
