/* File: ui-map-marks.c
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

#include "angband.h"

#include "ui-map-marks.h"

#define UI_MAP_MARKS_MAX 256

static ui_map_mark ui_map_marks[UI_MAP_MARKS_MAX];
static int ui_map_mark_count = 0;
static bool ui_map_marks_batch = FALSE;
static unsigned int ui_map_marks_revision = 1;

static void ui_map_marks_touch(void)
{
    ui_map_marks_revision++;
    if (ui_map_marks_revision == 0)
        ui_map_marks_revision = 1;
}

static int ui_map_mark_find(int y, int x)
{
    int i;

    for (i = 0; i < ui_map_mark_count; i++)
    {
        if ((ui_map_marks[i].y == y) && (ui_map_marks[i].x == x))
            return i;
    }

    return -1;
}

void ui_map_marks_begin(void)
{
    ui_map_mark_count = 0;
    ui_map_marks_batch = TRUE;
}

void ui_map_mark_add(int y, int x, byte attr, char chr)
{
    int i;
    bool changed = FALSE;
    byte mark_chr;

    if (!in_bounds(y, x))
        return;

    mark_chr = chr ? (byte)chr : (byte)'*';
    i = ui_map_mark_find(y, x);

    if (i >= 0)
    {
        if ((ui_map_marks[i].attr != attr) || (ui_map_marks[i].chr != mark_chr))
        {
            ui_map_marks[i].attr = attr;
            ui_map_marks[i].chr = mark_chr;
            changed = TRUE;
        }
    }
    else if (ui_map_mark_count < UI_MAP_MARKS_MAX)
    {
        ui_map_marks[ui_map_mark_count].y = y;
        ui_map_marks[ui_map_mark_count].x = x;
        ui_map_marks[ui_map_mark_count].attr = attr;
        ui_map_marks[ui_map_mark_count].chr = mark_chr;
        ui_map_mark_count++;
        changed = TRUE;
    }

    if (changed && !ui_map_marks_batch)
        ui_map_marks_touch();
}

void ui_map_marks_end(void)
{
    if (!ui_map_marks_batch)
        return;

    ui_map_marks_batch = FALSE;
    ui_map_marks_touch();
}

void ui_map_marks_clear(void)
{
    bool had_marks = (ui_map_mark_count > 0) || ui_map_marks_batch;

    ui_map_marks_batch = FALSE;
    ui_map_mark_count = 0;

    if (had_marks)
        ui_map_marks_touch();
}

bool ui_map_mark_lookup(int y, int x, byte* attr, byte* chr)
{
    int i = ui_map_mark_find(y, x);

    if (i < 0)
        return FALSE;

    if (attr)
        *attr = ui_map_marks[i].attr;
    if (chr)
        *chr = ui_map_marks[i].chr;

    return TRUE;
}

unsigned int ui_map_marks_get_revision(void)
{
    return ui_map_marks_revision;
}
