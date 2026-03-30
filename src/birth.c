/* File: birth.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "ui-birth.h"
#include "ui-character.h"
#include "ui-input.h"

/* Locations of the tables on the screen */
#define QUESTION_ROW 3
#define TABLE_ROW 4
#define INSTRUCT_ROW 21

#define QUESTION_COL 2

/*
 * Forward declare
 */
typedef struct birther birther;

/*
 * A structure to hold "rolled" information
 */
struct birther
{
    s16b age;
    s16b wt;
    s16b ht;
    s16b sc;

    s16b stat[A_MAX];

    char history[250];
};

static int get_start_xp(void)
{
    if (birth_fixed_exp)
    {
        return PY_FIXED_EXP;
    }
    else
    {
        return PY_START_EXP;
    }
}

/*
 * Generate some info that the auto-roller ignores
 */
static void get_extra(void)
{
    p_ptr->new_exp = p_ptr->exp = get_start_xp();

    /* Player is not singing */
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
}

/*
 * Get the racial history, and social class, using the "history charts".
 */
static void get_history_aux(void)
{
    int i, chart, roll;

    /* Clear the previous history strings */
    p_ptr->history[0] = '\0';

    /* Starting place */
    chart = rp_ptr->hist;

    /* Process the history */
    while (chart)
    {
        /* Start over */
        i = 0;

        /* Roll for nobility */
        roll = dieroll(100);

        /* Get the proper entry in the table */
        while ((chart != h_info[i].chart))
            i++;

        if (h_info[i].house)
        {
            while ((p_ptr->phouse != h_info[i].house) && h_info[i].house)
                i++;
        }

        while (roll > h_info[i].roll)
            i++;

        /* Get the textual history */
        my_strcat(
            p_ptr->history, (h_text + h_info[i].text), sizeof(p_ptr->history));

        /* Add a space */
        my_strcat(p_ptr->history, " ", sizeof(p_ptr->history));

        /* Enter the next chart */
        chart = h_info[i].next;
    }
}

/*
 * Get the racial history, and social class, using the "history charts".
 */
static bool get_history(void)
{
    bool roll_history = !(p_ptr->history[0]);

    while (TRUE)
    {
        char draft[sizeof(p_ptr->history)];

        if (roll_history)
            get_history_aux();
        else
            roll_history = TRUE;
        my_strcpy(draft, p_ptr->history, sizeof(draft));

        switch (ui_birth_prompt_history(draft, sizeof(draft)))
        {
        case UI_BIRTH_HISTORY_PROMPT_ACCEPT:
            my_strcpy(p_ptr->history, draft, sizeof(p_ptr->history));
            p_ptr->redraw |= PR_MISC;
            return (TRUE);

        case UI_BIRTH_HISTORY_PROMPT_REROLL:
            roll_history = TRUE;
            break;

        case UI_BIRTH_HISTORY_PROMPT_CANCEL:
        default:
            return (FALSE);
        }
    }
}

/*
 * Computes character's age, height, and weight
 */
static void get_ahw_aux(void)
{
    /* Calculate the age */
    p_ptr->age = rand_range(rp_ptr->b_age, rp_ptr->m_age);

    /* Calculate the height/weight */
    p_ptr->ht = Rand_normal(rp_ptr->b_ht, rp_ptr->m_ht);
    p_ptr->wt = Rand_normal(rp_ptr->b_wt, rp_ptr->m_wt);

    // Make weight a bit proportional to height
    p_ptr->wt += p_ptr->ht / 5;
}

/* Computes the legal manual-entry bounds for age, height, and weight. */
static void get_ahw_bounds(int* age_l, int* age_h, int* height_l, int* height_h,
    int* weight_l, int* weight_h)
{
    if (age_l)
        *age_l = rp_ptr->b_age;
    if (age_h)
        *age_h = rp_ptr->m_age;
    if (height_l)
        *height_l = rp_ptr->b_ht - 5 * (rp_ptr->m_ht);
    if (height_h)
        *height_h = rp_ptr->b_ht + 5 * (rp_ptr->m_ht);
    if (weight_l)
        *weight_l = rp_ptr->b_wt / 2;
    if (weight_h)
        *weight_h = rp_ptr->b_wt * 2;
}

/*
 * Get the Age, Height and Weight.
 */
static bool get_ahw(void)
{
    bool roll_ahw = !p_ptr->age;

    while (TRUE)
    {
        int age_l, age_h, height_l, height_h, weight_l, weight_h;
        int age;
        int height;
        int weight;

        if (roll_ahw)
            get_ahw_aux();
        else
            roll_ahw = TRUE;
        get_ahw_bounds(
            &age_l, &age_h, &height_l, &height_h, &weight_l, &weight_h);

        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;

        switch (ui_birth_prompt_ahw(&age, &height, &weight, age_l, age_h,
            height_l, height_h, weight_l, weight_h))
        {
        case UI_BIRTH_AHW_PROMPT_ACCEPT:
            if ((age < age_l) || (age > age_h) || (height < height_l)
                || (height > height_h) || (weight < weight_l)
                || (weight > weight_h))
            {
                bell("Invalid age, height, or weight.");
                continue;
            }

            p_ptr->age = age;
            p_ptr->ht = height;
            p_ptr->wt = weight;
            p_ptr->redraw |= PR_MISC;
            return (TRUE);

        case UI_BIRTH_AHW_PROMPT_REROLL:
            roll_ahw = TRUE;
            break;

        case UI_BIRTH_AHW_PROMPT_CANCEL:
        default:
            return (FALSE);
        }
    }
}

/*
 * Clear all the global "character" data
 */
static void player_wipe(void)
{
    int i;
    char history[250];
    int stat[A_MAX] = { 0 };

    /* Backup the player choices */
    // Initialized to soothe compilation warnings
    byte prace = 0;
    byte phouse = 0;
    int age = 0;
    int height = 0;
    int weight = 0;

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Backup the player choices */
        prace = p_ptr->prace;
        phouse = p_ptr->phouse;
        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;
        strnfmt(history, sizeof(history), "%s", p_ptr->history);

        for (i = 0; i < A_MAX; i++)
        {
            if (!(p_ptr->noscore & 0x0008))
                stat[i] = p_ptr->stat_base[i]
                    - (rp_ptr->r_adj[i] + hp_ptr->h_adj[i]);
            else
                stat[i] = 0;
        }
    }

    /* Wipe the player */
    (void)WIPE(p_ptr, player_type);

    // only save the old information if there was a character loaded
    if (character_loaded_dead)
    {
        /* Restore the choices */
        p_ptr->prace = prace;
        p_ptr->phouse = phouse;
        p_ptr->game_type = 0;
        p_ptr->age = age;
        p_ptr->ht = height;
        p_ptr->wt = weight;
        strnfmt(p_ptr->history, 250, "%s", history);
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = stat[i];
        }
    }
    else
    {
        /* Reset */
        p_ptr->prace = 0;
        p_ptr->phouse = 0;
        p_ptr->game_type = 0;
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        p_ptr->history[0] = '\0';
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }

    /* Clear the inventory */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_wipe(&inventory[i]);
    }

    /* Start with no artefacts made yet */
    /* and clear the slots for in-game randarts */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        a_ptr->cur_num = 0;
        a_ptr->found_num = 0;
    }

    /*re-set the object_level*/
    object_level = 0;

    /* Reset the "objects" */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Reset "tried" */
        k_ptr->tried = FALSE;

        /* Reset "aware" */
        k_ptr->aware = FALSE;
    }

    /* Reset the "monsters" */
    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Hack -- Reset the counter */
        r_ptr->cur_num = 0;

        /* Hack -- Reset the max counter */
        r_ptr->max_num = 100;

        /* Hack -- Reset the max counter */
        if (r_ptr->flags1 & (RF1_UNIQUE))
            r_ptr->max_num = 1;

        /* Clear player sights/kills */
        l_ptr->psights = 0;
        l_ptr->pkills = 0;
    }

    /*No current player ghosts*/
    bones_selector = 0;

    // give the player the most food possible without a message showing
    p_ptr->food = PY_FOOD_FULL - 1;

    // reset the stair info
    p_ptr->stairs_taken = 0;
    p_ptr->staircasiness = 0;

    // reset the forge info
    p_ptr->fixed_forge_count = 0;
    p_ptr->forge_count = 0;

    // No vengeance at birth
    p_ptr->vengeance = 0;

    // Morgoth unhurt
    p_ptr->morgoth_state = 0;

    p_ptr->killed_enemy_with_arrow = FALSE;

    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;

    p_ptr->thrall_quest = QUEST_NOT_STARTED;

    p_ptr->unused2 = 0;
    p_ptr->unused3 = 0;

    /*re-set the thefts counter*/
    recent_failed_thefts = 0;

    /*re-set the altered inventory counter*/
    allow_altered_inventory = 0;

    // reset some unique flags
    p_ptr->unique_forge_made = FALSE;
    p_ptr->unique_forge_seen = FALSE;
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        p_ptr->greater_vaults[i] = 0;
    }
}

void player_birth_wipe(void) { player_wipe(); }

/*
 * Init players with some belongings
 *
 * Having an item identifies it and makes the player "aware" of its purpose.
 */
static void player_outfit(void)
{
    int i, slot, inven_slot;
    const start_item* e_ptr;
    object_type* i_ptr;
    object_type object_type_body;
    object_type* o_ptr;

    time_t c; // time variables
    struct tm* tp; //

    /* Hack -- Give the player his equipment */
    for (i = 0; i < MAX_START_ITEMS; i++)
    {
        /* Access the item */
        e_ptr = &(rp_ptr->start_items[i]);

        /* Get local object */
        i_ptr = &object_type_body;

        /* Hack	-- Give the player an object */
        if (e_ptr->tval > 0)
        {
            /* Get the object_kind */
            s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);
            object_kind* k_ptr = &k_info[k_idx];

            /* Valid item? */
            if (!k_idx)
                continue;

            /* Prepare the item */
            object_prep(i_ptr, k_idx);
            i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);
            i_ptr->weight = k_ptr->weight;
        }

        /* Check the slot */
        slot = wield_slot(i_ptr);

        /* give light sources a duration */
        if (slot == INVEN_LITE)
        {
            i_ptr->timeout = 2000;
        }

        object_known(i_ptr);

        /*put it in the inventory*/
        inven_slot = inven_carry(i_ptr, TRUE);

        /*if player can wield an item, do so*/
        if (slot >= INVEN_WIELD)
        {
            /* Get the wield slot */
            o_ptr = &inventory[slot];

            /* Wear the new stuff */
            object_copy(o_ptr, i_ptr);

            /* Modify quantity */
            if (o_ptr->tval != TV_ARROW)
                o_ptr->number = 1;

            /* Decrease the item */
            inven_item_increase(inven_slot, -(o_ptr->number));
            inven_item_optimize(inven_slot);

            /* Increment the equip counter by hand */
            p_ptr->equip_cnt++;
        }

        /*Bugfix:  So we don't get duplicate objects*/
        object_wipe(i_ptr);
    }

    // Christmas presents:

    c = time((time_t*)0);
    tp = localtime(&c);
    if ((tp->tm_mon == 11) && (tp->tm_mday >= 25))
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Get the object_kind */
        s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);

        /* Prepare the item */
        object_prep(i_ptr, k_idx);
        i_ptr->number = 1;
        i_ptr->pval = -20;

        /*put it in the inventory*/
        inven_slot = inven_carry(i_ptr, TRUE);
    }

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Recalculate mana */
    p_ptr->update |= (PU_MANA);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
    ;
}

/*
 * Clear the previous question
 */
static void clear_question(void)
{
    int i;

    for (i = QUESTION_ROW; i < TABLE_ROW; i++)
    {
        /* Clear line, position cursor */
        Term_erase(0, i, 255);
    }
}

/*
 * Player race
 */
static bool get_player_race(void)
{
    int i;
    int race;
    race = ui_birth_choose_race(p_ptr->prace);

    /* No selection? */
    if (race == UI_BIRTH_CHOICE_CANCELLED)
    {
        return (FALSE);
    }

    // if different race to last time, then wipe the history, age, height,
    // weight
    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }
    p_ptr->prace = race;

    /* Save the race pointer */
    rp_ptr = &p_info[p_ptr->prace];

    /* Success */
    return (TRUE);
}

/*
 * Player house
 */
static bool get_player_house(void)
{
    int house;

    house = ui_birth_choose_house(p_ptr->prace, p_ptr->phouse);

    /* No selection? */
    if (house == UI_BIRTH_CHOICE_CANCELLED)
        return FALSE;

    /* If the house changed, wipe the dependent background state. */
    if (house != p_ptr->phouse)
    {
        int i;

        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
            p_ptr->stat_base[i] = 0;
    }

    p_ptr->phouse = house;
    hp_ptr = &c_info[p_ptr->phouse];

    return TRUE;
}

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race, and house, and
 * modify options (including the birth options).
 */
static bool player_birth_aux_1(void)
{
    int i, j;

    int phase = 1;

    while (phase <= 2)
    {
        clear_question();

        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race())
            {
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }

        if (phase == 2)
        {
            /* Choose the player's house */
            if (!get_player_house())
            {
                phase--;
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }
    }

    /* Clear the base values of the skills */
    for (i = 0; i < A_MAX; i++)
        p_ptr->skill_base[i] = 0;

    /* Clear the abilities */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = FALSE;
            p_ptr->active_ability[i][j] = FALSE;
        }
    }

    /* Set adult options from birth options */
    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
    {
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];
    }

    /* Reset score options from cheat options */
    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
    {
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];
    }

    // Set a default value for hitpoint warning / delay factor unless this is an
    // old game file
    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->hitpoint_warn = 3;
        op_ptr->delay_factor = 5;
    }

    /* Clear the special item knowledge flags. */
    for (i = 0; i < z_info->e_max; i++)
    {
        e_info[i].aware = FALSE;
    }

    /* Clear */
    Term_clear();

    /* Done */
    return (TRUE);
}

/*
 * Helper function for 'player_birth()'.
 */
static bool player_birth_aux_2(void)
{
    int i;

    int stat = 0;

    int stats[A_MAX];

    int cost;

    char ch;

    /* Initialize stats */
    for (i = 0; i < A_MAX; i++)
    {
        /* Initial stats */
        stats[i] = p_ptr->stat_base[i];
    }

    /* Determine experience and things */
    get_extra();

    /* Interact */
    while (1)
    {
        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + hp_ptr->h_adj[i];

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += ui_birth_stat_cost_for_value(stats[i]);
        }

        /* Restrict cost */
        if (cost > ui_birth_stat_budget())
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        /* Display the current semantic character sheet. */
        ui_character_publish_sheet();
        ui_character_render_sheet();
        ui_birth_publish_stats_screen(stats, stat);
        ui_birth_render_stats_screen();

        /* Get key */
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        /* Quit */
        if ((ch == 'Q') || (ch == 'q'))
            quit(NULL);

        switch (ui_input_parse_adjust_key(ch))
        {
        case UI_INPUT_ADJUST_ACTION_CANCEL:
            ui_birth_clear_stats_screen();
            ui_character_clear_sheet();
            return (FALSE);

        case UI_INPUT_ADJUST_ACTION_CONFIRM:
            ui_birth_clear_stats_screen();
            ui_character_clear_sheet();
            return (TRUE);

        case UI_INPUT_ADJUST_ACTION_PREV:
            stat = (stat + A_MAX - 1) % A_MAX;
            break;

        case UI_INPUT_ADJUST_ACTION_NEXT:
            stat = (stat + 1) % A_MAX;
            break;

        case UI_INPUT_ADJUST_ACTION_DECREASE:
            if (stats[stat] > 0)
                stats[stat]--;
            break;

        case UI_INPUT_ADJUST_ACTION_INCREASE:
            stats[stat]++;
            break;

        case UI_INPUT_ADJUST_ACTION_NONE:
        default:
            break;
        }
    }
}

/*
 * Increase your skills by spending experience points
 */
bool gain_skills(void)
{
    int i;
    int skill = 0;
    int old_base[S_MAX];
    int skill_gain[S_MAX];
    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;
    char ch;
    bool accepted;

    // hack global variable
    skill_gain_in_progress = TRUE;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        // reset the total cost
        total_cost = 0;

        /* Process skills */
        for (i = 0; i < S_MAX; i++)
        {
            /* Total cost */
            total_cost += ui_character_skill_gain_cost(old_base[i], skill_gain[i]);
        }

        // set the new experience pool total
        p_ptr->new_exp = old_new_exp - total_cost;

        /* Restrict cost */
        if (p_ptr->new_exp < 0)
        {
            /* Warning */
            bell("Excessive skills!");

            /* Reduce stat */
            skill_gain[skill]--;

            /* Recompute costs */
            continue;
        }

        /* Calculate the bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Set the redraw flag for everything */
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        /* update the skills */
        for (i = 0; i < S_MAX; i++)
        {
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        /* Update stuff */
        update_stuff();
        ui_character_publish_sheet();
        ui_character_publish_skill_editor(
            old_base, skill_gain, skill, p_ptr->new_exp);
        ui_character_render_sheet();

        /* Get key */
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        /* Quit (only if haven't begun game yet) */
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0))
            quit(NULL);

        switch (ui_input_parse_adjust_key(ch))
        {
        case UI_INPUT_ADJUST_ACTION_CONFIRM:
            accepted = TRUE;
            break;

        case UI_INPUT_ADJUST_ACTION_CANCEL:
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++)
                p_ptr->skill_base[i] = old_base[i];
            ui_character_clear_skill_editor();
            ui_character_clear_sheet();
            accepted = FALSE;
            break;

        case UI_INPUT_ADJUST_ACTION_PREV:
            skill = (skill + S_MAX - 1) % S_MAX;
            continue;

        case UI_INPUT_ADJUST_ACTION_NEXT:
            skill = (skill + 1) % S_MAX;
            continue;

        case UI_INPUT_ADJUST_ACTION_DECREASE:
            if (skill_gain[skill] > 0)
                skill_gain[skill]--;
            continue;

        case UI_INPUT_ADJUST_ACTION_INCREASE:
            skill_gain[skill]++;
            continue;

        case UI_INPUT_ADJUST_ACTION_NONE:
        default:
            continue;
        }

        break;
    }

    // reset hack global variable
    skill_gain_in_progress = FALSE;
    ui_character_clear_skill_editor();
    ui_character_clear_sheet();

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    /* Done */
    return (accepted);
}

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static bool player_birth_aux(void)
{
    /* Ask questions */
    if (!player_birth_aux_1())
        return (FALSE);

    /* Point-based stats */
    if (!player_birth_aux_2())
        return (FALSE);

    /* Point-based skills */
    if (!gain_skills())
        return (FALSE);

    /* Roll for history */
    if (!get_history())
        return (FALSE);

    /* Roll for age/height/weight */
    if (!get_ahw())
        return (FALSE);

    /* Get a name, prepare savefile */
    if (!get_name())
        return (FALSE);

    // Reset the number of artefacts
    p_ptr->artefacts = 0;

    /* Accept */
    return (TRUE);
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
void player_birth(void)
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    /* Create a new character */
    while (1)
    {
        /* Wipe the player */
        player_wipe();

        /* Roll up a new character */
        if (player_birth_aux())
            break;
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    strnfmt(month, sizeof(month), "%.2s", raw_date + 5);
    atomonth(atoi(month), month, sizeof(month));

    if (*(raw_date + 7) == '0')
        strnfmt(clean_date, sizeof(clean_date), "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        strnfmt(clean_date, sizeof(clean_date), "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    my_strcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    my_strcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    my_strcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC, 0);
    message_add("  ", MSG_GENERIC, 0);
    message_add("====================", MSG_GENERIC, 0);
    message_add("  ", MSG_GENERIC, 0);
    message_add(" ", MSG_GENERIC, 0);

    /* Hack -- outfit the player */
    player_outfit();
}
