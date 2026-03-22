/* File: travel.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL 
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Destination-based automatic travel support.
 *
 * This is currently wired up by the web frontend, but the core logic is kept
 * independent of how the destination was chosen so other frontends can reuse it.
 */

#include "angband.h"

#ifdef USE_WEB

#define TRAVEL_PATH_MAX (MAX_DUNGEON_HGT * MAX_DUNGEON_WID)

static bool travel_pending = FALSE;
static int travel_pending_y = 0;
static int travel_pending_x = 0;
static bool travel_active = FALSE;
static s16b travel_message_count = 0;
static bool travel_visible_monsters[MAX_MONSTERS];
static int travel_path_len = 0;
static int travel_path_pos = 0;
static byte travel_path[TRAVEL_PATH_MAX];
static byte travel_prev_dir[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static byte travel_closed[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int travel_cost[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static int travel_heap_pos[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
static s16b travel_queue_y[TRAVEL_PATH_MAX];
static s16b travel_queue_x[TRAVEL_PATH_MAX];
static int travel_queue_size = 0;

static const byte travel_dirs[] = { 1, 2, 3, 4, 6, 7, 8, 9 };

/* Clears the currently active travel state without touching the chosen target. */
static void travel_reset_active(void)
{
    travel_active = FALSE;
    travel_path_len = 0;
    travel_path_pos = 0;
}

/* Updates the baseline used to notice new messages during automatic travel. */
static void travel_sync_messages(void)
{
    travel_message_count = message_num();
}

/* Returns whether any new message appeared since the last travel step. */
static bool travel_messages_changed(void)
{
    return (message_num() != travel_message_count);
}

/* Records which monsters are visible at the current travel checkpoint. */
static void travel_sync_visible_monsters(void)
{
    int i;

    memset(travel_visible_monsters, 0, sizeof(travel_visible_monsters));

    for (i = 1; i < mon_max; i++)
    {
        if (mon_list[i].r_idx && mon_list[i].ml)
            travel_visible_monsters[i] = TRUE;
    }
}

/* Returns whether any monster became newly visible since the last checkpoint. */
static bool travel_new_visible_monster(void)
{
    int i;

    for (i = 1; i < mon_max; i++)
    {
        if (mon_list[i].r_idx && mon_list[i].ml && !travel_visible_monsters[i])
            return TRUE;
    }

    return FALSE;
}

/* Returns whether one tile is currently known to the player. */
static bool travel_tile_known(int y, int x)
{
    if (!in_bounds(y, x))
        return FALSE;

    return ((cave_info[y][x] & (CAVE_MARK)) || (cave_info[y][x] & (CAVE_SEEN)));
}

/* Returns whether one known tile may be used for routed travel. */
static bool travel_tile_passable(int y, int x)
{
    if (!travel_tile_known(y, x))
        return FALSE;

    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
        return FALSE;

    if (cave_known_closed_door_bold(y, x))
        return TRUE;

    if (!cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
        return FALSE;

    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
        return FALSE;

    return TRUE;
}

/* Returns whether one known terrain tile is solid enough to block a diagonal. */
static bool travel_tile_solid_known(int y, int x)
{
    if (!travel_tile_known(y, x))
        return FALSE;

    if (cave_known_closed_door_bold(y, x))
        return FALSE;

    if (!cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
        return TRUE;

    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
        return TRUE;

    return FALSE;
}

/* Returns whether a diagonal step would clip through two solid known corners. */
static bool travel_diagonal_blocked(int y, int x, int dir)
{
    if ((ddy[dir] == 0) || (ddx[dir] == 0))
        return FALSE;

    return travel_tile_solid_known(y + ddy[dir], x)
        && travel_tile_solid_known(y, x + ddx[dir]);
}

/* Returns the planner cost for stepping onto one destination tile. */
static int travel_tile_route_cost(int y, int x)
{
    if (!in_bounds(y, x))
        return -1;

    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
        return -1;

    if (!travel_tile_known(y, x))
        return 30;

    if (cave_known_closed_door_bold(y, x))
        return 20;

    if (!cave_floor_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
        return -1;

    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
        return -1;

    return 10;
}

/* Returns the planner cost for one routed step from the current origin tile. */
static int travel_step_route_cost(int y, int x, int dir)
{
    int ny = y + ddy[dir];
    int nx = x + ddx[dir];

    if (!in_bounds(ny, nx))
        return -1;

    if (travel_diagonal_blocked(y, x, dir))
        return -1;

    return travel_tile_route_cost(ny, nx);
}

/* Returns whether the next travel step can be attempted immediately in play. */
static bool travel_step_actionable(int y, int x, int dir)
{
    int ny = y + ddy[dir];
    int nx = x + ddx[dir];

    if (!in_bounds(ny, nx))
        return FALSE;

    if (travel_diagonal_blocked(y, x, dir))
        return FALSE;

    if ((cave_m_idx[ny][nx] > 0) && mon_list[cave_m_idx[ny][nx]].ml)
        return FALSE;

    return do_cmd_walk_test(ny, nx);
}

/* Consumes the latest chosen travel target, if any. */
static bool travel_pull_pending(int* y, int* x)
{
    if (!travel_pending || !y || !x)
        return FALSE;

    *y = travel_pending_y;
    *x = travel_pending_x;
    travel_pending = FALSE;

    return TRUE;
}

/* Returns the squared distance from one tile to the requested target. */
static int travel_target_score(int y, int x, int target_y, int target_x)
{
    int dy = target_y - y;
    int dx = target_x - x;

    return (dy * dy) + (dx * dx);
}

/* Returns the optimistic remaining turn cost from one tile to the target. */
static int travel_estimate_cost(int y, int x, int target_y, int target_x)
{
    int dy = ABS(target_y - y);
    int dx = ABS(target_x - x);

    return MAX(dy, dx) * 10;
}

/* Orders movement directions to prefer tiles that stay closest to the target. */
static void travel_order_dirs(int y, int x, int target_y, int target_x,
    byte ordered_dirs[N_ELEMENTS(travel_dirs)])
{
    int i;

    for (i = 0; i < (int)N_ELEMENTS(travel_dirs); i++)
    {
        ordered_dirs[i] = travel_dirs[i];
    }

    for (i = 1; i < (int)N_ELEMENTS(travel_dirs); i++)
    {
        byte dir = ordered_dirs[i];
        int dir_score = travel_target_score(y + ddy[dir], x + ddx[dir],
            target_y, target_x);
        int j = i;

        while (j > 0)
        {
            byte prev_dir = ordered_dirs[j - 1];
            int prev_score = travel_target_score(y + ddy[prev_dir],
                x + ddx[prev_dir], target_y, target_x);

            if (prev_score <= dir_score)
                break;

            ordered_dirs[j] = prev_dir;
            j--;
        }

        ordered_dirs[j] = dir;
    }
}

/* Reconstructs one queued travel path from the chosen destination back to the player. */
static bool travel_reconstruct_path(int dest_y, int dest_x)
{
    int py;
    int px;
    int y;
    int x;
    int swap_i;

    if (!p_ptr)
        return FALSE;

    py = p_ptr->py;
    px = p_ptr->px;
    travel_path_len = 0;
    y = dest_y;
    x = dest_x;

    while ((y != py) || (x != px))
    {
        byte dir = travel_prev_dir[y][x];

        if ((dir == 0) || (dir == 5) || (travel_path_len >= TRAVEL_PATH_MAX))
        {
            travel_reset_active();
            return FALSE;
        }

        travel_path[travel_path_len++] = dir;
        y -= ddy[dir];
        x -= ddx[dir];
    }

    for (swap_i = 0; swap_i < travel_path_len / 2; swap_i++)
    {
        byte tmp = travel_path[swap_i];
        int other = travel_path_len - 1 - swap_i;

        travel_path[swap_i] = travel_path[other];
        travel_path[other] = tmp;
    }

    travel_path_pos = 0;

    return (travel_path_len > 0);
}

/* Returns the current heap priority for one queued travel node. */
static int travel_heap_score_at(int y, int x, int target_y, int target_x)
{
    return travel_cost[y][x] + travel_estimate_cost(y, x, target_y, target_x);
}

/* Swaps two entries in the travel open-set heap. */
static void travel_heap_swap(int a, int b)
{
    s16b y = travel_queue_y[a];
    s16b x = travel_queue_x[a];

    travel_queue_y[a] = travel_queue_y[b];
    travel_queue_x[a] = travel_queue_x[b];
    travel_queue_y[b] = y;
    travel_queue_x[b] = x;

    travel_heap_pos[travel_queue_y[a]][travel_queue_x[a]] = a;
    travel_heap_pos[travel_queue_y[b]][travel_queue_x[b]] = b;
}

/* Moves one heap entry upward after its travel score improves. */
static void travel_heap_sift_up(int index, int target_y, int target_x)
{
    while (index > 0)
    {
        int parent = (index - 1) / 2;
        int score = travel_heap_score_at(travel_queue_y[index],
            travel_queue_x[index], target_y, target_x);
        int parent_score = travel_heap_score_at(travel_queue_y[parent],
            travel_queue_x[parent], target_y, target_x);

        if (parent_score <= score)
            break;

        travel_heap_swap(index, parent);
        index = parent;
    }
}

/* Moves one heap entry downward after removing the current best node. */
static void travel_heap_sift_down(int index, int target_y, int target_x)
{
    while (TRUE)
    {
        int left = (index * 2) + 1;
        int right = left + 1;
        int best = index;

        if (left < travel_queue_size)
        {
            int left_score = travel_heap_score_at(travel_queue_y[left],
                travel_queue_x[left], target_y, target_x);
            int best_score = travel_heap_score_at(travel_queue_y[best],
                travel_queue_x[best], target_y, target_x);

            if (left_score < best_score)
                best = left;
        }

        if (right < travel_queue_size)
        {
            int right_score = travel_heap_score_at(travel_queue_y[right],
                travel_queue_x[right], target_y, target_x);
            int best_score = travel_heap_score_at(travel_queue_y[best],
                travel_queue_x[best], target_y, target_x);

            if (right_score < best_score)
                best = right;
        }

        if (best == index)
            break;

        travel_heap_swap(index, best);
        index = best;
    }
}

/* Inserts or decreases one travel node in the open-set heap. */
static bool travel_heap_push(int y, int x, int target_y, int target_x)
{
    int index = travel_heap_pos[y][x];

    if (index >= 0)
    {
        travel_heap_sift_up(index, target_y, target_x);
        return TRUE;
    }

    if (travel_queue_size >= TRAVEL_PATH_MAX)
        return FALSE;

    index = travel_queue_size++;
    travel_queue_y[index] = (s16b)y;
    travel_queue_x[index] = (s16b)x;
    travel_heap_pos[y][x] = index;
    travel_heap_sift_up(index, target_y, target_x);
    return TRUE;
}

/* Removes the best next node from the travel open-set heap. */
static bool travel_heap_pop(int target_y, int target_x, int* y, int* x)
{
    if ((travel_queue_size <= 0) || !y || !x)
        return FALSE;

    *y = travel_queue_y[0];
    *x = travel_queue_x[0];
    travel_heap_pos[*y][*x] = -1;
    travel_queue_size--;

    if (travel_queue_size > 0)
    {
        travel_queue_y[0] = travel_queue_y[travel_queue_size];
        travel_queue_x[0] = travel_queue_x[travel_queue_size];
        travel_heap_pos[travel_queue_y[0]][travel_queue_x[0]] = 0;
        travel_heap_sift_down(0, target_y, target_x);
    }

    return TRUE;
}

/* Builds one best-effort route that strongly prefers known safe terrain. */
static bool travel_build_path(int target_y, int target_x)
{
    int py;
    int px;
    int y;
    int x;
    int i;
    byte ordered_dirs[N_ELEMENTS(travel_dirs)];

    if (!p_ptr)
        return FALSE;

    py = p_ptr->py;
    px = p_ptr->px;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        memset(travel_prev_dir[y], 0,
            (size_t)p_ptr->cur_map_wid * sizeof(travel_prev_dir[y][0]));
        memset(travel_closed[y], 0,
            (size_t)p_ptr->cur_map_wid * sizeof(travel_closed[y][0]));
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            travel_cost[y][x] = 0x3fffffff;
            travel_heap_pos[y][x] = -1;
        }
    }

    travel_queue_size = 0;
    travel_prev_dir[py][px] = 5;
    travel_cost[py][px] = 0;
    if (!travel_heap_push(py, px, target_y, target_x))
        return FALSE;

    while (travel_heap_pop(target_y, target_x, &y, &x))
    {
        if (travel_closed[y][x])
            continue;
        travel_closed[y][x] = TRUE;

        if ((y == target_y) && (x == target_x))
            break;

        travel_order_dirs(y, x, target_y, target_x, ordered_dirs);

        for (i = 0; i < (int)N_ELEMENTS(travel_dirs); i++)
        {
            int dir = ordered_dirs[i];
            int ny = y + ddy[dir];
            int nx = x + ddx[dir];
            int step_cost = travel_step_route_cost(y, x, dir);
            int new_cost;

            if (step_cost < 0)
                continue;
            if (travel_closed[ny][nx])
                continue;
            new_cost = travel_cost[y][x] + step_cost;
            if (new_cost >= travel_cost[ny][nx])
                continue;

            travel_prev_dir[ny][nx] = (byte)dir;
            travel_cost[ny][nx] = new_cost;
            if (!travel_heap_push(ny, nx, target_y, target_x))
                return FALSE;
        }
    }

    if (!travel_prev_dir[target_y][target_x])
        return FALSE;

    return travel_reconstruct_path(target_y, target_x);
}

/* Tries one direct exploratory step when no known route is available yet. */
static bool travel_try_direct_step(int target_y, int target_x)
{
    int dir;
    int best_known_dir = 0;
    int best_unknown_dir = 0;
    int y;
    int x;
    int start_y;
    int start_x;
    int i;
    byte ordered_dirs[N_ELEMENTS(travel_dirs)];

    if (!p_ptr)
        return FALSE;

    travel_order_dirs(p_ptr->py, p_ptr->px, target_y, target_x, ordered_dirs);

    for (i = 0; i < (int)N_ELEMENTS(travel_dirs); i++)
    {
        dir = ordered_dirs[i];
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];

        if (!in_bounds(y, x))
            continue;
        if (travel_diagonal_blocked(p_ptr->py, p_ptr->px, dir))
            continue;

        if (travel_tile_passable(y, x))
        {
            best_known_dir = dir;
            break;
        }

        if (!travel_tile_known(y, x) && !best_unknown_dir
            && do_cmd_walk_test(y, x))
        {
            best_unknown_dir = dir;
        }
    }

    dir = best_known_dir ? best_known_dir : best_unknown_dir;
    if (!dir)
        return FALSE;

    start_y = p_ptr->py;
    start_x = p_ptr->px;
    p_ptr->energy_use = 100;
    move_player(dir);

    return ((p_ptr->py != start_y) || (p_ptr->px != start_x)
        || (p_ptr->energy_use > 0));
}

/* Returns whether automatic travel is currently advancing between turns. */
bool travel_is_running(void)
{
    return travel_active;
}

/* Stores one chosen travel target for the next synthetic command. */
void travel_set_target(int y, int x)
{
    travel_pending_y = y;
    travel_pending_x = x;
    travel_pending = TRUE;
}

/* Cancels any pending or active automatic travel request. */
void travel_clear(void)
{
    travel_pending = FALSE;
    travel_reset_active();
    travel_sync_messages();
    travel_sync_visible_monsters();
}

/* Advances automatic travel by one game turn, if still valid. */
void travel_step(void)
{
    int start_y;
    int start_x;
    int dir;
    int next_y;
    int next_x;

    if (!p_ptr || !character_dungeon || !p_ptr->playing)
    {
        travel_reset_active();
        return;
    }

    if (!travel_active)
        return;

    if (travel_messages_changed() || p_ptr->confused
        || travel_new_visible_monster())
    {
        travel_reset_active();
        return;
    }

    if ((travel_path_pos < 0) || (travel_path_pos >= travel_path_len))
    {
        travel_reset_active();
        return;
    }

    dir = travel_path[travel_path_pos];
    next_y = p_ptr->py + ddy[dir];
    next_x = p_ptr->px + ddx[dir];

    if (!travel_step_actionable(p_ptr->py, p_ptr->px, dir))
    {
        travel_reset_active();
        return;
    }

    start_y = p_ptr->py;
    start_x = p_ptr->px;

    p_ptr->energy_use = 100;
    move_player(dir);

    if ((p_ptr->py == start_y) && (p_ptr->px == start_x))
    {
        if (travel_messages_changed() || (cave_m_idx[next_y][next_x] > 0)
            || travel_new_visible_monster() || (p_ptr->energy_use == 0))
        {
            travel_reset_active();
        }
        return;
    }

    if ((p_ptr->py != next_y) || (p_ptr->px != next_x))
    {
        travel_reset_active();
        return;
    }

    travel_path_pos++;

    if (travel_messages_changed() || (travel_path_pos >= travel_path_len)
        || travel_new_visible_monster())
    {
        travel_reset_active();
        return;
    }

    travel_sync_messages();
    travel_sync_visible_monsters();
}

/* Starts automatic travel, or a single exploratory step, from the last target. */
void travel_command(void)
{
    int target_y;
    int target_x;

    if (!travel_pull_pending(&target_y, &target_x))
        return;

    travel_reset_active();

    if (!p_ptr || !character_dungeon || !p_ptr->playing)
        return;

    if (!in_bounds(target_y, target_x))
        return;

    if ((target_y == p_ptr->py) && (target_x == p_ptr->px))
        return;

    if (p_ptr->confused)
        return;

    travel_sync_messages();
    travel_sync_visible_monsters();

    if (travel_build_path(target_y, target_x))
    {
        travel_active = TRUE;
        travel_step();
        return;
    }

    (void)travel_try_direct_step(target_y, target_x);
}

#endif /* USE_WEB */
