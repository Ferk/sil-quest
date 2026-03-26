/* File: main-web.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * This file provides a web frontend for Emscripten builds.
 * It exports per-cell render metadata so JavaScript can render text or tiles.
 */

#include "angband.h"

#ifdef USE_WEB

#include "main.h"
#include "ui-birth.h"
#include "ui-character.h"
#include "ui-marks.h"
#include "ui-model.h"

#include <stdint.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#include <unistd.h>
#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif
#endif

typedef struct web_cell web_cell;
typedef struct term_data term_data;

/* ------------------------------------------------------------------------ */
/* Cell Metadata                                                            */
/* ------------------------------------------------------------------------ */

/* Cell render modes */
#define WEB_CELL_EMPTY 0
#define WEB_CELL_TEXT 1
#define WEB_CELL_PICT 2

/* Overlay flags (same semantics as x11/win compositing) */
#define WEB_FLAG_ALERT 0x01
#define WEB_FLAG_GLOW 0x02
#define WEB_FLAG_FG_PICT 0x04
#define WEB_FLAG_BG_PICT 0x08
#define WEB_FLAG_MARK 0x10
struct web_cell
{
    byte kind;

    byte text_attr;
    byte text_char;

    byte fg_attr;
    byte fg_char;

    byte bg_attr;
    byte bg_char;

    byte flags;
};

struct term_data
{
    term t;

    int cols;
    int rows;

    web_cell* cells;

    int cursor_x;
    int cursor_y;
    bool cursor_visible;
    uint32_t frame_id;
};

/* ------------------------------------------------------------------------ */
/* Global Web Frontend State                                                */
/* ------------------------------------------------------------------------ */

static term_data data;

#define WEB_LOG_TEXT_MAX 4096
#define WEB_LOG_MESSAGES_MAX 20
#define WEB_OVERLAY_TEXT_MAX 16384
#define WEB_BIRTH_STATE_MAX 4096
#define WEB_CHARACTER_SHEET_STATE_MAX 8192
#define WEB_PLAYER_STATE_MAX 8192
#define WEB_TILE_CONTEXT_STATE_MAX 4096
#define WEB_AUTO_RESUME_MARKER_NAME "web-autoresume.txt"

struct web_birth_submission
{
    bool pending;
    enum ui_birth_screen_kind kind;
    char text[UI_BIRTH_HISTORY_TEXT_MAX];
    int age;
    int height;
    int weight;
};

static char web_log_text[WEB_LOG_TEXT_MAX];
static byte web_log_attrs[WEB_LOG_TEXT_MAX];
static int web_log_attrs_len = 0;
static uint32_t web_log_text_frame = UINT32_MAX;
static char web_birth_state[WEB_BIRTH_STATE_MAX];
static uint32_t web_birth_state_revision = UINT32_MAX;
static struct web_birth_submission web_birth_submission = { 0 };
static char web_character_sheet_state[WEB_CHARACTER_SHEET_STATE_MAX];
static uint32_t web_character_sheet_state_revision = UINT32_MAX;
static char web_player_state[WEB_PLAYER_STATE_MAX];
static char web_tile_context_state[WEB_TILE_CONTEXT_STATE_MAX];
static char web_overlay_text[WEB_OVERLAY_TEXT_MAX];
static byte web_overlay_attrs[WEB_OVERLAY_TEXT_MAX];
static int web_overlay_attrs_len = 0;
static uint32_t web_overlay_text_frame = UINT32_MAX;
static uint32_t web_fx_delay_seq = 0;
static int web_fx_delay_msec = 0;
static web_cell* web_fx_cells = NULL;
static int web_fx_cell_count = 0;
static int web_fx_cols = 0;
static int web_fx_rows = 0;
static int web_overlay_mode = 0;
static byte* web_overlay_capture = NULL;
static byte* web_overlay_capture_attr = NULL;
static bool web_overlay_capture_active = FALSE;
static web_cell* web_map_cells = NULL;
static int web_map_cell_count = 0;
static uint32_t web_map_cells_frame = UINT32_MAX;
static unsigned int web_saved_screen_revision = 0;
static unsigned int web_map_marks_revision = 0;

static void web_refresh_map_cells(void);
static void web_capture_fx_cells(void);
static void web_set_text_cell(web_cell* cell, byte attr, byte chr);
static void web_mark_dirty(term_data* td, int x, int y, int w, int h);
static void web_overlay_capture_clear(void);
static void web_get_auto_resume_marker_path(char* buf, size_t max);

/* MicroChasm defaults */
static int web_tile_wid = 16;
static int web_tile_hgt = 16;

#define WEB_KEY_QUEUE_SIZE 4096

static int key_queue[WEB_KEY_QUEUE_SIZE];
static int key_head = 0;
static int key_tail = 0;

#define WEB_SOUND_QUEUE_SIZE 64
#define WEB_SOUND_EVENT_NOISE 0

static int web_sound_queue[WEB_SOUND_QUEUE_SIZE];
static int web_sound_head = 0;
static int web_sound_tail = 0;

/* ------------------------------------------------------------------------ */
/* WASM Export Declarations                                                 */
/* ------------------------------------------------------------------------ */

EMSCRIPTEN_KEEPALIVE int web_get_cols(void);
EMSCRIPTEN_KEEPALIVE int web_get_rows(void);
EMSCRIPTEN_KEEPALIVE int web_get_cell_stride(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_cells_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_tile_wid(void);
EMSCRIPTEN_KEEPALIVE int web_get_tile_hgt(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_col(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_row(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_cols(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_rows(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_x_step(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_world_x(void);
EMSCRIPTEN_KEEPALIVE int web_get_map_world_y(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_map_cells_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_player_map_x(void);
EMSCRIPTEN_KEEPALIVE int web_get_player_map_y(void);
EMSCRIPTEN_KEEPALIVE int web_get_icon_alert_attr(void);
EMSCRIPTEN_KEEPALIVE int web_get_icon_alert_char(void);
EMSCRIPTEN_KEEPALIVE int web_get_icon_glow_attr(void);
EMSCRIPTEN_KEEPALIVE int web_get_icon_glow_char(void);
EMSCRIPTEN_KEEPALIVE uint32_t web_get_frame_id(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_fx_cells_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_fx_cells_cols(void);
EMSCRIPTEN_KEEPALIVE int web_get_fx_cells_rows(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_items_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_item_count(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_item_stride(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_text_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_attrs_len(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_active_x(void);
EMSCRIPTEN_KEEPALIVE unsigned int web_get_menu_revision(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_snapshot_retained(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_details_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_details_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_attrs_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_summary_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_summary_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_attrs_len(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_width(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_rows(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_kind(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_attr(void);
EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_char(void);
EMSCRIPTEN_KEEPALIVE int web_menu_hover(int index);
EMSCRIPTEN_KEEPALIVE int web_menu_activate(int index);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_modal_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_modal_text_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_modal_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_modal_attrs_len(void);
EMSCRIPTEN_KEEPALIVE int web_get_modal_dismiss_key(void);
EMSCRIPTEN_KEEPALIVE unsigned int web_get_modal_revision(void);
EMSCRIPTEN_KEEPALIVE int web_get_prompt_kind(void);
EMSCRIPTEN_KEEPALIVE int web_get_prompt_more_hint(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_prompt_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_prompt_text_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_prompt_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_prompt_attrs_len(void);
EMSCRIPTEN_KEEPALIVE unsigned int web_get_prompt_revision(void);
EMSCRIPTEN_KEEPALIVE int web_modal_activate(void);
EMSCRIPTEN_KEEPALIVE int web_get_cursor_x(void);
EMSCRIPTEN_KEEPALIVE int web_get_cursor_y(void);
EMSCRIPTEN_KEEPALIVE int web_get_cursor_visible(void);
EMSCRIPTEN_KEEPALIVE int web_act_here(void);
EMSCRIPTEN_KEEPALIVE int web_act_adjacent(int dir);
EMSCRIPTEN_KEEPALIVE int web_open_inventory(void);
EMSCRIPTEN_KEEPALIVE int web_open_ranged_target(void);
EMSCRIPTEN_KEEPALIVE int web_toggle_stealth(void);
EMSCRIPTEN_KEEPALIVE int web_open_song_menu(void);
EMSCRIPTEN_KEEPALIVE int web_save_game_automatically(void);
EMSCRIPTEN_KEEPALIVE int web_request_autosave(void);
EMSCRIPTEN_KEEPALIVE int web_target_map(int y, int x);
EMSCRIPTEN_KEEPALIVE int web_travel_to(int y, int x);
EMSCRIPTEN_KEEPALIVE int web_push_key(int key);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_log_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_log_text_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_log_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_log_attrs_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_birth_state_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_birth_state_len(void);
EMSCRIPTEN_KEEPALIVE unsigned int web_get_birth_state_revision(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_birth_submit_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_submit_birth_form(
    int kind, int age, int height, int weight);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_character_sheet_state_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_character_sheet_state_len(void);
EMSCRIPTEN_KEEPALIVE unsigned int web_get_character_sheet_state_revision(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_player_state_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_player_state_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_tile_context_state_ptr(int dir);
EMSCRIPTEN_KEEPALIVE int web_get_tile_context_state_len(int dir);
EMSCRIPTEN_KEEPALIVE int web_execute_tile_context_action(int dir, int action_id);
EMSCRIPTEN_KEEPALIVE int web_get_sound_count(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_sound_name_ptr(int index);
EMSCRIPTEN_KEEPALIVE int web_get_sound_name_len(int index);
EMSCRIPTEN_KEEPALIVE int web_pop_sound_event(void);
EMSCRIPTEN_KEEPALIVE int web_get_overlay_mode(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_overlay_text_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_overlay_text_len(void);
EMSCRIPTEN_KEEPALIVE uintptr_t web_get_overlay_attrs_ptr(void);
EMSCRIPTEN_KEEPALIVE int web_get_overlay_attrs_len(void);
EMSCRIPTEN_KEEPALIVE uint32_t web_get_fx_delay_seq(void);
EMSCRIPTEN_KEEPALIVE int web_get_fx_delay_msec(void);
/* ------------------------------------------------------------------------ */
/* Shared Helpers                                                           */
/* ------------------------------------------------------------------------ */

/*
 * Clip a single-row span against terminal bounds.
 * If src_skip is provided, it receives the number of source cells to skip.
 */
static bool web_clip_row_span(
    int cols, int rows, int y, int* x, int* n, int* src_skip)
{
    int sx;
    int sn;
    int skip = 0;

    if (!x || !n)
        return FALSE;

    sx = *x;
    sn = *n;

    if ((y < 0) || (y >= rows) || (sn <= 0))
        return FALSE;

    if (sx < 0)
    {
        skip = -sx;
        sn += sx;
        sx = 0;
    }

    if (sx >= cols)
        return FALSE;

    if (sx + sn > cols)
        sn = cols - sx;

    if (sn <= 0)
        return FALSE;

    *x = sx;
    *n = sn;
    if (src_skip)
        *src_skip = skip;

    return TRUE;
}

/* Translate angband pict metadata into web-facing flags. */
static byte web_pict_flags(byte a, byte c, byte ta, byte tc)
{
    byte flags = 0;

    if (c & GRAPHICS_ALERT_MASK)
        flags |= WEB_FLAG_ALERT;
    if (a & GRAPHICS_GLOW_MASK)
        flags |= WEB_FLAG_GLOW;
    if ((a & 0x80) && (c & 0x80))
        flags |= WEB_FLAG_FG_PICT;
    if ((ta & 0x80) && (tc & 0x80))
        flags |= WEB_FLAG_BG_PICT;

    return flags;
}

/* Invalidate the map snapshot cache and request a map-area redraw. */
static void web_invalidate_map_snapshot(void)
{
    if ((data.cols <= 0) || (data.rows <= 0))
        return;

    web_map_cells_frame = UINT32_MAX;
    web_mark_dirty(&data, COL_MAP, ROW_MAP,
        web_get_map_cols() * web_get_map_x_step(), web_get_map_rows());
}

/* Invalidates cached map cells when semantic map marks changed. */
static void web_sync_map_marks_revision(void)
{
    unsigned int revision = ui_marks_get_revision();

    if (revision == web_map_marks_revision)
        return;

    web_map_marks_revision = revision;
    web_invalidate_map_snapshot();
}

/* Read mark metadata for a world-grid cell, if any. */
static bool web_get_target_mark(int y, int x, byte* attr, byte* chr)
{
    return ui_marks_lookup(y, x, attr, chr);
}

/* Clears any staged web birth-form submission. */
static void web_clear_birth_submission(void)
{
    WIPE(&web_birth_submission, struct web_birth_submission);
}

/* Builds the persisted marker path used to remember the latest resumable save. */
static void web_get_auto_resume_marker_path(char* buf, size_t max)
{
    if (!buf || (max == 0))
        return;

    if (!ANGBAND_DIR_USER)
    {
        buf[0] = '\0';
        return;
    }

    path_build(buf, max, ANGBAND_DIR_USER, WEB_AUTO_RESUME_MARKER_NAME);
}

/* Stores or clears the savefile marker used for automatic web resume. */
void web_update_auto_resume_marker(int active)
{
    char marker_path[1024];

    web_get_auto_resume_marker_path(marker_path, sizeof(marker_path));
    if (!marker_path[0])
        return;

    if (!active || !savefile[0] || !ANGBAND_DIR_SAVE
        || !prefix(savefile, ANGBAND_DIR_SAVE))
    {
        fd_kill(marker_path);
        return;
    }

    FILE_TYPE(FILE_TYPE_TEXT);

    {
        FILE* marker = my_fopen(marker_path, "w");

        if (!marker)
            return;

        (void)my_fputs(marker, savefile, strlen(savefile));
        (void)my_fputs(marker, "\n", 1);
        (void)my_fclose(marker);
    }
}

/* Consumes one accepted birth submission for the active web birth screen. */
static bool web_consume_birth_submission(
    enum ui_birth_screen_kind kind, char* text, size_t text_size, int* age,
    int* height, int* weight)
{
    if (!web_birth_submission.pending || (web_birth_submission.kind != kind))
        return FALSE;

    if ((kind == UI_BIRTH_SCREEN_AHW) && age && height && weight)
    {
        *age = web_birth_submission.age;
        *height = web_birth_submission.height;
        *weight = web_birth_submission.weight;
    }
    else if (text && text_size)
    {
        my_strcpy(text, web_birth_submission.text, text_size);
    }
    else
    {
        return FALSE;
    }

    web_clear_birth_submission();
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Render Invalidation                                                      */
/* ------------------------------------------------------------------------ */

static void web_mark_dirty(term_data* td, int x, int y, int w, int h)
{
    if (!td || (w <= 0) || (h <= 0))
        return;

    (void)x;
    (void)y;

    td->frame_id++;
}

static void web_mark_cursor_dirty(term_data* td)
{
    if (!td)
        return;

    if ((td->cursor_x < 0) || (td->cursor_y < 0))
        return;

    if ((td->cursor_x >= td->cols) || (td->cursor_y >= td->rows))
        return;

    web_mark_dirty(td, td->cursor_x, td->cursor_y, 1, 1);
}

/* ------------------------------------------------------------------------ */
/* FX Snapshot Capture                                                      */
/* ------------------------------------------------------------------------ */

static void web_ensure_fx_cells(int count)
{
    if (count < 0)
        count = 0;

    if (count == web_fx_cell_count)
        return;

    FREE(web_fx_cells);
    web_fx_cells = NULL;
    web_fx_cell_count = 0;

    if (count <= 0)
        return;

    C_MAKE(web_fx_cells, (size_t)count, web_cell);
    web_fx_cell_count = count;
}

static void web_capture_fx_cells(void)
{
    int cols = web_get_map_cols();
    int rows = web_get_map_rows();
    int step = use_bigtile ? 2 : 1;
    int count = cols * rows;
    int my;
    int mx;

    web_ensure_fx_cells(count);
    web_fx_cols = cols;
    web_fx_rows = rows;

    if (!web_fx_cells || !data.cells || (cols <= 0) || (rows <= 0))
        return;

    for (my = 0; my < rows; my++)
    {
        for (mx = 0; mx < cols; mx++)
        {
            int sx = COL_MAP + mx * step;
            int sy = ROW_MAP + my;
            int idx = my * cols + mx;

            if ((sx >= 0) && (sx < data.cols) && (sy >= 0) && (sy < data.rows))
            {
                web_fx_cells[idx] = data.cells[sy * data.cols + sx];
            }
            else
            {
                web_set_text_cell(&web_fx_cells[idx], TERM_WHITE, (byte)' ');
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Input Queue                                                              */
/* ------------------------------------------------------------------------ */

static bool web_key_enqueue(int key)
{
    int next = (key_head + 1) % WEB_KEY_QUEUE_SIZE;

    if (next == key_tail)
        return FALSE;

    key_queue[key_head] = key;
    key_head = next;

    return TRUE;
}

static bool web_key_dequeue(int* key)
{
    if (key_head == key_tail)
        return FALSE;

    *key = key_queue[key_tail];
    key_tail = (key_tail + 1) % WEB_KEY_QUEUE_SIZE;

    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Sound Queue                                                              */
/* ------------------------------------------------------------------------ */

/* Pushes one sound event into the web-facing queue, dropping the oldest if full. */
static void web_sound_queue_push(int sound_event)
{
    int next_head;

    next_head = (web_sound_head + 1) % WEB_SOUND_QUEUE_SIZE;
    if (next_head == web_sound_tail)
        web_sound_tail = (web_sound_tail + 1) % WEB_SOUND_QUEUE_SIZE;

    web_sound_queue[web_sound_head] = sound_event;
    web_sound_head = next_head;
}

static void web_queue_sound_event(int sound_event)
{
    if ((sound_event <= 0) || (sound_event >= SOUND_MAX))
        return;

    web_sound_queue_push(sound_event);
}

static void web_queue_noise_event(void)
{
    web_sound_queue_push(WEB_SOUND_EVENT_NOISE);
}

static void web_clear_sound_events(void)
{
    web_sound_head = 0;
    web_sound_tail = 0;
}

/* ------------------------------------------------------------------------ */
/* Cell Operations                                                          */
/* ------------------------------------------------------------------------ */

static void web_set_text_cell(web_cell* cell, byte attr, byte chr)
{
    if (!cell)
        return;

    cell->kind = WEB_CELL_TEXT;
    cell->text_attr = attr;
    cell->text_char = chr;

    cell->fg_attr = 0;
    cell->fg_char = 0;
    cell->bg_attr = 0;
    cell->bg_char = 0;
    cell->flags = 0;
}

static void web_set_pict_cell(
    web_cell* cell, byte a, byte c, byte ta, byte tc, byte flags)
{
    if (!cell)
        return;

    cell->kind = WEB_CELL_PICT;

    cell->text_attr = 0;
    cell->text_char = 0;

    cell->fg_attr = a;
    cell->fg_char = c;
    cell->bg_attr = ta;
    cell->bg_char = tc;
    cell->flags = flags;
}

static void web_clear_cells(term_data* td, byte attr, byte chr)
{
    int x;
    int y;

    if (!td || !td->cells)
        return;

    for (y = 0; y < td->rows; y++)
    {
        for (x = 0; x < td->cols; x++)
        {
            web_set_text_cell(&td->cells[y * td->cols + x], attr, chr);
        }
    }
}

static void web_sleep_msec(int msec)
{
    if (msec <= 0)
        return;

#ifdef __EMSCRIPTEN__
    emscripten_sleep(msec);
#else
    usleep((unsigned int)msec * 1000U);
#endif
}

/* ------------------------------------------------------------------------ */
/* Semantic Side/Log UI Builders                                            */
/* ------------------------------------------------------------------------ */

static cptr web_get_race_name(void)
{
    if (!z_info || !p_info || !p_name || !p_ptr)
        return NULL;

    if (p_ptr->prace >= z_info->p_max)
        return NULL;

    return p_name + p_info[p_ptr->prace].name;
}

static cptr web_get_house_name(void)
{
    if (!z_info || !c_info || !c_name || !p_ptr)
        return NULL;

    if (p_ptr->phouse >= z_info->c_max)
        return NULL;

    return c_name + c_info[p_ptr->phouse].name;
}

static cptr web_get_song_name(byte song)
{
    int idx;
    cptr name;

    if (song == SNG_NOTHING)
        return NULL;

    if (!z_info || !b_info || !b_name)
        return NULL;

    idx = ability_index(S_SNG, song);
    if ((idx < 0) || (idx >= z_info->b_max))
        return NULL;

    name = b_name + b_info[idx].name;
    if (!name || !name[0])
        return NULL;

    if (strncmp(name, "Song of ", 8) == 0)
        name += 8;
    else if (strncmp(name, "Theme of ", 9) == 0)
        name += 9;

    if (strncmp(name, "the ", 4) == 0)
        name += 4;

    return name;
}

static void web_get_tracked_monster_state(char* name, size_t name_size,
    char* hp_bar, size_t hp_bar_size, char* alert, size_t alert_size)
{
    monster_type* m_ptr;
    int len;
    int color = TERM_WHITE;
    int i;
    cptr pattern = "********";

    if (!name || !name_size || !hp_bar || !hp_bar_size || !alert || !alert_size)
        return;

    my_strcpy(name, "(none)", name_size);
    hp_bar[0] = '\0';
    alert[0] = '\0';

    if (!p_ptr)
        return;

    if (p_ptr->health_who <= 0)
        return;

    m_ptr = &mon_list[p_ptr->health_who];
    if (!m_ptr->r_idx || !m_ptr->ml || p_ptr->image || (m_ptr->hp <= 0)
        || (m_ptr->maxhp <= 0))
    {
        my_strcpy(name, "(not visible)", name_size);
        return;
    }

    monster_desc(name, name_size, m_ptr, 0);
    my_strcpy(hp_bar, "--------", hp_bar_size);

    len = (8 * m_ptr->hp + m_ptr->maxhp - 1) / m_ptr->maxhp;
    if (len < 0)
        len = 0;
    if (len > 8)
        len = 8;

    if (m_ptr->confused && m_ptr->stunned)
        pattern = "cscscscs";
    else if (m_ptr->confused)
        pattern = "cccccccc";
    else if (m_ptr->stunned)
        pattern = "ssssssss";

    for (i = 0; i < len; i++)
        hp_bar[i] = pattern[i];

    (void)get_alertness_text(m_ptr, alert_size, alert, &color);
}

static void web_append_list_item(
    char* buf, size_t buf_size, size_t* off, cptr item)
{
    if (!buf || !off || !item || !item[0])
        return;

    if (*off > 0)
        strnfcat(buf, buf_size, off, ", ");

    strnfcat(buf, buf_size, off, "%s", item);
}

static void web_get_state_text(char* out, size_t out_size)
{
    if (!out || (out_size == 0) || !p_ptr)
        return;

    if (p_ptr->entranced)
    {
        my_strcpy(out, "Entranced!", out_size);
    }
    else if (p_ptr->smithing)
    {
        my_strcpy(out, "Smithing", out_size);
    }
    else if (p_ptr->fletching)
    {
        my_strcpy(out, "Fletching", out_size);
    }
    else if (p_ptr->resting)
    {
        int n = p_ptr->resting;

        if (n == -1)
            my_strcpy(out, "Rest *", out_size);
        else if (n == -2)
            my_strcpy(out, "Rest &", out_size);
        else if (n > 0)
            strnfmt(out, out_size, "Rest %d", n);
        else
            my_strcpy(out, "Resting", out_size);
    }
    else if (p_ptr->command_rep)
    {
        strnfmt(out, out_size, "Repeat %d", p_ptr->command_rep);
    }
    else if (p_ptr->stealth_mode)
    {
        my_strcpy(out, "Stealth", out_size);
    }
    else
    {
        my_strcpy(out, "Normal", out_size);
    }
}

static cptr web_get_speed_text(void)
{
    if (!p_ptr)
        return "Normal";

    if (p_ptr->pspeed > 2)
        return "Fast";
    if (p_ptr->pspeed < 2)
        return "Slow";
    return "Normal";
}

static cptr web_get_hunger_text(void)
{
    if (!p_ptr)
        return "Normal";

    if (p_ptr->food < PY_FOOD_STARVE)
        return "Starving";
    if (p_ptr->food < PY_FOOD_WEAK)
        return "Weak";
    if (p_ptr->food < PY_FOOD_ALERT)
        return "Hungry";
    if (p_ptr->food < PY_FOOD_FULL)
        return "Normal";
    return "Full";
}

static cptr web_get_terrain_text(void)
{
    if (!p_ptr || !in_bounds(p_ptr->py, p_ptr->px))
        return NULL;

    if (cave_pit_bold(p_ptr->py, p_ptr->px))
        return "Pit";
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
        return "Web";
    if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
        return "Sunlight";

    return NULL;
}

static void web_get_effects_text(char* out, size_t out_size)
{
    size_t off = 0;
    char tmp[32];

    if (!out || (out_size == 0))
        return;

    out[0] = '\0';
    if (!p_ptr)
        return;

    if (p_ptr->blind)
        web_append_list_item(out, out_size, &off, "Blind");
    if (p_ptr->confused)
        web_append_list_item(out, out_size, &off, "Confused");
    if (p_ptr->afraid)
        web_append_list_item(out, out_size, &off, "Afraid");

    if (p_ptr->poisoned > 0)
    {
        strnfmt(tmp, sizeof(tmp), "Poisoned %d", p_ptr->poisoned);
        web_append_list_item(out, out_size, &off, tmp);
    }

    if (p_ptr->cut >= 100)
    {
        web_append_list_item(out, out_size, &off, "Mortal wound");
    }
    else if (p_ptr->cut > 0)
    {
        strnfmt(tmp, sizeof(tmp), "Bleeding %d", p_ptr->cut);
        web_append_list_item(out, out_size, &off, tmp);
    }

    if (p_ptr->stun > 100)
    {
        web_append_list_item(out, out_size, &off, "Knocked out");
    }
    else if (p_ptr->stun > 50)
    {
        web_append_list_item(out, out_size, &off, "Heavy stun");
    }
    else if (p_ptr->stun > 0)
    {
        web_append_list_item(out, out_size, &off, "Stunned");
    }

    if (p_ptr->fast > 0)
    {
        strnfmt(tmp, sizeof(tmp), "Fast %d", p_ptr->fast);
        web_append_list_item(out, out_size, &off, tmp);
    }
    if (p_ptr->slow > 0)
    {
        strnfmt(tmp, sizeof(tmp), "Slow %d", p_ptr->slow);
        web_append_list_item(out, out_size, &off, tmp);
    }
}

static void web_json_append_escaped(
    char* buf, size_t buf_size, size_t* off, cptr text)
{
    const unsigned char* s = (const unsigned char*)(text ? text : "");

    if (!buf || !off || (buf_size == 0))
        return;

    while (*s)
    {
        unsigned char ch = *s++;

        if (ch == '"')
        {
            strnfcat(buf, buf_size, off, "\\\"");
        }
        else if (ch == '\\')
        {
            strnfcat(buf, buf_size, off, "\\\\");
        }
        else if (ch == '\n')
        {
            strnfcat(buf, buf_size, off, "\\n");
        }
        else if (ch == '\r')
        {
            strnfcat(buf, buf_size, off, "\\r");
        }
        else if (ch == '\t')
        {
            strnfcat(buf, buf_size, off, "\\t");
        }
        else if (ch < 32)
        {
            strnfcat(buf, buf_size, off, " ");
        }
        else
        {
            strnfcat(buf, buf_size, off, "%c", (char)ch);
        }
    }
}

static void web_json_append_field_sep(
    char* buf, size_t buf_size, size_t* off, bool* first)
{
    if (!buf || !off || !first || (buf_size == 0))
        return;

    if (!(*first))
        strnfcat(buf, buf_size, off, ",");

    *first = FALSE;
}

static void web_json_append_field_string(char* buf, size_t buf_size, size_t* off,
    bool* first, cptr key, cptr value)
{
    if (!key)
        return;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"%s\":\"", key);
    web_json_append_escaped(buf, buf_size, off, value);
    strnfcat(buf, buf_size, off, "\"");
}

static void web_json_append_field_int(char* buf, size_t buf_size, size_t* off,
    bool* first, cptr key, int value)
{
    if (!key)
        return;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"%s\":%d", key, value);
}

/* Appends one semantic character-sheet field object into a JSON array. */
static void web_json_append_character_field_object(char* buf, size_t buf_size,
    size_t* off, const ui_character_field* field)
{
    bool field_first = TRUE;

    strnfcat(buf, buf_size, off, "{");
    web_json_append_field_string(
        buf, buf_size, off, &field_first, "label", field->label);
    web_json_append_field_string(
        buf, buf_size, off, &field_first, "value", field->value);
    web_json_append_field_int(buf, buf_size, off, &field_first, "visible",
        field->visible ? 1 : 0);
    strnfcat(buf, buf_size, off, "}");
}

/* Appends one named array of semantic character-sheet fields. */
static void web_json_append_character_field_array(char* buf, size_t buf_size,
    size_t* off, bool* first, cptr key, const ui_character_field* fields,
    int count)
{
    int i;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"%s\":[", key);

    for (i = 0; i < count; i++)
    {
        if (i > 0)
            strnfcat(buf, buf_size, off, ",");

        web_json_append_character_field_object(buf, buf_size, off, &fields[i]);
    }

    strnfcat(buf, buf_size, off, "]");
}

/* Appends one semantic character stat object into a JSON array. */
static void web_json_append_character_stat_object(char* buf, size_t buf_size,
    size_t* off, const ui_character_stat* stat)
{
    bool stat_first = TRUE;

    strnfcat(buf, buf_size, off, "{");
    web_json_append_field_string(
        buf, buf_size, off, &stat_first, "label", stat->label);
    web_json_append_field_string(
        buf, buf_size, off, &stat_first, "current", stat->current);
    web_json_append_field_string(
        buf, buf_size, off, &stat_first, "base", stat->base);
    web_json_append_field_int(buf, buf_size, off, &stat_first, "reduced",
        stat->reduced ? 1 : 0);
    web_json_append_field_int(buf, buf_size, off, &stat_first, "showBase",
        stat->show_base ? 1 : 0);
    web_json_append_field_int(
        buf, buf_size, off, &stat_first, "equipMod", stat->equip_mod);
    web_json_append_field_int(
        buf, buf_size, off, &stat_first, "drainMod", stat->drain_mod);
    web_json_append_field_int(
        buf, buf_size, off, &stat_first, "miscMod", stat->misc_mod);
    strnfcat(buf, buf_size, off, "}");
}

/* Appends the semantic character stats array. */
static void web_json_append_character_stats_array(char* buf, size_t buf_size,
    size_t* off, bool* first, const ui_character_stat* stats, int count)
{
    int i;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"stats\":[");

    for (i = 0; i < count; i++)
    {
        if (i > 0)
            strnfcat(buf, buf_size, off, ",");

        web_json_append_character_stat_object(buf, buf_size, off, &stats[i]);
    }

    strnfcat(buf, buf_size, off, "]");
}

/* Appends one semantic character skill object into a JSON array. */
static void web_json_append_character_skill_object(char* buf, size_t buf_size,
    size_t* off, const ui_character_skill* skill)
{
    bool skill_first = TRUE;

    strnfcat(buf, buf_size, off, "{");
    web_json_append_field_string(
        buf, buf_size, off, &skill_first, "label", skill->label);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "value", skill->value);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "base", skill->base);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "statMod", skill->stat_mod);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "equipMod", skill->equip_mod);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "miscMod", skill->misc_mod);
    web_json_append_field_int(
        buf, buf_size, off, &skill_first, "cost", skill->cost);
    web_json_append_field_int(buf, buf_size, off, &skill_first, "editable",
        skill->editable ? 1 : 0);
    web_json_append_field_int(buf, buf_size, off, &skill_first, "selected",
        skill->selected ? 1 : 0);
    web_json_append_field_int(buf, buf_size, off, &skill_first, "canDecrease",
        skill->can_decrease ? 1 : 0);
    web_json_append_field_int(buf, buf_size, off, &skill_first, "canIncrease",
        skill->can_increase ? 1 : 0);
    strnfcat(buf, buf_size, off, "}");
}

/* Appends the semantic character skills array. */
static void web_json_append_character_skills_array(char* buf, size_t buf_size,
    size_t* off, bool* first, const ui_character_skill* skills, int count)
{
    int i;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"skills\":[");

    for (i = 0; i < count; i++)
    {
        if (i > 0)
            strnfcat(buf, buf_size, off, ",");

        web_json_append_character_skill_object(buf, buf_size, off, &skills[i]);
    }

    strnfcat(buf, buf_size, off, "]");
}

/* Appends the semantic character action list. */
static void web_json_append_character_actions_array(char* buf, size_t buf_size,
    size_t* off, bool* first, const ui_character_action* actions, int count)
{
    int i;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"actions\":[");

    for (i = 0; i < count; i++)
    {
        bool action_first = TRUE;

        if (i > 0)
            strnfcat(buf, buf_size, off, ",");

        strnfcat(buf, buf_size, off, "{");
        web_json_append_field_int(
            buf, buf_size, off, &action_first, "key", actions[i].key);
        web_json_append_field_string(
            buf, buf_size, off, &action_first, "label", actions[i].label);
        strnfcat(buf, buf_size, off, "}");
    }

    strnfcat(buf, buf_size, off, "]");
}

/* Returns the short frontend label for one exported tile-context action id. */
static cptr web_get_tile_context_action_label(int dir, int action_id)
{
    switch (action_id)
    {
    case GRID_CONTEXT_ACTION_DEFAULT:
        return (dir == 5) ? current_square_action_label() : adjacent_action_label(dir);

    case GRID_CONTEXT_ACTION_PICKUP:
        return "Pick up";

    case GRID_CONTEXT_ACTION_ASCEND:
        return "Ascend";

    case GRID_CONTEXT_ACTION_DESCEND:
        return "Descend";

    case GRID_CONTEXT_ACTION_USE_FORGE:
        return "Use forge";

    case GRID_CONTEXT_ACTION_OPEN:
        return "Open door";

    default:
        return NULL;
    }
}

/* Appends the exported tile-context action list into one JSON array. */
static void web_json_append_tile_context_actions_array(char* buf, size_t buf_size,
    size_t* off, bool* first, int dir, const int action_ids[], int count)
{
    int i;

    web_json_append_field_sep(buf, buf_size, off, first);
    strnfcat(buf, buf_size, off, "\"actions\":[");

    for (i = 0; i < count; i++)
    {
        bool action_first = TRUE;
        cptr label = web_get_tile_context_action_label(dir, action_ids[i]);

        if (i > 0)
            strnfcat(buf, buf_size, off, ",");

        strnfcat(buf, buf_size, off, "{");
        web_json_append_field_int(
            buf, buf_size, off, &action_first, "id", action_ids[i]);
        web_json_append_field_string(
            buf, buf_size, off, &action_first, "label", label ? label : "");
        strnfcat(buf, buf_size, off, "}");
    }

    strnfcat(buf, buf_size, off, "]");
}

/* Chooses the default quiver for one synthetic ranged action. */
static int web_choose_ranged_quiver(bool* ready)
{
    bool quiver1_loaded = inventory[INVEN_QUIVER1].k_idx ? TRUE : FALSE;
    bool quiver2_loaded = inventory[INVEN_QUIVER2].k_idx ? TRUE : FALSE;

    if (ready)
        *ready = quiver1_loaded || quiver2_loaded;

    if (quiver1_loaded)
        return 1;

    if (quiver2_loaded)
        return 2;

    return 1;
}

/* Builds semantic player status payload for frontend-side layout/rendering. */
static void web_build_player_state(void)
{
    size_t off = 0;
    bool first = TRUE;
    cptr player_name;
    cptr race_name;
    cptr house_name;
    cptr song1_name;
    cptr song2_name;
    cptr speed_text;
    cptr hunger_text;
    cptr terrain_text;
    char depth_buf[32];
    char str_buf[8];
    char dex_buf[8];
    char con_buf[8];
    char gra_buf[8];
    char state_buf[32];
    char effects_buf[256];
    char melee_buf[32];
    char melee2_buf[32];
    char arc_buf[32];
    char armor_buf[32];
    char ranged_action_buf[80];
    char target_name[80];
    char target_hp_bar[9];
    char target_alert[20];
    char adjacent_action_key[32];
    cptr current_square_label = NULL;
    cptr adjacent_action_label_by_dir[10];
    bool dual_wield;
    bool has_bow;
    bool ranged_action_ready = FALSE;
    bool rapid_attack;
    bool blocking;
    byte current_square_attr = TERM_WHITE;
    byte current_square_char = (byte)' ';
    byte ranged_action_attr = TERM_WHITE;
    byte ranged_action_char = (byte)' ';
    byte adjacent_action_attr_by_dir[10];
    byte adjacent_action_char_by_dir[10];
    int armor_min;
    int armor_max;
    int arc_dd;
    int dir;
    int current_square_visual_kind = UI_MENU_VISUAL_NONE;
    int ranged_action_visual_kind = UI_MENU_VISUAL_NONE;
    int ranged_action_quiver = 0;
    int adjacent_action_visual_kind_by_dir[10];
    int depth_feet;

    web_player_state[0] = '\0';

    if (!p_ptr || !op_ptr)
    {
        my_strcpy(web_player_state, "{\"ready\":0}",
            sizeof(web_player_state));
        return;
    }

    player_name = op_ptr->full_name[0] ? op_ptr->full_name : "(unnamed)";
    race_name = web_get_race_name();
    house_name = web_get_house_name();
    song1_name = web_get_song_name(p_ptr->song1);
    song2_name = web_get_song_name(p_ptr->song2);

    if (!race_name || !race_name[0])
        race_name = "Unknown";
    if (!house_name || !house_name[0])
        house_name = "Unknown";

    for (dir = 0; dir < 10; dir++)
    {
        adjacent_action_label_by_dir[dir] = NULL;
        adjacent_action_attr_by_dir[dir] = TERM_WHITE;
        adjacent_action_char_by_dir[dir] = (byte)' ';
        adjacent_action_visual_kind_by_dir[dir] = UI_MENU_VISUAL_NONE;
    }

    depth_feet = p_ptr->depth * 50;
    if (!p_ptr->depth)
        my_strcpy(depth_buf, "Surface", sizeof(depth_buf));
    else
        strnfmt(depth_buf, sizeof(depth_buf), "%d ft", depth_feet);

    cnv_stat(p_ptr->stat_use[A_STR], str_buf, sizeof(str_buf));
    cnv_stat(p_ptr->stat_use[A_DEX], dex_buf, sizeof(dex_buf));
    cnv_stat(p_ptr->stat_use[A_CON], con_buf, sizeof(con_buf));
    cnv_stat(p_ptr->stat_use[A_GRA], gra_buf, sizeof(gra_buf));

    dual_wield = ((&inventory[INVEN_ARM])->k_idx)
        && ((&inventory[INVEN_ARM])->tval != TV_SHIELD);
    rapid_attack = p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] ? TRUE : FALSE;
    has_bow = ((&inventory[INVEN_BOW])->k_idx) ? TRUE : FALSE;

    strnfmt(melee_buf, sizeof(melee_buf), "(%+d,%dd%d)%s", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds, rapid_attack ? " 2x" : "");
    if (dual_wield)
    {
        strnfmt(melee2_buf, sizeof(melee2_buf), "(%+d,%dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
    }
    else
    {
        my_strcpy(melee2_buf, "(none)", sizeof(melee2_buf));
    }

    if (has_bow)
    {
        arc_dd = p_ptr->add;
        if (p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL]
            && p_ptr->killed_enemy_with_arrow)
        {
            arc_dd = 2 * arc_dd;
        }
        strnfmt(arc_buf, sizeof(arc_buf), "(%+d,%dd%d)", p_ptr->skill_use[S_ARC],
            arc_dd, p_ptr->ads);
    }
    else
    {
        my_strcpy(arc_buf, "(none)", sizeof(arc_buf));
    }

    ranged_action_buf[0] = '\0';
    if (has_bow)
    {
        object_type* bow_ptr = &inventory[INVEN_BOW];

        object_desc(ranged_action_buf, sizeof(ranged_action_buf), bow_ptr, FALSE, 0);
        ranged_action_attr = (byte)object_attr(bow_ptr);
        ranged_action_char = (byte)object_char(bow_ptr);
        ranged_action_visual_kind
            = graphics_are_ascii() ? UI_MENU_VISUAL_TEXT : UI_MENU_VISUAL_TILE;
        ranged_action_quiver = web_choose_ranged_quiver(&ranged_action_ready);
    }

    blocking = p_ptr->active_ability[S_EVN][EVN_BLOCKING];
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = FALSE;
    armor_min = p_min(GF_HURT, TRUE);
    armor_max = p_max(GF_HURT, TRUE);
    p_ptr->active_ability[S_EVN][EVN_BLOCKING] = blocking;
    strnfmt(armor_buf, sizeof(armor_buf), "[%+d,%d-%d]", p_ptr->skill_use[S_EVN],
        armor_min, armor_max);

    web_get_state_text(state_buf, sizeof(state_buf));
    web_get_effects_text(effects_buf, sizeof(effects_buf));
    speed_text = web_get_speed_text();
    hunger_text = web_get_hunger_text();
    terrain_text = web_get_terrain_text();
    current_square_label = current_square_action_label();
    current_square_action_visual(&current_square_attr, &current_square_char);

    if (current_square_label && current_square_action_available())
    {
        current_square_visual_kind
            = graphics_are_ascii() ? UI_MENU_VISUAL_TEXT : UI_MENU_VISUAL_TILE;
    }

    for (dir = 1; dir <= 9; dir++)
    {
        if (dir == 5)
            continue;

        adjacent_action_label_by_dir[dir] = adjacent_action_label(dir);
        adjacent_action_visual(dir, &adjacent_action_attr_by_dir[dir],
            &adjacent_action_char_by_dir[dir]);

        if (adjacent_action_label_by_dir[dir] && adjacent_action_available(dir))
        {
            adjacent_action_visual_kind_by_dir[dir]
                = graphics_are_ascii() ? UI_MENU_VISUAL_TEXT
                                       : UI_MENU_VISUAL_TILE;
        }
    }

    web_get_tracked_monster_state(target_name, sizeof(target_name), target_hp_bar,
        sizeof(target_hp_bar), target_alert, sizeof(target_alert));

    strnfcat(web_player_state, sizeof(web_player_state), &off, "{");
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "ready", 1);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "name", player_name);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "race", race_name);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "house", house_name);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "depthFeet", depth_feet);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "depthText", depth_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "str",
        str_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "dex",
        dex_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "con",
        con_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "gra",
        gra_buf);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "healthCur", p_ptr->chp);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "healthMax", p_ptr->mhp);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "voiceCur", p_ptr->csp);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "voiceMax", p_ptr->msp);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "melee", melee_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "melee2", melee2_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "ranged", arc_buf);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionVisible", has_bow ? 1 : 0);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionLabel", ranged_action_buf);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionReady", ranged_action_ready ? 1 : 0);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionQuiver", ranged_action_quiver);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionVisualKind", ranged_action_visual_kind);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionVisualAttr", ranged_action_attr);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "rangedActionVisualChar", ranged_action_char);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "armor", armor_buf);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "song",
        song1_name ? song1_name : "(none)");
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first, "theme",
        song2_name ? song2_name : "(none)");
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "state", state_buf);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "stealthActive", p_ptr->stealth_mode ? 1 : 0);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "speed", speed_text);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "hunger", hunger_text);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "terrain", terrain_text ? terrain_text : "(none)");
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "effects", effects_buf[0] ? effects_buf : "(none)");
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "tileActionVisible", current_square_label ? 1 : 0);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "tileActionLabel", current_square_label ? current_square_label : "");
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "tileActionVisualKind", current_square_visual_kind);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "tileActionVisualAttr", current_square_attr);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "tileActionVisualChar", current_square_char);
    for (dir = 1; dir <= 9; dir++)
    {
        if (dir == 5)
            continue;

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dVisible", dir);
        web_json_append_field_int(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key, adjacent_action_label_by_dir[dir] ? 1 : 0);

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dLabel", dir);
        web_json_append_field_string(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key,
            adjacent_action_label_by_dir[dir] ? adjacent_action_label_by_dir[dir]
                                              : "");

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dVisualKind", dir);
        web_json_append_field_int(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key, adjacent_action_visual_kind_by_dir[dir]);

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dVisualAttr", dir);
        web_json_append_field_int(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key, adjacent_action_attr_by_dir[dir]);

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dVisualChar", dir);
        web_json_append_field_int(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key, adjacent_action_char_by_dir[dir]);

        strnfmt(adjacent_action_key, sizeof(adjacent_action_key),
            "adjAction%dAttack", dir);
        web_json_append_field_int(
            web_player_state, sizeof(web_player_state), &off, &first,
            adjacent_action_key, adjacent_action_is_attack(dir) ? 1 : 0);
    }
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "targetName", target_name);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "targetHpBar", target_hp_bar);
    web_json_append_field_string(
        web_player_state, sizeof(web_player_state), &off, &first,
        "targetAlert", target_alert);
    web_json_append_field_int(
        web_player_state, sizeof(web_player_state), &off, &first,
        "targetVisible", target_hp_bar[0] ? 1 : 0);
    strnfcat(web_player_state, sizeof(web_player_state), &off, "}");
}

/* Builds semantic birth-screen payload for frontend-side rendering. */
static void web_build_birth_state(void)
{
    struct ui_birth_state_snapshot snapshot;
    size_t off = 0;
    bool first = TRUE;
    int i;

    web_birth_state[0] = '\0';
    ui_birth_get_state_snapshot(&snapshot);

    if (!snapshot.active)
    {
        my_strcpy(web_birth_state, "{\"active\":0}", sizeof(web_birth_state));
        return;
    }

    strnfcat(web_birth_state, sizeof(web_birth_state), &off, "{");
    web_json_append_field_int(
        web_birth_state, sizeof(web_birth_state), &off, &first, "active", 1);
    web_json_append_field_string(web_birth_state, sizeof(web_birth_state), &off,
        &first, "kind",
        (snapshot.kind == UI_BIRTH_SCREEN_NAME)
            ? "name"
            : (snapshot.kind == UI_BIRTH_SCREEN_AHW)
                ? "ahw"
                : (snapshot.kind == UI_BIRTH_SCREEN_HISTORY)
                    ? "history"
                    : "stats");
    web_json_append_field_string(web_birth_state, sizeof(web_birth_state), &off,
        &first, "title", snapshot.title ? snapshot.title : "");
    web_json_append_field_string(web_birth_state, sizeof(web_birth_state), &off,
        &first, "help", snapshot.help ? snapshot.help : "");

    if (snapshot.kind == UI_BIRTH_SCREEN_NAME)
    {
        web_json_append_field_string(web_birth_state, sizeof(web_birth_state),
            &off, &first, "text", snapshot.text ? snapshot.text : "");
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "maxLength", snapshot.max_length);
        strnfcat(web_birth_state, sizeof(web_birth_state), &off, "}");
        return;
    }

    if (snapshot.kind == UI_BIRTH_SCREEN_AHW)
    {
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "age", snapshot.age);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "height", snapshot.height);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "weight", snapshot.weight);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "ageMin", snapshot.age_min);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "ageMax", snapshot.age_max);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "heightMin", snapshot.height_min);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "heightMax", snapshot.height_max);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "weightMin", snapshot.weight_min);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "weightMax", snapshot.weight_max);
        strnfcat(web_birth_state, sizeof(web_birth_state), &off, "}");
        return;
    }

    if (snapshot.kind == UI_BIRTH_SCREEN_HISTORY)
    {
        web_json_append_field_string(web_birth_state, sizeof(web_birth_state),
            &off, &first, "text", snapshot.text ? snapshot.text : "");
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &first, "maxLength", snapshot.max_length);
        strnfcat(web_birth_state, sizeof(web_birth_state), &off, "}");
        return;
    }

    web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
        &first, "pointsLeft", snapshot.points_left);
    web_json_append_field_sep(
        web_birth_state, sizeof(web_birth_state), &off, &first);
    strnfcat(web_birth_state, sizeof(web_birth_state), &off, "\"stats\":[");

    for (i = 0; i < snapshot.stats_count; i++)
    {
        bool stat_first = TRUE;
        const struct ui_birth_stat_snapshot* stat = &snapshot.stats[i];

        if (i > 0)
            strnfcat(web_birth_state, sizeof(web_birth_state), &off, ",");

        strnfcat(web_birth_state, sizeof(web_birth_state), &off, "{");
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &stat_first, "index", i);
        web_json_append_field_string(web_birth_state, sizeof(web_birth_state),
            &off, &stat_first, "name", stat->name ? stat->name : "");
        web_json_append_field_string(web_birth_state, sizeof(web_birth_state),
            &off, &stat_first, "value", stat->value ? stat->value : "");
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &stat_first, "cost", stat->cost);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &stat_first, "selected", stat->selected ? 1 : 0);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &stat_first, "canDecrease", stat->can_decrease ? 1 : 0);
        web_json_append_field_int(web_birth_state, sizeof(web_birth_state), &off,
            &stat_first, "canIncrease", stat->can_increase ? 1 : 0);
        strnfcat(web_birth_state, sizeof(web_birth_state), &off, "}");
    }

    strnfcat(web_birth_state, sizeof(web_birth_state), &off, "]}");
}

/* Builds semantic character-sheet payload for frontend-side rendering. */
static void web_build_character_sheet_state(void)
{
    const ui_character_sheet_state* sheet = ui_character_get_sheet();
    size_t off = 0;
    bool first = TRUE;

    web_character_sheet_state[0] = '\0';

    if (!sheet)
    {
        my_strcpy(web_character_sheet_state, "{\"active\":0}",
            sizeof(web_character_sheet_state));
        return;
    }

    strnfcat(web_character_sheet_state, sizeof(web_character_sheet_state), &off,
        "{");
    web_json_append_field_int(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "active", 1);
    web_json_append_field_string(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "title", sheet->title);
    web_json_append_field_int(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "skillEditorActive",
        sheet->skill_editor_active ? 1 : 0);
    web_json_append_field_string(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "skillEditorTitle",
        sheet->skill_editor_title);
    web_json_append_field_string(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "skillEditorHelp",
        sheet->skill_editor_help);
    web_json_append_field_int(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "skillPointsLeft",
        sheet->skill_points_left);
    web_json_append_character_field_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "identity",
        sheet->identity, (int)N_ELEMENTS(sheet->identity));
    web_json_append_character_field_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "physical",
        sheet->physical, (int)N_ELEMENTS(sheet->physical));
    web_json_append_character_field_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "progress",
        sheet->progress, UI_CHARACTER_FIELD_COUNT);
    web_json_append_character_stats_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, sheet->stats, A_MAX);
    web_json_append_character_field_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "combat",
        sheet->combat, UI_CHARACTER_COMBAT_COUNT);
    web_json_append_character_skills_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, sheet->skills, S_MAX);
    web_json_append_field_string(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, "history", sheet->history);
    web_json_append_character_actions_array(web_character_sheet_state,
        sizeof(web_character_sheet_state), &off, &first, sheet->actions,
        sheet->action_count);
    strnfcat(web_character_sheet_state, sizeof(web_character_sheet_state), &off,
        "}");
}

static void web_build_log_text(void)
{
    size_t off = 0;
    int count;
    int shown;
    int age;
    int i;
    byte attr;
    cptr msg;
    cptr empty_msg = "(no messages)";

    web_log_text[0] = '\0';
    web_log_attrs_len = 0;

    count = (int)message_num();
    if (count <= 0)
    {
        for (i = 0; empty_msg[i] != '\0'; i++)
        {
            if (off + 1 >= sizeof(web_log_text))
                break;
            web_log_text[off] = empty_msg[i];
            web_log_attrs[off] = TERM_WHITE;
            off++;
        }
        web_log_text[off] = '\0';
        web_log_attrs_len = (int)off;
        return;
    }

    shown = count;
    if (shown > WEB_LOG_MESSAGES_MAX)
        shown = WEB_LOG_MESSAGES_MAX;

    for (age = shown - 1; age >= 0; age--)
    {
        msg = message_str((s16b)age);
        attr = message_color((s16b)age);
        if (!msg || !msg[0])
            continue;

        for (i = 0; msg[i] != '\0'; i++)
        {
            if (off + 1 >= sizeof(web_log_text))
                goto done;

            web_log_text[off] = msg[i];
            web_log_attrs[off] = attr;
            off++;
        }

        if (off + 1 >= sizeof(web_log_text))
            goto done;

        web_log_text[off] = '\n';
        web_log_attrs[off] = attr;
        off++;
    }

done:
    if ((off > 0) && (web_log_text[off - 1] == '\n'))
        off--;
    web_log_text[off] = '\0';
    web_log_attrs_len = (int)off;
}

static void web_refresh_log_text(void)
{
    if (web_log_text_frame == data.frame_id)
        return;

    web_build_log_text();
    web_log_text_frame = data.frame_id;
}

static void web_refresh_player_state(void)
{
    web_build_player_state();
}

/* Resolves one player-relative direction into absolute grid coordinates. */
static bool web_get_tile_context_target(int dir, int* y, int* x)
{
    int ty;
    int tx;

    if (!p_ptr || !character_dungeon)
        return FALSE;

    if ((dir < 1) || (dir > 9))
        return FALSE;

    ty = p_ptr->py + ddy[dir];
    tx = p_ptr->px + ddx[dir];

    if (!in_bounds(ty, tx))
        return FALSE;

    if (y)
        *y = ty;
    if (x)
        *x = tx;

    return TRUE;
}

/* Builds the semantic tile-context popup payload for one current/adjacent grid. */
static void web_build_tile_context_state(int dir)
{
    char description[2048];
    int action_ids[8];
    int action_count = 0;
    int y;
    int x;
    size_t off = 0;
    bool first = TRUE;

    web_tile_context_state[0] = '\0';

    if (!web_get_tile_context_target(dir, &y, &x))
    {
        my_strcpy(web_tile_context_state, "{\"valid\":0}",
            sizeof(web_tile_context_state));
        return;
    }

    if (dir == 5)
        action_count = current_square_collect_context_actions(
            action_ids, N_ELEMENTS(action_ids));
    else
        action_count = adjacent_collect_context_actions(
            dir, action_ids, N_ELEMENTS(action_ids));

    if (action_count <= 0)
    {
        my_strcpy(web_tile_context_state, "{\"valid\":0}",
            sizeof(web_tile_context_state));
        return;
    }

    describe_grid_for_look(description, sizeof(description), y, x);

    strnfcat(web_tile_context_state, sizeof(web_tile_context_state), &off, "{");
    web_json_append_field_int(
        web_tile_context_state, sizeof(web_tile_context_state), &off, &first,
        "valid", 1);
    web_json_append_field_int(
        web_tile_context_state, sizeof(web_tile_context_state), &off, &first,
        "dir", dir);
    web_json_append_field_string(
        web_tile_context_state, sizeof(web_tile_context_state), &off, &first,
        "description", description);
    web_json_append_tile_context_actions_array(
        web_tile_context_state, sizeof(web_tile_context_state), &off, &first,
        dir, action_ids, action_count);
    strnfcat(web_tile_context_state, sizeof(web_tile_context_state), &off, "}");
}

/* Returns whether one action id is currently valid for one tile-context target. */
static bool web_tile_context_has_action(int dir, int action_id)
{
    int action_ids[8];
    int action_count;
    int i;

    if (dir == 5)
        action_count = current_square_collect_context_actions(
            action_ids, N_ELEMENTS(action_ids));
    else
        action_count = adjacent_collect_context_actions(
            dir, action_ids, N_ELEMENTS(action_ids));

    for (i = 0; i < action_count; i++)
    {
        if (action_ids[i] == action_id)
            return TRUE;
    }

    return FALSE;
}

static void web_refresh_birth_state(void)
{
    unsigned int revision = ui_birth_revision();

    if (web_birth_state_revision == revision)
        return;

    web_build_birth_state();
    web_birth_state_revision = revision;
}

static void web_refresh_character_sheet_state(void)
{
    unsigned int revision = ui_character_sheet_revision();

    if (web_character_sheet_state_revision == revision)
        return;

    web_build_character_sheet_state();
    web_character_sheet_state_revision = revision;
}

/* ------------------------------------------------------------------------ */
/* Overlay Capture And Semantic Extraction                                  */
/* ------------------------------------------------------------------------ */

static void web_clear_overlay_text(void)
{
    web_overlay_mode = 0;
    web_overlay_text[0] = '\0';
    web_overlay_attrs_len = 0;
}

static void web_sync_saved_screen_lifecycle(void)
{
    unsigned int revision = ui_saved_screen_get_revision();

    if (revision == web_saved_screen_revision)
        return;

    web_saved_screen_revision = revision;
    web_overlay_capture_active = FALSE;
    web_overlay_capture_clear();
    web_clear_overlay_text();
    web_overlay_text_frame = UINT32_MAX;
}

static bool web_overlay_semantic_active(void)
{
    if (ui_saved_screen_was_restored())
        return FALSE;

    if (character_icky > 0)
        return TRUE;

    if (!p_ptr)
        return TRUE;

    if (!p_ptr->playing)
        return TRUE;

    return FALSE;
}

static void web_overlay_capture_clear(void)
{
    size_t count;

    if (!web_overlay_capture || !web_overlay_capture_attr || (data.cols <= 0)
        || (data.rows <= 0))
        return;

    count = (size_t)data.cols * (size_t)data.rows;
    memset(web_overlay_capture, (int)' ', count);
    memset(web_overlay_capture_attr, TERM_WHITE, count);
}

static void web_overlay_capture_sync_state(void)
{
    bool active = web_overlay_semantic_active();

    web_sync_saved_screen_lifecycle();

    if (active == web_overlay_capture_active)
        return;

    web_overlay_capture_clear();
    web_overlay_capture_active = active;
}

static void web_overlay_capture_wipe(int x, int y, int n)
{
    int i;

    web_overlay_capture_sync_state();

    if (!web_overlay_capture_active || !web_overlay_capture
        || !web_overlay_capture_attr)
        return;

    if (!web_clip_row_span(data.cols, data.rows, y, &x, &n, NULL))
        return;

    for (i = 0; i < n; i++)
    {
        web_overlay_capture[y * data.cols + x + i] = (byte)' ';
        web_overlay_capture_attr[y * data.cols + x + i] = TERM_WHITE;
    }
}

static void web_overlay_capture_text(int x, int y, int n, cptr s, byte attr)
{
    int i;
    int src_skip = 0;

    web_overlay_capture_sync_state();

    if (!web_overlay_capture_active || !web_overlay_capture
        || !web_overlay_capture_attr || !s)
        return;

    if (!web_clip_row_span(data.cols, data.rows, y, &x, &n, &src_skip))
        return;

    s += src_skip;

    for (i = 0; i < n; i++)
    {
        byte ch = (byte)s[i];
        if ((ch < 32) || (ch > 126))
            ch = (byte)' ';

        web_overlay_capture[y * data.cols + x + i] = ch;
        web_overlay_capture_attr[y * data.cols + x + i] = attr;
    }
}

static void web_build_overlay_text(void)
{
    bool active;
    int x;
    int y;
    int min_x = data.cols;
    int min_y = data.rows;
    int max_x = -1;
    int max_y = -1;
    size_t off = 0;

    web_clear_overlay_text();

    if (!data.cells || (data.cols <= 0) || (data.rows <= 0))
        return;

    web_overlay_capture_sync_state();
    active = web_overlay_capture_active;

    if (!active || !web_overlay_capture || !web_overlay_capture_attr)
        return;

    web_overlay_mode = 1;
    for (y = 0; y < data.rows; y++)
    {
        for (x = 0; x < data.cols; x++)
        {
            byte ch = web_overlay_capture[y * data.cols + x];
            if (ch == (byte)' ')
                continue;
            if (x < min_x)
                min_x = x;
            if (y < min_y)
                min_y = y;
            if (x > max_x)
                max_x = x;
            if (y > max_y)
                max_y = y;
        }
    }

    if ((max_x < min_x) || (max_y < min_y))
        return;

    for (y = min_y; y <= max_y; y++)
    {
        for (x = min_x; x <= max_x; x++)
        {
            char ch = (char)web_overlay_capture[y * data.cols + x];
            byte attr = web_overlay_capture_attr[y * data.cols + x];

            if ((ch < 32) || (ch > 126))
                ch = ' ';

            if (off + 1 >= sizeof(web_overlay_text))
                goto overlay_done;

            web_overlay_text[off] = ch;
            web_overlay_attrs[off] = attr;
            off++;
        }

        if (y != max_y)
        {
            if (off + 1 >= sizeof(web_overlay_text))
                goto overlay_done;
            web_overlay_text[off] = '\n';
            web_overlay_attrs[off] = TERM_WHITE;
            off++;
        }
    }

overlay_done:
    web_overlay_text[off] = '\0';
    web_overlay_attrs_len = (int)off;
}

static void web_refresh_overlay_text(void)
{
    web_sync_saved_screen_lifecycle();

    if (web_overlay_text_frame == data.frame_id)
        return;

    web_build_overlay_text();
    web_overlay_text_frame = data.frame_id;
}

/* ------------------------------------------------------------------------ */
/* Map Metadata Snapshot                                                    */
/* ------------------------------------------------------------------------ */

static void web_ensure_map_cells(int count)
{
    if (count < 0)
        count = 0;

    if (count == web_map_cell_count)
        return;

    FREE(web_map_cells);
    web_map_cells = NULL;
    web_map_cell_count = 0;

    if (count <= 0)
        return;

    C_MAKE(web_map_cells, (size_t)count, web_cell);
    web_map_cell_count = count;
}

static void web_clear_map_cells(int count)
{
    int i;

    if (!web_map_cells || (count <= 0))
        return;

    for (i = 0; i < count; i++)
    {
        web_set_text_cell(&web_map_cells[i], TERM_WHITE, (byte)' ');
    }
}

static void web_refresh_map_cells(void)
{
    int cols;
    int rows;
    int count;
    int my;
    int mx;
    int saved_graphics;

    if (web_map_cells_frame == data.frame_id)
        return;

    cols = web_get_map_cols();
    rows = web_get_map_rows();
    count = cols * rows;

    web_ensure_map_cells(count);
    web_clear_map_cells(count);

    if (!web_map_cells || !p_ptr || (cols <= 0) || (rows <= 0))
    {
        web_map_cells_frame = data.frame_id;
        return;
    }

    saved_graphics = use_graphics;

    /* First pass: force graphics metadata from core map logic. */
    use_graphics = GRAPHICS_MICROCHASM;
    for (my = 0; my < rows; my++)
    {
        for (mx = 0; mx < cols; mx++)
        {
            int y = p_ptr->wy + my;
            int x = p_ptr->wx + mx;
            int idx = my * cols + mx;
            byte a;
            char c;
            byte ta;
            char tc;
            byte flags;

            if (!in_bounds(y, x))
                continue;

            map_info(y, x, &a, &c, &ta, &tc);

            flags = web_pict_flags(a, (byte)c, ta, (byte)tc);

            web_set_pict_cell(&web_map_cells[idx], a, (byte)c, ta, (byte)tc, flags);
        }
    }

    /* Second pass: capture ASCII fallback metadata from the same logic. */
    use_graphics = GRAPHICS_NONE;
    for (my = 0; my < rows; my++)
    {
        for (mx = 0; mx < cols; mx++)
        {
            int y = p_ptr->wy + my;
            int x = p_ptr->wx + mx;
            int idx = my * cols + mx;
            byte a;
            char c;
            byte ta;
            char tc;
            byte mark_attr = 0;
            byte mark_chr = 0;

            if (!in_bounds(y, x))
                continue;

            map_info(y, x, &a, &c, &ta, &tc);
            web_map_cells[idx].text_attr = a;
            web_map_cells[idx].text_char = (byte)c;

            if (!(web_map_cells[idx].flags & (WEB_FLAG_FG_PICT | WEB_FLAG_BG_PICT)))
            {
                web_map_cells[idx].kind = WEB_CELL_TEXT;
            }
            else
            {
                web_map_cells[idx].kind = WEB_CELL_PICT;
            }

            if (web_get_target_mark(y, x, &mark_attr, &mark_chr))
            {
                web_map_cells[idx].flags |= WEB_FLAG_MARK;
                web_map_cells[idx].text_attr = mark_attr;
                web_map_cells[idx].text_char = mark_chr;
            }
        }
    }

    use_graphics = saved_graphics;
    web_map_cells_frame = data.frame_id;
}

/* ------------------------------------------------------------------------ */
/* Term Hooks                                                               */
/* ------------------------------------------------------------------------ */

const char help_web[] =
    "Web/Emscripten metadata frontend, subopts -s<cols>x<rows>";

static void Term_init_web(term* t)
{
    (void)t;

    use_sound = arg_sound;
}

static void Term_nuke_web(term* t)
{
    term_data* td = (term_data*)(t->data);

    if (!td)
        return;

    FREE(td->cells);
    td->cells = NULL;

    FREE(web_overlay_capture);
    web_overlay_capture = NULL;
    FREE(web_overlay_capture_attr);
    web_overlay_capture_attr = NULL;
    web_overlay_capture_active = FALSE;

    FREE(web_map_cells);
    web_map_cells = NULL;
    web_map_cell_count = 0;
    web_map_cells_frame = UINT32_MAX;
    ui_menu_clear();
    ui_marks_clear();
    web_map_marks_revision = ui_marks_get_revision();

    FREE(web_fx_cells);
    web_fx_cells = NULL;
    web_fx_cell_count = 0;
    web_fx_cols = 0;
    web_fx_rows = 0;
    web_clear_sound_events();
}

/* Process pending input from JS and feed angband's key queue. */
static errr Term_xtra_web_event(int v)
{
    int key = 0;

    if (!v)
    {
        if (!web_key_dequeue(&key))
            return (1);

        Term_keypress(key);
        return (0);
    }

    while (!web_key_dequeue(&key))
    {
        web_sleep_msec(1);
    }

    Term_keypress(key);

    return (0);
}

static errr Term_xtra_web(int n, int v)
{
    term_data* td = (term_data*)(Term->data);

    switch (n)
    {
    case TERM_XTRA_EVENT:
    {
        return Term_xtra_web_event(v);
    }

    case TERM_XTRA_FLUSH:
    {
        while (Term_xtra_web_event(FALSE) == 0)
        {
        }
        return (0);
    }

    case TERM_XTRA_CLEAR:
    {
        if (!td)
            return (1);

        web_clear_cells(td, Term->attr_blank, (byte)Term->char_blank);
        ui_menu_clear();
        ui_marks_clear();
        web_map_marks_revision = ui_marks_get_revision();
        web_overlay_capture_clear();
        web_mark_dirty(td, 0, 0, td->cols, td->rows);
        return (0);
    }

    case TERM_XTRA_SHAPE:
    {
        if (!td)
            return (1);

        web_mark_cursor_dirty(td);
        td->cursor_visible = (v != 0);
        web_mark_cursor_dirty(td);
        return (0);
    }

    case TERM_XTRA_DELAY:
    {
        if (v > 0)
        {
            web_fx_delay_msec = v;
            web_fx_delay_seq++;
            web_capture_fx_cells();
        }
        web_sleep_msec(v);
        return (0);
    }

    case TERM_XTRA_REACT:
    {
        /* Web frontend always exports/render tiles metadata. */
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        ANGBAND_GRAF = "tiles";
        return (0);
    }

    case TERM_XTRA_FROSH:
    case TERM_XTRA_FRESH:
    case TERM_XTRA_ALIVE:
    case TERM_XTRA_LEVEL:
    case TERM_XTRA_BORED:
    {
        return (0);
    }

    case TERM_XTRA_NOISE:
    {
        web_queue_noise_event();
        return (0);
    }

    case TERM_XTRA_SOUND:
    {
        web_queue_sound_event(v);
        return (0);
    }

    default:
    {
        return (1);
    }
    }
}

static errr Term_curs_web(int x, int y)
{
    term_data* td = (term_data*)(Term->data);

    if (!td)
        return (1);

    web_mark_cursor_dirty(td);

    td->cursor_x = x;
    td->cursor_y = y;
    td->cursor_visible = TRUE;

    web_mark_cursor_dirty(td);

    return (0);
}

static errr Term_wipe_web(int x, int y, int n)
{
    int i;
    term_data* td = (term_data*)(Term->data);

    if (!td || !td->cells)
        return (1);

    if (!web_clip_row_span(td->cols, td->rows, y, &x, &n, NULL))
        return (0);

    for (i = 0; i < n; i++)
    {
        web_set_text_cell(&td->cells[y * td->cols + x + i], Term->attr_blank,
            (byte)Term->char_blank);
    }

    web_overlay_capture_wipe(x, y, n);
    web_mark_dirty(td, x, y, n, 1);

    return (0);
}

static errr Term_text_web(int x, int y, int n, byte a, cptr s)
{
    int i;
    int src_skip = 0;
    term_data* td = (term_data*)(Term->data);

    if (!td || !td->cells || !s)
        return (1);

    if (!web_clip_row_span(td->cols, td->rows, y, &x, &n, &src_skip))
        return (0);

    s += src_skip;

    for (i = 0; i < n; i++)
    {
        web_set_text_cell(&td->cells[y * td->cols + x + i], a, (byte)s[i]);
    }

    web_overlay_capture_text(x, y, n, s, a);
    web_mark_dirty(td, x, y, n, 1);

    return (0);
}

static errr Term_pict_web(int x, int y, int n, const byte* ap, const char* cp,
    const byte* tap, const char* tcp)
{
    int i;
    int src_skip = 0;
    term_data* td = (term_data*)(Term->data);

    if (!td || !td->cells || !ap || !cp || !tap || !tcp)
        return (1);

    if (!web_clip_row_span(td->cols, td->rows, y, &x, &n, &src_skip))
        return (0);

    ap += src_skip;
    cp += src_skip;
    tap += src_skip;
    tcp += src_skip;

    for (i = 0; i < n; i++)
    {
        byte a = ap[i];
        byte c = (byte)cp[i];
        byte ta = tap[i];
        byte tc = (byte)tcp[i];
        byte flags = web_pict_flags(a, c, ta, tc);

        web_set_pict_cell(&td->cells[y * td->cols + x + i], a, c, ta, tc, flags);
    }

    web_overlay_capture_wipe(x, y, n);
    web_mark_dirty(td, x, y, n, 1);

    return (0);
}

static errr term_data_init_web(term_data* td, int rows, int cols)
{
    term* t = &td->t;

    WIPE(td, term_data);

    td->cols = cols;
    td->rows = rows;
    td->cursor_x = -1;
    td->cursor_y = -1;

    C_MAKE(td->cells, (size_t)(rows * cols), web_cell);
    C_MAKE(web_overlay_capture, (size_t)(rows * cols), byte);
    C_MAKE(web_overlay_capture_attr, (size_t)(rows * cols), byte);

    term_init(t, cols, rows, 1024);

    t->icky_corner = TRUE;
    t->higher_pict = TRUE;
    t->attr_blank = TERM_WHITE;
    t->char_blank = ' ';

    t->init_hook = Term_init_web;
    t->nuke_hook = Term_nuke_web;

    t->text_hook = Term_text_web;
    t->wipe_hook = Term_wipe_web;
    t->curs_hook = Term_curs_web;
    t->pict_hook = Term_pict_web;
    t->xtra_hook = Term_xtra_web;

    t->data = td;

    web_clear_cells(td, t->attr_blank, (byte)t->char_blank);
    web_overlay_capture_clear();
    ui_menu_clear();
    web_mark_dirty(td, 0, 0, cols, rows);
    Term_activate(t);

    return (0);
}

/* ------------------------------------------------------------------------ */
/* Frontend Init                                                            */
/* ------------------------------------------------------------------------ */

static int web_default_cols = 110;
static int web_default_rows = 36;

errr init_web(int argc, char** argv)
{
    int i;

    for (i = 1; i < argc; i++)
    {
        int cols = 0;
        int rows = 0;

        if (sscanf(argv[i], "-s%dx%d", &cols, &rows) == 2)
        {
            if ((cols >= 80) && (rows >= 24))
            {
                web_default_cols = cols;
                web_default_rows = rows;
            }
            continue;
        }

        plog_fmt("Ignoring option: %s", argv[i]);
    }

    /* Web frontend always exports/render tiles metadata. */
    arg_graphics = GRAPHICS_MICROCHASM;
    use_graphics = GRAPHICS_MICROCHASM;
    ANGBAND_GRAF = "tiles";

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
        angband_term[i] = NULL;

    if (term_data_init_web(&data, web_default_rows, web_default_cols))
        return (-1);

    angband_term[0] = &data.t;

    Term_activate(&data.t);
    term_screen = &data.t;

    web_log_text_frame = UINT32_MAX;
    web_birth_state_revision = UINT32_MAX;
    web_character_sheet_state_revision = UINT32_MAX;
    web_overlay_text_frame = UINT32_MAX;
    web_log_text[0] = '\0';
    web_birth_state[0] = '\0';
    web_character_sheet_state[0] = '\0';
    web_player_state[0] = '\0';
    web_tile_context_state[0] = '\0';
    web_log_attrs_len = 0;
    web_overlay_text[0] = '\0';
    web_overlay_attrs_len = 0;
    web_overlay_capture_active = FALSE;
    web_overlay_capture_clear();
    ui_menu_clear();
    ui_marks_clear();
    web_clear_birth_submission();
    web_map_marks_revision = ui_marks_get_revision();
    web_fx_delay_seq = 0;
    web_fx_delay_msec = 0;
    web_fx_cols = 0;
    web_fx_rows = 0;
    web_map_cells_frame = UINT32_MAX;
    ui_birth_set_submit_hook((ui_birth_submit_hook)web_consume_birth_submission);

    return (0);
}

/* ------------------------------------------------------------------------ */
/* JavaScript <-> WASM Bridge                                               */
/* ------------------------------------------------------------------------ */

/*
 * JavaScript <-> WASM bridge.
 *
 * Cell format (8 bytes per cell):
 *   kind, text_attr, text_char, fg_attr, fg_char, bg_attr, bg_char, flags
 */
EMSCRIPTEN_KEEPALIVE int web_get_cols(void)
{
    return data.cols;
}

EMSCRIPTEN_KEEPALIVE int web_get_rows(void)
{
    return data.rows;
}

EMSCRIPTEN_KEEPALIVE int web_get_cell_stride(void)
{
    return (int)sizeof(web_cell);
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_cells_ptr(void)
{
    return (uintptr_t)data.cells;
}

EMSCRIPTEN_KEEPALIVE int web_get_tile_wid(void)
{
    return web_tile_wid;
}

EMSCRIPTEN_KEEPALIVE int web_get_tile_hgt(void)
{
    return web_tile_hgt;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_col(void)
{
    return COL_MAP;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_row(void)
{
    return ROW_MAP;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_cols(void)
{
    return (data.cols - COL_MAP - 1) / (use_bigtile ? 2 : 1);
}

EMSCRIPTEN_KEEPALIVE int web_get_map_rows(void)
{
    return data.rows - ROW_MAP - 1;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_x_step(void)
{
    return use_bigtile ? 2 : 1;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_world_x(void)
{
    if (!p_ptr)
        return 0;

    return p_ptr->wx;
}

EMSCRIPTEN_KEEPALIVE int web_get_map_world_y(void)
{
    if (!p_ptr)
        return 0;

    return p_ptr->wy;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_map_cells_ptr(void)
{
    web_sync_map_marks_revision();
    web_refresh_map_cells();
    return (uintptr_t)web_map_cells;
}

EMSCRIPTEN_KEEPALIVE int web_get_player_map_x(void)
{
    int cols;
    int x;

    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return -1;

    cols = web_get_map_cols();
    x = p_ptr->px - p_ptr->wx;

    if ((x < 0) || (x >= cols))
        return -1;

    return x;
}

EMSCRIPTEN_KEEPALIVE int web_get_player_map_y(void)
{
    int rows;
    int y;

    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return -1;

    rows = web_get_map_rows();
    y = p_ptr->py - p_ptr->wy;

    if ((y < 0) || (y >= rows))
        return -1;

    return y;
}

EMSCRIPTEN_KEEPALIVE int web_get_icon_alert_attr(void)
{
    return (int)misc_to_attr[ICON_ALERT];
}

EMSCRIPTEN_KEEPALIVE int web_get_icon_alert_char(void)
{
    return (int)misc_to_char[ICON_ALERT];
}

EMSCRIPTEN_KEEPALIVE int web_get_icon_glow_attr(void)
{
    return (int)misc_to_attr[ICON_GLOW];
}

EMSCRIPTEN_KEEPALIVE int web_get_icon_glow_char(void)
{
    return (int)misc_to_char[ICON_GLOW];
}

EMSCRIPTEN_KEEPALIVE uint32_t web_get_frame_id(void)
{
    web_sync_map_marks_revision();
    return data.frame_id;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_fx_cells_ptr(void)
{
    return (uintptr_t)web_fx_cells;
}

EMSCRIPTEN_KEEPALIVE int web_get_fx_cells_cols(void)
{
    return web_fx_cols;
}

EMSCRIPTEN_KEEPALIVE int web_get_fx_cells_rows(void)
{
    return web_fx_rows;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_items_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_items();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_item_count(void)
{
    return ui_menu_get_item_count();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_item_stride(void)
{
    return (int)sizeof(ui_menu_item);
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_text_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_text();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_text_len(void)
{
    return ui_menu_get_text_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_attrs_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_attrs();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_attrs_len(void)
{
    return ui_menu_get_attrs_len();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_active_x(void)
{
    return ui_menu_get_active_column();
}

EMSCRIPTEN_KEEPALIVE unsigned int web_get_menu_revision(void)
{
    return ui_menu_get_revision();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_snapshot_retained(void)
{
    return ui_menu_snapshot_retained() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_details_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_details();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_len(void)
{
    return ui_menu_get_details_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_details_attrs_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_details_attrs();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_attrs_len(void)
{
    return ui_menu_get_details_attrs_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_summary_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_summary();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_len(void)
{
    return ui_menu_get_summary_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_menu_summary_attrs_ptr(void)
{
    return (uintptr_t)(const void*)ui_menu_get_summary_attrs();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_attrs_len(void)
{
    return ui_menu_get_summary_attrs_len();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_width(void)
{
    return ui_menu_get_details_width();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_summary_rows(void)
{
    return ui_menu_get_summary_rows();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_kind(void)
{
    return ui_menu_get_details_visual_kind();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_attr(void)
{
    return ui_menu_get_details_visual_attr();
}

EMSCRIPTEN_KEEPALIVE int web_get_menu_details_visual_char(void)
{
    return ui_menu_get_details_visual_char();
}

EMSCRIPTEN_KEEPALIVE int web_menu_hover(int index)
{
    const ui_menu_item* items = ui_menu_get_items();
    int count = ui_menu_get_item_count();
    int selected;
    int step;
    int target;
    int i;

    if ((index < 0) || (index >= count))
        return 0;

    if (items[index].nav_len > 0)
    {
        for (i = 0; i < items[index].nav_len; i++)
        {
            if (!web_key_enqueue((byte)items[index].nav[i]))
                return 0;
        }

        ui_menu_select_index(index);
        return 1;
    }

    selected = ui_menu_get_selected_index();
    if (selected < 0)
        return 0;

    if (selected == index)
        return 1;

    step = (index > selected) ? 1 : -1;
    for (target = selected; target != index; target += step)
    {
        if (!web_key_enqueue((step > 0) ? '2' : '8'))
            return 0;
    }

    ui_menu_select_index(index);

    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_menu_activate(int index)
{
    const ui_menu_item* items = ui_menu_get_items();
    int count = ui_menu_get_item_count();
    int key;

    if ((index < 0) || (index >= count))
        return 0;

    if (!web_menu_hover(index))
        return 0;

    key = items[index].key;
    if (key <= 0)
        return 1;

    return web_key_enqueue(key) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_modal_text_ptr(void)
{
    return (uintptr_t)(const void*)ui_modal_get_text();
}

EMSCRIPTEN_KEEPALIVE int web_get_modal_text_len(void)
{
    return ui_modal_get_text_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_modal_attrs_ptr(void)
{
    return (uintptr_t)(const void*)ui_modal_get_attrs();
}

EMSCRIPTEN_KEEPALIVE int web_get_modal_attrs_len(void)
{
    return ui_modal_get_attrs_len();
}

EMSCRIPTEN_KEEPALIVE int web_get_modal_dismiss_key(void)
{
    return ui_modal_get_dismiss_key();
}

EMSCRIPTEN_KEEPALIVE unsigned int web_get_modal_revision(void)
{
    return ui_modal_get_revision();
}

EMSCRIPTEN_KEEPALIVE int web_get_prompt_kind(void)
{
    ui_prompt_kind kind = ui_prompt_get_kind();

    return (int)kind;
}

EMSCRIPTEN_KEEPALIVE int web_get_prompt_more_hint(void)
{
    return ui_prompt_get_more_hint() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_prompt_text_ptr(void)
{
    const char* text = ui_prompt_get_text();

    return (uintptr_t)text;
}

EMSCRIPTEN_KEEPALIVE int web_get_prompt_text_len(void)
{
    return ui_prompt_get_text_len();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_prompt_attrs_ptr(void)
{
    const byte* attrs = ui_prompt_get_attrs();

    return (uintptr_t)attrs;
}

EMSCRIPTEN_KEEPALIVE int web_get_prompt_attrs_len(void)
{
    return ui_prompt_get_attrs_len();
}

EMSCRIPTEN_KEEPALIVE unsigned int web_get_prompt_revision(void)
{
    return ui_prompt_get_revision();
}

EMSCRIPTEN_KEEPALIVE int web_modal_activate(void)
{
    int key = ui_modal_get_dismiss_key();

    if ((ui_modal_get_text_len() <= 0) || (key <= 0))
        return 0;

    return web_key_enqueue(key) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int web_get_cursor_x(void)
{
    return data.cursor_x;
}

EMSCRIPTEN_KEEPALIVE int web_get_cursor_y(void)
{
    return data.cursor_y;
}

EMSCRIPTEN_KEEPALIVE int web_get_cursor_visible(void)
{
    return data.cursor_visible ? 1 : 0;
}

/* Queues the current-square interaction for one floating web action button. */
EMSCRIPTEN_KEEPALIVE int web_act_here(void)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (!current_square_action_available())
        return 0;

    if (travel_is_running())
        travel_clear();

    return web_key_enqueue(ACT_HERE_CMD) ? 1 : 0;
}

/* Queues one adjacent alter action for one floating directional web button. */
EMSCRIPTEN_KEEPALIVE int web_act_adjacent(int dir)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (!adjacent_action_available(dir))
        return 0;

    if (travel_is_running())
        travel_clear();

    if (!web_key_enqueue('/'))
        return 0;

    return web_key_enqueue(I2D(dir)) ? 1 : 0;
}

/* Queues the unified inventory command for one floating web action button. */
EMSCRIPTEN_KEEPALIVE int web_open_inventory(void)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (travel_is_running())
        travel_clear();

    return web_key_enqueue(INVENTORY_CMD) ? 1 : 0;
}

/* Queues the ranged fire command and opens the normal targeter when possible. */
EMSCRIPTEN_KEEPALIVE int web_open_ranged_target(void)
{
    int quiver;
    int fire_key;
    bool ready = FALSE;

    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (!inventory[INVEN_BOW].k_idx)
        return 0;

    if (travel_is_running())
        travel_clear();

    quiver = web_choose_ranged_quiver(&ready);
    fire_key = (quiver == 2) ? 'F' : 'f';

    if (!web_key_enqueue(fire_key))
        return 0;

    if (!ready)
        return 1;

    return web_key_enqueue('*') ? 1 : 0;
}

/* Moves an active targeter preview to one tapped map square. */
EMSCRIPTEN_KEEPALIVE int web_target_map(int y, int x)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing || !in_bounds(y, x))
        return 0;

    if (!ui_marks_targeting_active())
        return 0;

    ui_marks_request_target_grid(y, x);

    return web_key_enqueue('5') ? 1 : 0;
}

/* Queues the song-selection command for one floating web action button. */
EMSCRIPTEN_KEEPALIVE int web_toggle_stealth(void)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (travel_is_running())
        travel_clear();

    return web_key_enqueue('S') ? 1 : 0;
}

/* Queues the song-selection command for one floating web action button. */
EMSCRIPTEN_KEEPALIVE int web_open_song_menu(void)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (travel_is_running())
        travel_clear();

    if (!web_key_enqueue('s'))
        return 0;

    if ((p_ptr->song1 != SNG_NOTHING) || (p_ptr->song2 != SNG_NOTHING))
        return web_key_enqueue('s') ? 1 : 0;

    return 1;
}

/* Saves the active game state without disturbing the player. */
EMSCRIPTEN_KEEPALIVE int web_save_game_automatically(void)
{
    if (!save_game_automatically())
        return 0;

    web_update_auto_resume_marker(TRUE);
    return 1;
}

/* Requests one immediate autosave from the web frontend. */
EMSCRIPTEN_KEEPALIVE __attribute__((noinline)) int web_request_autosave(void)
{
    return web_save_game_automatically();
}

EMSCRIPTEN_KEEPALIVE int web_travel_to(int y, int x)
{
    int dir;

    if (!p_ptr || !character_dungeon || !p_ptr->playing || !in_bounds(y, x))
        return 0;

    if (travel_is_running())
        travel_clear();

    if ((y == p_ptr->py) && (x == p_ptr->px))
        return web_key_enqueue(HOLD_CMD) ? 1 : 0;

    dir = motion_dir(p_ptr->py, p_ptr->px, y, x);
    if ((dir != 5)
        && (ABS(y - p_ptr->py) <= 1)
        && (ABS(x - p_ptr->px) <= 1)
        && (cave_m_idx[y][x] > 0)
        && mon_list[cave_m_idx[y][x]].ml)
    {
        if (!web_key_enqueue(';'))
            return 0;
        return web_key_enqueue(I2D(dir)) ? 1 : 0;
    }

    travel_set_target(y, x);

    return web_key_enqueue(TRAVEL_CMD) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int web_push_key(int key)
{
    return web_key_enqueue(key) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int web_get_sound_count(void)
{
    return SOUND_MAX;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_sound_name_ptr(int index)
{
    if ((index < 0) || (index >= SOUND_MAX))
        return 0;

    return (uintptr_t)(const void*)angband_sound_name[index];
}

EMSCRIPTEN_KEEPALIVE int web_get_sound_name_len(int index)
{
    cptr sound_name;

    if ((index < 0) || (index >= SOUND_MAX))
        return 0;

    sound_name = angband_sound_name[index];
    if (!sound_name)
        return 0;

    return (int)strlen(sound_name);
}

EMSCRIPTEN_KEEPALIVE int web_pop_sound_event(void)
{
    int sound_event;

    if (web_sound_head == web_sound_tail)
        return -1;

    sound_event = web_sound_queue[web_sound_tail];
    web_sound_tail = (web_sound_tail + 1) % WEB_SOUND_QUEUE_SIZE;
    return sound_event;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_log_text_ptr(void)
{
    web_refresh_log_text();
    return (uintptr_t)web_log_text;
}

EMSCRIPTEN_KEEPALIVE int web_get_log_text_len(void)
{
    web_refresh_log_text();
    return (int)strlen(web_log_text);
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_log_attrs_ptr(void)
{
    web_refresh_log_text();
    return (uintptr_t)web_log_attrs;
}

EMSCRIPTEN_KEEPALIVE int web_get_log_attrs_len(void)
{
    web_refresh_log_text();
    return web_log_attrs_len;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_birth_state_ptr(void)
{
    web_refresh_birth_state();
    return (uintptr_t)web_birth_state;
}

EMSCRIPTEN_KEEPALIVE int web_get_birth_state_len(void)
{
    web_refresh_birth_state();
    return (int)strlen(web_birth_state);
}

EMSCRIPTEN_KEEPALIVE unsigned int web_get_birth_state_revision(void)
{
    return ui_birth_revision();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_birth_submit_text_ptr(void)
{
    return (uintptr_t)(const void*)web_birth_submission.text;
}

EMSCRIPTEN_KEEPALIVE int web_submit_birth_form(
    int kind, int age, int height, int weight)
{
    if ((kind != UI_BIRTH_SCREEN_NAME) && (kind != UI_BIRTH_SCREEN_HISTORY)
        && (kind != UI_BIRTH_SCREEN_AHW))
        return 0;

    web_birth_submission.pending = TRUE;
    web_birth_submission.kind = (enum ui_birth_screen_kind)kind;
    web_birth_submission.age = age;
    web_birth_submission.height = height;
    web_birth_submission.weight = weight;
    web_birth_submission.text[UI_BIRTH_HISTORY_TEXT_MAX - 1] = '\0';
    return 1;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_character_sheet_state_ptr(void)
{
    web_refresh_character_sheet_state();
    return (uintptr_t)web_character_sheet_state;
}

EMSCRIPTEN_KEEPALIVE int web_get_character_sheet_state_len(void)
{
    web_refresh_character_sheet_state();
    return (int)strlen(web_character_sheet_state);
}

EMSCRIPTEN_KEEPALIVE unsigned int web_get_character_sheet_state_revision(void)
{
    return ui_character_sheet_revision();
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_player_state_ptr(void)
{
    web_refresh_player_state();
    return (uintptr_t)web_player_state;
}

EMSCRIPTEN_KEEPALIVE int web_get_player_state_len(void)
{
    web_refresh_player_state();
    return (int)strlen(web_player_state);
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_tile_context_state_ptr(int dir)
{
    web_build_tile_context_state(dir);
    return (uintptr_t)web_tile_context_state;
}

EMSCRIPTEN_KEEPALIVE int web_get_tile_context_state_len(int dir)
{
    web_build_tile_context_state(dir);
    return (int)strlen(web_tile_context_state);
}

EMSCRIPTEN_KEEPALIVE int web_execute_tile_context_action(int dir, int action_id)
{
    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return 0;

    if (!web_tile_context_has_action(dir, action_id))
        return 0;

    if (travel_is_running())
        travel_clear();

    switch (action_id)
    {
    case GRID_CONTEXT_ACTION_DEFAULT:
        if (dir == 5)
            return web_key_enqueue(ACT_HERE_CMD) ? 1 : 0;

        if (!web_key_enqueue('/'))
            return 0;
        return web_key_enqueue(I2D(dir)) ? 1 : 0;

    case GRID_CONTEXT_ACTION_PICKUP:
        return web_key_enqueue('g') ? 1 : 0;

    case GRID_CONTEXT_ACTION_ASCEND:
        return web_key_enqueue('<') ? 1 : 0;

    case GRID_CONTEXT_ACTION_DESCEND:
        return web_key_enqueue('>') ? 1 : 0;

    case GRID_CONTEXT_ACTION_USE_FORGE:
        return web_key_enqueue('0') ? 1 : 0;

    case GRID_CONTEXT_ACTION_OPEN:
        if (!web_key_enqueue('o'))
            return 0;

        if (dir == 5)
            return 1;

        return web_key_enqueue(I2D(dir)) ? 1 : 0;

    default:
        return 0;
    }
}

EMSCRIPTEN_KEEPALIVE int web_get_overlay_mode(void)
{
    web_refresh_overlay_text();
    return web_overlay_mode;
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_overlay_text_ptr(void)
{
    web_refresh_overlay_text();
    return (uintptr_t)web_overlay_text;
}

EMSCRIPTEN_KEEPALIVE int web_get_overlay_text_len(void)
{
    web_refresh_overlay_text();
    return (int)strlen(web_overlay_text);
}

EMSCRIPTEN_KEEPALIVE uintptr_t web_get_overlay_attrs_ptr(void)
{
    web_refresh_overlay_text();
    return (uintptr_t)web_overlay_attrs;
}

EMSCRIPTEN_KEEPALIVE int web_get_overlay_attrs_len(void)
{
    web_refresh_overlay_text();
    return web_overlay_attrs_len;
}

EMSCRIPTEN_KEEPALIVE uint32_t web_get_fx_delay_seq(void)
{
    return web_fx_delay_seq;
}

EMSCRIPTEN_KEEPALIVE int web_get_fx_delay_msec(void)
{
    return web_fx_delay_msec;
}

#endif /* USE_WEB */
