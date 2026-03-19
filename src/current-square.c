/* File: current-square.c */

/*
 * Helpers for interacting with the player's current square without prompting
 * for a direction.
 */

#include "angband.h"

typedef enum current_square_action_type current_square_action_type;
typedef struct current_square_action_info current_square_action_info;

enum current_square_action_type
{
    CURRENT_SQUARE_ACTION_NONE = 0,
    CURRENT_SQUARE_ACTION_DISARM_TRAP,
    CURRENT_SQUARE_ACTION_DISARM_CHEST,
    CURRENT_SQUARE_ACTION_OPEN_CHEST,
    CURRENT_SQUARE_ACTION_SEARCH_SKELETON,
    CURRENT_SQUARE_ACTION_ASCEND,
    CURRENT_SQUARE_ACTION_DESCEND,
    CURRENT_SQUARE_ACTION_USE_FORGE,
    CURRENT_SQUARE_ACTION_PICKUP
};

struct current_square_action_info
{
    current_square_action_type type;
    int feat;
    object_type* o_ptr;
};

/* Inspects the player's current square using the same branch order as alter. */
static current_square_action_info current_square_action_info_at_player(void)
{
    current_square_action_info info;
    int y;
    int x;
    int feat;
    bool chest_trap = FALSE;
    bool chest_present = FALSE;
    bool skeleton_present = FALSE;

    info.type = CURRENT_SQUARE_ACTION_NONE;
    info.feat = FEAT_NONE;
    info.o_ptr = NULL;

    if (!p_ptr || !character_dungeon || !in_bounds(p_ptr->py, p_ptr->px))
        return info;

    y = p_ptr->py;
    x = p_ptr->px;
    feat = cave_feat[y][x];

    if (!(cave_info[y][x] & CAVE_MARK))
        feat = FEAT_NONE;

    if (cave_o_idx[y][x])
    {
        info.o_ptr = &o_list[cave_o_idx[y][x]];

        if (info.o_ptr->tval == TV_CHEST)
        {
            chest_present = TRUE;

            if ((info.o_ptr->pval > 0) && chest_traps[info.o_ptr->pval]
                && object_known_p(info.o_ptr))
            {
                chest_trap = TRUE;
            }
        }
        else if (info.o_ptr->tval == TV_SKELETON)
        {
            skeleton_present = TRUE;
        }
    }

    if (cave_trap_bold(y, x) && !cave_floorlike_bold(y, x))
    {
        info.type = CURRENT_SQUARE_ACTION_DISARM_TRAP;
        info.feat = cave_feat[y][x];
    }
    else if (chest_trap)
    {
        info.type = CURRENT_SQUARE_ACTION_DISARM_CHEST;
    }
    else if (chest_present)
    {
        info.type = CURRENT_SQUARE_ACTION_OPEN_CHEST;
    }
    else if (skeleton_present)
    {
        info.type = CURRENT_SQUARE_ACTION_SEARCH_SKELETON;
    }
    else if ((feat == FEAT_LESS) || (feat == FEAT_LESS_SHAFT))
    {
        info.type = CURRENT_SQUARE_ACTION_ASCEND;
        info.feat = feat;
    }
    else if ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT))
    {
        info.type = CURRENT_SQUARE_ACTION_DESCEND;
        info.feat = feat;
    }
    else if (cave_forge_bold(y, x))
    {
        info.type = CURRENT_SQUARE_ACTION_USE_FORGE;
        info.feat = cave_feat[y][x];
    }
    else if (cave_o_idx[y][x])
    {
        info.type = CURRENT_SQUARE_ACTION_PICKUP;
    }

    return info;
}

/* Returns whether the player's current square has a direct context action. */
bool current_square_action_available(void)
{
    return current_square_action_info_at_player().type
        != CURRENT_SQUARE_ACTION_NONE;
}

/* Returns a short label describing what acting on the current square will do. */
cptr current_square_action_label(void)
{
    switch (current_square_action_info_at_player().type)
    {
    case CURRENT_SQUARE_ACTION_DISARM_TRAP:
        return "Disarm trap";

    case CURRENT_SQUARE_ACTION_DISARM_CHEST:
        return "Disarm chest";

    case CURRENT_SQUARE_ACTION_OPEN_CHEST:
        return "Open chest";

    case CURRENT_SQUARE_ACTION_SEARCH_SKELETON:
        return "Search skeleton";

    case CURRENT_SQUARE_ACTION_ASCEND:
        return "Ascend";

    case CURRENT_SQUARE_ACTION_DESCEND:
        return "Descend";

    case CURRENT_SQUARE_ACTION_USE_FORGE:
        return "Use forge";

    case CURRENT_SQUARE_ACTION_PICKUP:
        return "Pick up";

    case CURRENT_SQUARE_ACTION_NONE:
    default:
        return NULL;
    }
}

/* Exports one visual for the current-square action target, if any exists. */
void current_square_action_visual(byte* attr, byte* chr)
{
    current_square_action_info info = current_square_action_info_at_player();

    if (attr)
        *attr = TERM_WHITE;
    if (chr)
        *chr = (byte)' ';

    switch (info.type)
    {
    case CURRENT_SQUARE_ACTION_DISARM_TRAP:
    case CURRENT_SQUARE_ACTION_ASCEND:
    case CURRENT_SQUARE_ACTION_DESCEND:
    case CURRENT_SQUARE_ACTION_USE_FORGE:
        if ((info.feat > FEAT_NONE) && attr && chr)
        {
            *attr = f_info[info.feat].x_attr;
            *chr = f_info[info.feat].x_char;
        }
        break;

    case CURRENT_SQUARE_ACTION_DISARM_CHEST:
    case CURRENT_SQUARE_ACTION_OPEN_CHEST:
    case CURRENT_SQUARE_ACTION_SEARCH_SKELETON:
    case CURRENT_SQUARE_ACTION_PICKUP:
        if (info.o_ptr && info.o_ptr->k_idx && attr && chr)
        {
            *attr = (byte)object_attr(info.o_ptr);
            *chr = (byte)object_char(info.o_ptr);
        }
        break;

    case CURRENT_SQUARE_ACTION_NONE:
    default:
        break;
    }
}

/* Executes the generic alter/interact flow targeting the player's own square. */
void do_cmd_act_here(void)
{
    p_ptr->command_dir = 5;
    do_cmd_alter();
}
