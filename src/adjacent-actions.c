/* File: adjacent-actions.c */

/*
 * Helpers for exposing adjacent alter actions without prompting for direction.
 */

#include "angband.h"

typedef enum adjacent_action_type adjacent_action_type;
typedef struct adjacent_action_info adjacent_action_info;

enum adjacent_action_type
{
    ADJACENT_ACTION_NONE = 0,
    ADJACENT_ACTION_ATTACK,
    ADJACENT_ACTION_TUNNEL,
    ADJACENT_ACTION_BASH_DOOR,
    ADJACENT_ACTION_CLOSE_DOOR,
    ADJACENT_ACTION_DISARM_TRAP,
    ADJACENT_ACTION_DISARM_CHEST,
    ADJACENT_ACTION_OPEN_CHEST,
    ADJACENT_ACTION_SEARCH_SKELETON
};

struct adjacent_action_info
{
    adjacent_action_type type;
    int feat;
    object_type* o_ptr;
    monster_type* m_ptr;
};

/* Returns the digging score the player could use for a tunnel action. */
static int adjacent_tunnel_score(void)
{
    int i;
    int digging_score = 0;
    object_type* o_ptr = &inventory[INVEN_WIELD];
    u32b f1, f2, f3;

    object_flags(o_ptr, &f1, &f2, &f3);
    if ((f1 & TR1_TUNNEL) && (o_ptr->pval > 0))
        return o_ptr->pval;

    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];
        object_flags(o_ptr, &f1, &f2, &f3);

        if ((f1 & TR1_TUNNEL) && (o_ptr->pval > digging_score))
            digging_score = o_ptr->pval;
    }

    return digging_score;
}

/* Returns the tunnel difficulty for one adjacent feature. */
static int adjacent_tunnel_difficulty(int feat)
{
    if (feat == FEAT_WALL_PERM)
        return 0;

    if (feat >= FEAT_WALL_EXTRA)
        return 3;

    if (feat >= FEAT_QUARTZ)
        return 2;

    if (feat == FEAT_RUBBLE)
        return 1;

    return 3;
}

/* Returns whether the player can currently tunnel through one adjacent grid. */
static bool adjacent_can_tunnel(int y, int x)
{
    int difficulty;
    int digging_score;

    if (!cave_wall_bold(y, x))
        return FALSE;

    difficulty = adjacent_tunnel_difficulty(cave_feat[y][x]);
    if (difficulty <= 0)
        return FALSE;

    digging_score = adjacent_tunnel_score();
    if (digging_score < difficulty)
        return FALSE;

    if (p_ptr->stat_use[A_STR] < difficulty)
        return FALSE;

    return TRUE;
}

/* Inspects one adjacent grid using the same decision order as do_cmd_alter(). */
static adjacent_action_info adjacent_action_info_in_dir(int dir)
{
    adjacent_action_info info;
    int y;
    int x;
    bool chest_trap = FALSE;
    bool chest_present = FALSE;
    bool skeleton_present = FALSE;
    bool has_known_searchable = FALSE;
    bool is_marked;
    bool is_visible;

    info.type = ADJACENT_ACTION_NONE;
    info.feat = FEAT_NONE;
    info.o_ptr = NULL;
    info.m_ptr = NULL;

    if (!p_ptr || !character_dungeon || (dir < 1) || (dir > 9) || (dir == 5))
        return info;

    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];
    if (!in_bounds(y, x))
        return info;

    if (cave_o_idx[y][x])
    {
        info.o_ptr = &o_list[cave_o_idx[y][x]];

        if (info.o_ptr->tval == TV_CHEST)
        {
            chest_present = TRUE;
            if (info.o_ptr->marked)
                has_known_searchable = TRUE;

            if ((info.o_ptr->pval > 0) && chest_traps[info.o_ptr->pval]
                && object_known_p(info.o_ptr))
            {
                chest_trap = TRUE;
            }
        }
        else if (info.o_ptr->tval == TV_SKELETON)
        {
            skeleton_present = TRUE;
            if (info.o_ptr->marked)
                has_known_searchable = TRUE;
        }
    }

    is_marked = (cave_info[y][x] & CAVE_MARK) ? TRUE : FALSE;
    is_visible = (cave_info[y][x] & CAVE_SEEN) ? TRUE : FALSE;

    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
    {
        info.type = ADJACENT_ACTION_ATTACK;
        info.m_ptr = &mon_list[cave_m_idx[y][x]];
    }
    else if (!(is_marked || is_visible || has_known_searchable))
    {
        info.type = ADJACENT_ACTION_NONE;
    }
    else if (cave_known_closed_door_bold(y, x))
    {
        info.type = ADJACENT_ACTION_BASH_DOOR;
        info.feat = cave_feat[y][x];
    }
    else if (cave_feat[y][x] == FEAT_OPEN)
    {
        info.type = ADJACENT_ACTION_CLOSE_DOOR;
        info.feat = cave_feat[y][x];
    }
    else if (adjacent_can_tunnel(y, x))
    {
        info.type = ADJACENT_ACTION_TUNNEL;
        info.feat = cave_feat[y][x];
    }
    else if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        info.type = ADJACENT_ACTION_DISARM_TRAP;
        info.feat = cave_feat[y][x];
    }
    else if (chest_trap)
    {
        info.type = ADJACENT_ACTION_DISARM_CHEST;
    }
    else if (chest_present)
    {
        info.type = ADJACENT_ACTION_OPEN_CHEST;
    }
    else if (skeleton_present)
    {
        info.type = ADJACENT_ACTION_SEARCH_SKELETON;
    }

    return info;
}

/* Returns whether one adjacent direction has a direct context action. */
bool adjacent_action_available(int dir)
{
    return adjacent_action_info_in_dir(dir).type != ADJACENT_ACTION_NONE;
}

/* Returns whether one adjacent action should be presented as an attack. */
bool adjacent_action_is_attack(int dir)
{
    return adjacent_action_info_in_dir(dir).type == ADJACENT_ACTION_ATTACK;
}

/* Returns a short label describing what altering one adjacent grid will do. */
cptr adjacent_action_label(int dir)
{
    switch (adjacent_action_info_in_dir(dir).type)
    {
    case ADJACENT_ACTION_ATTACK:
        return "Attack";

    case ADJACENT_ACTION_TUNNEL:
        return "Tunnel";

    case ADJACENT_ACTION_BASH_DOOR:
        return "Bash door";

    case ADJACENT_ACTION_CLOSE_DOOR:
        return "Close door";

    case ADJACENT_ACTION_DISARM_TRAP:
        return "Disarm trap";

    case ADJACENT_ACTION_DISARM_CHEST:
        return "Disarm chest";

    case ADJACENT_ACTION_OPEN_CHEST:
        return "Open chest";

    case ADJACENT_ACTION_SEARCH_SKELETON:
        return "Search skeleton";

    case ADJACENT_ACTION_NONE:
    default:
        return NULL;
    }
}

/* Exports one visual for one adjacent action target, if any exists. */
void adjacent_action_visual(int dir, byte* attr, byte* chr)
{
    adjacent_action_info info = adjacent_action_info_in_dir(dir);

    if (attr)
        *attr = TERM_WHITE;
    if (chr)
        *chr = (byte)' ';

    switch (info.type)
    {
    case ADJACENT_ACTION_ATTACK:
        if (info.m_ptr && info.m_ptr->r_idx && attr && chr)
        {
            *attr = r_info[info.m_ptr->r_idx].x_attr;
            *chr = (byte)r_info[info.m_ptr->r_idx].x_char;
        }
        break;

    case ADJACENT_ACTION_TUNNEL:
    case ADJACENT_ACTION_BASH_DOOR:
    case ADJACENT_ACTION_CLOSE_DOOR:
    case ADJACENT_ACTION_DISARM_TRAP:
        if ((info.feat > FEAT_NONE) && attr && chr)
        {
            *attr = f_info[info.feat].x_attr;
            *chr = f_info[info.feat].x_char;
        }
        break;

    case ADJACENT_ACTION_DISARM_CHEST:
    case ADJACENT_ACTION_OPEN_CHEST:
    case ADJACENT_ACTION_SEARCH_SKELETON:
        if (info.o_ptr && info.o_ptr->k_idx && attr && chr)
        {
            *attr = (byte)object_attr(info.o_ptr);
            *chr = (byte)object_char(info.o_ptr);
        }
        break;

    case ADJACENT_ACTION_NONE:
    default:
        break;
    }
}
