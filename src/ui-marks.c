/* File: ui-marks.c
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

#include "angband.h"

#include "ui-marks.h"
#include "ui-model.h"

#define UI_MARKS_MAX 256
#define UI_MARKS_IMPACT_SPECIAL_NONE (-1)
#define UI_MARKS_IMPACT_SPECIAL_GENERIC 0x39
#define UI_MARKS_IMPACT_SPECIAL_ARROW 0x3F

static ui_mark ui_marks[UI_MARKS_MAX];
static int ui_marks_count = 0;
static bool ui_marks_batch = FALSE;
static unsigned int ui_marks_revision = 1;
static bool ui_marks_projectile_visual_active = FALSE;
static byte ui_marks_projectile_attr = TERM_WHITE;
static char ui_marks_projectile_char = '*';
static int ui_marks_projectile_impact_special = UI_MARKS_IMPACT_SPECIAL_NONE;
static bool ui_marks_targeting_active_state = FALSE;
static bool ui_marks_target_prompt_active_state = FALSE;
static bool ui_marks_target_grid_request_active_state = FALSE;
static int ui_marks_target_grid_request_y_state = 0;
static int ui_marks_target_grid_request_x_state = 0;

static void ui_marks_resolve_impact_special(
    int special, int typ, byte* attr, char* chr)
{
    u16b pict;
    byte resolved_attr;
    char resolved_chr;

    if (!graphics_are_ascii() && (special != UI_MARKS_IMPACT_SPECIAL_NONE))
    {
        resolved_attr = misc_to_attr[special & 0xFF];
        resolved_chr = misc_to_char[special & 0xFF];
    }
    else
    {
        pict = bolt_pict(0, 0, 0, 0, typ);
        resolved_attr = PICT_A(pict);
        resolved_chr = PICT_C(pict);
    }

    if (attr)
        *attr = resolved_attr;
    if (chr)
        *chr = resolved_chr;
}

static void ui_marks_touch(void)
{
    ui_marks_revision++;
    if (ui_marks_revision == 0)
        ui_marks_revision = 1;
    ui_front_invalidate();
}

static int ui_marks_find(int y, int x)
{
    int i;

    for (i = 0; i < ui_marks_count; i++)
    {
        if ((ui_marks[i].y == y) && (ui_marks[i].x == x))
            return i;
    }

    return -1;
}

void ui_marks_begin(void)
{
    ui_marks_count = 0;
    ui_marks_batch = TRUE;
}

void ui_marks_add(int y, int x, byte attr, char chr)
{
    int i;
    bool changed = FALSE;
    byte mark_chr;

    if (!in_bounds(y, x))
        return;

    mark_chr = chr ? (byte)chr : (byte)'*';
    i = ui_marks_find(y, x);

    if (i >= 0)
    {
        if ((ui_marks[i].attr != attr) || (ui_marks[i].chr != mark_chr))
        {
            ui_marks[i].attr = attr;
            ui_marks[i].chr = mark_chr;
            changed = TRUE;
        }
    }
    else if (ui_marks_count < UI_MARKS_MAX)
    {
        ui_marks[ui_marks_count].y = y;
        ui_marks[ui_marks_count].x = x;
        ui_marks[ui_marks_count].attr = attr;
        ui_marks[ui_marks_count].chr = mark_chr;
        ui_marks_count++;
        changed = TRUE;
    }

    if (changed && !ui_marks_batch)
        ui_marks_touch();
}

void ui_marks_end(void)
{
    if (!ui_marks_batch)
        return;

    ui_marks_batch = FALSE;
    ui_marks_touch();
}

void ui_marks_clear(void)
{
    bool had_marks = (ui_marks_count > 0) || ui_marks_batch;

    ui_marks_batch = FALSE;
    ui_marks_count = 0;

    if (had_marks)
        ui_marks_touch();
}

bool ui_marks_lookup(int y, int x, byte* attr, byte* chr)
{
    int i = ui_marks_find(y, x);

    if (i < 0)
        return FALSE;

    if (attr)
        *attr = ui_marks[i].attr;
    if (chr)
        *chr = ui_marks[i].chr;

    return TRUE;
}

unsigned int ui_marks_get_revision(void)
{
    return ui_marks_revision;
}

void ui_marks_set_projectile_visual(byte attr, char chr)
{
    ui_marks_projectile_visual_active = TRUE;
    ui_marks_projectile_attr = attr;
    ui_marks_projectile_char = chr ? chr : '*';
}

void ui_marks_clear_projectile_visual(void)
{
    ui_marks_projectile_visual_active = FALSE;
    ui_marks_projectile_attr = TERM_WHITE;
    ui_marks_projectile_char = '*';
    ui_marks_projectile_impact_special = UI_MARKS_IMPACT_SPECIAL_NONE;
}

void ui_marks_prepare_fire_projectile_visual(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
    {
        ui_marks_clear_projectile_visual();
        return;
    }

    if (graphics_are_ascii())
    {
        ui_marks_set_projectile_visual(
            (byte)object_attr(o_ptr), object_char(o_ptr));
    }
    else
    {
        ui_marks_clear_projectile_visual();
        ui_marks_projectile_impact_special = UI_MARKS_IMPACT_SPECIAL_ARROW;
    }
}

void ui_marks_prepare_throw_projectile_visual(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
    {
        ui_marks_clear_projectile_visual();
        return;
    }

    ui_marks_set_projectile_visual(
        (byte)object_attr(o_ptr), object_char(o_ptr));

    if (!graphics_are_ascii())
        ui_marks_projectile_impact_special = UI_MARKS_IMPACT_SPECIAL_GENERIC;
}

void ui_marks_resolve_fire_impact_visual(int typ, byte* attr, char* chr)
{
    ui_marks_resolve_impact_special(
        UI_MARKS_IMPACT_SPECIAL_ARROW, typ, attr, chr);
}

void ui_marks_resolve_throw_impact_visual(int typ, byte* attr, char* chr)
{
    ui_marks_resolve_impact_special(
        UI_MARKS_IMPACT_SPECIAL_GENERIC, typ, attr, chr);
}

void ui_marks_resolve_projectile_visual(int y, int x, int ny, int nx,
    bool impact, int typ, byte* attr, char* chr)
{
    u16b pict;
    byte resolved_attr;
    char resolved_chr;

    if (impact)
    {
        ui_marks_resolve_impact_special(
            ui_marks_projectile_impact_special, typ, &resolved_attr, &resolved_chr);
    }
    else if (ui_marks_projectile_visual_active)
    {
        resolved_attr = ui_marks_projectile_attr;
        resolved_chr = ui_marks_projectile_char;
    }
    else
    {
        pict = bolt_pict(y, x, ny, nx, typ);
        resolved_attr = PICT_A(pict);
        resolved_chr = PICT_C(pict);
    }

    if (attr)
        *attr = resolved_attr;
    if (chr)
        *chr = resolved_chr;
}

void ui_marks_set_targeting_active(bool active)
{
    ui_marks_targeting_active_state = active ? TRUE : FALSE;

    if (!ui_marks_targeting_active_state)
        ui_marks_target_grid_request_active_state = FALSE;
}

bool ui_marks_targeting_active(void)
{
    return ui_marks_targeting_active_state;
}

void ui_marks_set_target_prompt_active(bool active)
{
    ui_marks_target_prompt_active_state = active ? TRUE : FALSE;
}

void ui_marks_place_target_cursor(void)
{
    if (hilite_target && ui_marks_should_highlight_target() && target_sighted())
        move_cursor_relative(p_ptr->target_row, p_ptr->target_col);
}

bool ui_marks_should_highlight_target(void)
{
    return ui_marks_targeting_active_state || ui_marks_target_prompt_active_state;
}

void ui_marks_request_target_grid(int y, int x)
{
    if (!ui_marks_targeting_active_state || !in_bounds(y, x))
        return;

    ui_marks_target_grid_request_y_state = y;
    ui_marks_target_grid_request_x_state = x;
    ui_marks_target_grid_request_active_state = TRUE;
}

bool ui_marks_take_target_grid_request(int* y, int* x)
{
    if (!ui_marks_target_grid_request_active_state)
        return FALSE;

    ui_marks_target_grid_request_active_state = FALSE;

    if (y)
        *y = ui_marks_target_grid_request_y_state;
    if (x)
        *x = ui_marks_target_grid_request_x_state;

    return TRUE;
}
