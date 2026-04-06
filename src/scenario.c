/* File: scenario.c */

/*
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Human-readable quest scenario loader, generator, and save-to-scenario
 * exporter.
 */

#include <limits.h>

#include "angband.h"
#include "ui-birth.h"
#include "ui-character.h"

#define SCENARIO_VERSION 1

#define SCENARIO_MAX_ROWS MAX_DUNGEON_HGT
#define SCENARIO_MAX_COLS MAX_DUNGEON_WID
#define SCENARIO_MAX_LEVELS 8
#define SCENARIO_MAX_ITEMS 64
#define SCENARIO_MAX_TERRAIN_LEGENDS 96
#define SCENARIO_MAX_LEVEL_ENTITIES 512
#define SCENARIO_MAX_GRID_TAGS 128
#define SCENARIO_MAX_TEXTS 64
#define SCENARIO_MAX_TEXT_LEN 4096
#define SCENARIO_MAX_RULES 128
#define SCENARIO_MAX_RULE_TERMS 8
#define SCENARIO_MAX_RULE_ACTIONS 8
#define SCENARIO_MAX_NAME_LEN 32
#define SCENARIO_MAX_FLAGS 32
#define SCENARIO_MAX_OATHS 8
#define SCENARIO_MAX_OATH_TEXT_LEN 160

#define SCN_FLAG_LIT 0x01
#define SCN_LEVEL_FLAG_GOOD_ITEM 0x01

#define SCN_SLOT_PACK 255

#define SCN_TERRAIN_FEATURE 1
#define SCN_TERRAIN_PLACE 2
#define SCN_TERRAIN_FLOOR 3

#define SCN_TERRAIN_PLACE_CLOSED_DOOR 1
#define SCN_TERRAIN_PLACE_TRAP 2
#define SCN_TERRAIN_PLACE_FORGE 3

#define SCN_LEVEL_ENTITY_PLAYER 1
#define SCN_LEVEL_ENTITY_OBJECT 2
#define SCN_LEVEL_ENTITY_MONSTER 3
#define SCN_LEVEL_ENTITY_GEN_OBJECT 4
#define SCN_LEVEL_ENTITY_GEN_MONSTER 5

#define SCN_EVENT_SEE_MONSTER 1
#define SCN_EVENT_USE_EXIT 2
#define SCN_EVENT_DEATH 3
#define SCN_EVENT_ENTER_LEVEL 4
#define SCN_EVENT_USE_UP_EXIT 5
#define SCN_EVENT_USE_DOWN_EXIT 6
#define SCN_EVENT_ESCAPE 7
#define SCN_EVENT_START_SONG 8
#define SCN_EVENT_ATTACK_MONSTER 9
#define SCN_EVENT_DAMAGE_MONSTER 10

#define SCN_ACTION_SET_FLAG 1
#define SCN_ACTION_DISPLAY_TEXT 2
#define SCN_ACTION_END_GAME 3
#define SCN_ACTION_CLEAR_FLAG 4
#define SCN_ACTION_DISPLAY_MESSAGE 5
#define SCN_ACTION_CONFIRM_TEXT 6
#define SCN_ACTION_DENY_ACTION 7
#define SCN_ACTION_SET_NEW_DEPTH 8
#define SCN_ACTION_CREATE_STAIRS 9
#define SCN_ACTION_ADD_NOTE 10
#define SCN_ACTION_BREAK_OATH 11

#define SCN_TERM_FLAG 1
#define SCN_TERM_DEPTH 2
#define SCN_TERM_NEW_DEPTH 3
#define SCN_TERM_HAS_SILMARIL 4
#define SCN_TERM_MONSTER_SLAIN 5
#define SCN_TERM_HAS_ITEM 6
#define SCN_TERM_CHOSEN_OATH 7
#define SCN_TERM_OATH_BROKEN 8
#define SCN_TERM_MONSTER_IS 9
#define SCN_TERM_MONSTER_VISIBLE 10

#define SCN_MONSTER_IS_MAN -1
#define SCN_MONSTER_IS_ELF -2

#define SCN_NOTE_TEXT_MAGIC 0x53430000UL
#define SCN_NOTE_TEXT_MASK 0xFFFF0000UL
#define SCN_NOTE_TEXT_INDEX_MASK 0x0000FFFFUL

#define SCENARIO_CAVE_INFO_FLAGS                                              \
    (CAVE_MARK | CAVE_GLOW | CAVE_ICKY | CAVE_ROOM | CAVE_G_VAULT | CAVE_HIDDEN)

typedef struct scenario_object_spec scenario_object_spec;
typedef struct scenario_monster_spec scenario_monster_spec;
typedef struct scenario_terrain_legend scenario_terrain_legend;
typedef struct scenario_grid_tag scenario_grid_tag;
typedef struct scenario_map_entity scenario_map_entity;
typedef struct scenario_map scenario_map;
typedef struct scenario_text_block scenario_text_block;
typedef struct scenario_oath scenario_oath;
typedef struct scenario_rule_term scenario_rule_term;
typedef struct scenario_rule_action scenario_rule_action;
typedef struct scenario_rule scenario_rule;
typedef struct scenario_event_context scenario_event_context;
typedef struct scenario_event_result scenario_event_result;
typedef struct scenario_rule_effects scenario_rule_effects;
typedef struct scenario_state scenario_state;

struct scenario_object_spec
{
    byte slot;
    s16b k_idx;
    char tag[SCENARIO_MAX_NAME_LEN];
    bool tag_set;

    byte number;
    bool number_set;

    s16b pval;
    bool pval_set;

    byte discount;
    bool discount_set;

    s16b weight;
    bool weight_set;

    byte name1;
    bool name1_set;

    byte name2;
    bool name2_set;

    s16b timeout;
    bool timeout_set;

    s16b att;
    bool att_set;

    s16b evn;
    bool evn_set;

    byte dd;
    bool dd_set;

    byte ds;
    bool ds_set;

    byte pd;
    bool pd_set;

    byte ps;
    bool ps_set;

    byte pickup;
    bool pickup_set;

    u32b ident;
    bool ident_set;

    byte marked;
    bool marked_set;

    byte xtra1;
    bool xtra1_set;

    byte abilities;
    bool abilities_set;

    byte skilltype[8];
    byte abilitynum[8];
    bool ability_slot_set[8];

    s32b unused1;
    bool unused1_set;

    s32b unused2;
    bool unused2_set;

    s32b unused3;
    bool unused3_set;

    s32b unused4;
    bool unused4_set;

    bool known;
    bool known_set;

    bool aware;
    bool aware_set;

    bool tried;
    bool tried_set;

    bool everseen;
    bool everseen_set;
};

struct scenario_monster_spec
{
    s16b r_idx;
    char tag[SCENARIO_MAX_NAME_LEN];
    bool tag_set;

    s16b image_r_idx;
    bool image_r_idx_set;

    s16b hp;
    bool hp_set;

    s16b maxhp;
    bool maxhp_set;

    s16b alertness;
    bool alertness_set;

    byte skip_next_turn;
    bool skip_next_turn_set;

    byte mspeed;
    bool mspeed_set;

    byte energy;
    bool energy_set;

    byte stunned;
    bool stunned_set;

    byte confused;
    bool confused_set;

    s16b hasted;
    bool hasted_set;

    s16b slowed;
    bool slowed_set;

    byte stance;
    bool stance_set;

    s16b morale;
    bool morale_set;

    s16b tmp_morale;
    bool tmp_morale_set;

    byte noise;
    bool noise_set;

    byte encountered;
    bool encountered_set;

    byte target_y;
    bool target_y_set;

    byte target_x;
    bool target_x_set;

    s16b wandering_idx;
    bool wandering_idx_set;

    byte wandering_dist;
    bool wandering_dist_set;

    byte mana;
    bool mana_set;

    byte song;
    bool song_set;

    byte skip_this_turn;
    bool skip_this_turn_set;

    s16b consecutive_attacks;
    bool consecutive_attacks_set;

    s16b turns_stationary;
    bool turns_stationary_set;

    u32b mflag;
    bool mflag_set;

    bool home_wander;
    bool home_wander_set;

    bool no_open_doors;
    bool no_open_doors_set;

    byte previous_action[ACTION_MAX];
    bool previous_action_set[ACTION_MAX];

    bool sleep;
    bool sleep_set;
};

struct scenario_map_entity
{
    byte kind;
    int y;
    int x;

    scenario_object_spec object;
    scenario_monster_spec monster;

    bool relative_depth;
    s16b min_depth;
    s16b max_depth;

    bool good;
    byte drop_type;
    byte chance;
};

struct scenario_terrain_legend
{
    char symbol;
    byte type;
    byte feat;
    byte place;
    byte chance;
    u16b info;

    bool entity_set;
    scenario_map_entity entity;
};

struct scenario_grid_tag
{
    int y;
    int x;
    char tag[SCENARIO_MAX_NAME_LEN];
};

struct scenario_map
{
    char name[SCENARIO_MAX_NAME_LEN];

    bool depth_set;
    s16b depth;
    byte flags;

    bool map_size_set;
    int map_height;
    int map_width;

    bool origin_set;
    int origin_y;
    int origin_x;

    int width;
    int height;
    int terrain_rows;

    bool player_pos_set;
    int player_y;
    int player_x;

    int entity_count;
    scenario_map_entity entities[SCENARIO_MAX_LEVEL_ENTITIES];

    int terrain_legend_count;
    scenario_terrain_legend terrain_legends[SCENARIO_MAX_TERRAIN_LEGENDS];

    int grid_tag_count;
    scenario_grid_tag grid_tags[SCENARIO_MAX_GRID_TAGS];

    char terrain[SCENARIO_MAX_ROWS][SCENARIO_MAX_COLS + 1];
};

struct scenario_text_block
{
    char name[SCENARIO_MAX_NAME_LEN];
    char text[SCENARIO_MAX_TEXT_LEN];
};

struct scenario_oath
{
    char name[SCENARIO_MAX_NAME_LEN];
    char vow[SCENARIO_MAX_OATH_TEXT_LEN];
    bool vow_set;
    char restriction[SCENARIO_MAX_OATH_TEXT_LEN];
    bool restriction_set;
    byte reward_stat;
    s16b reward_amount;
    bool reward_stat_set;
};

struct scenario_rule_term
{
    byte kind;
    byte flag_idx;
    bool expected;
    s16b value;
    s16b extra;
};

struct scenario_rule_action
{
    byte kind;
    byte flag_idx;
    byte text_idx;
    s16b value;
};

struct scenario_rule
{
    byte event;
    bool tag_set;
    char tag[SCENARIO_MAX_NAME_LEN];

    int when_count;
    scenario_rule_term when[SCENARIO_MAX_RULE_TERMS];

    int action_count;
    scenario_rule_action actions[SCENARIO_MAX_RULE_ACTIONS];
};

struct scenario_event_context
{
    byte event;
    int y;
    int x;
    int depth;
    int new_depth;
    int monster_idx;
    bool monster_visible;
    bool has_silmaril;
};

struct scenario_event_result
{
    bool end_game;
    bool deny_action;
    int prompt_text_idx;
    bool new_depth_set;
    s16b new_depth;
    bool create_stair_set;
    s16b create_stair;
    int text_count;
    byte text_idx[SCENARIO_MAX_RULES];
    int message_count;
    byte message_idx[SCENARIO_MAX_RULES];
    int note_count;
    byte note_idx[SCENARIO_MAX_RULES];
};

struct scenario_rule_effects
{
    bool end_game;
    bool deny_action;
    int prompt_text_idx;
    bool new_depth_set;
    s16b new_depth;
    bool create_stair_set;
    s16b create_stair;
    bool break_oath;
    int text_count;
    byte text_idx[SCENARIO_MAX_RULE_ACTIONS];
    int message_count;
    byte message_idx[SCENARIO_MAX_RULE_ACTIONS];
    int note_count;
    byte note_idx[SCENARIO_MAX_RULE_ACTIONS];
    int set_flag_count;
    byte set_flag_idx[SCENARIO_MAX_RULE_ACTIONS];
    int clear_flag_count;
    byte clear_flag_idx[SCENARIO_MAX_RULE_ACTIONS];
};

struct scenario_state
{
    bool loaded;

    char source[1024];

    byte flags;

    byte prace;
    bool prace_set;

    byte phouse;
    bool phouse_set;

    char full_name[32];
    bool full_name_set;

    char history[250];

    s16b age;
    bool age_set;

    s16b ht;
    bool ht_set;

    s16b wt;
    bool wt_set;

    s16b depth;
    bool depth_set;

    s16b max_depth;
    bool max_depth_set;

    s32b exp;
    bool exp_set;

    s32b new_exp;
    bool new_exp_set;

    s16b food;
    bool food_set;

    s16b chp;
    bool chp_set;

    s16b csp;
    bool csp_set;

    s16b stat_base[A_MAX];
    bool stat_set[A_MAX];

    s16b skill_base[S_MAX];
    bool skill_set[S_MAX];

    byte innate_ability[S_MAX][ABILITIES_MAX];
    byte active_ability[S_MAX][ABILITIES_MAX];

    int item_count;
    scenario_object_spec items[SCENARIO_MAX_ITEMS];

    int text_count;
    scenario_text_block texts[SCENARIO_MAX_TEXTS];

    int oath_count;
    scenario_oath oaths[SCENARIO_MAX_OATHS];

    int rule_count;
    scenario_rule rules[SCENARIO_MAX_RULES];

    int flag_count;
    char flag_names[SCENARIO_MAX_FLAGS][SCENARIO_MAX_NAME_LEN];

    int level_count;
    scenario_map levels[SCENARIO_MAX_LEVELS];
};

static scenario_state scenario;
static char scenario_monster_tags[MAX_MONSTERS][SCENARIO_MAX_NAME_LEN];
static int scenario_active_map_idx = -1;
static int scenario_truce_flag_idx = -1;
static int scenario_escape_flag_idx = -1;
static int scenario_error_line = 0;
static char scenario_error[160];
static int scenario_warning_line = 0;
static int scenario_warning_count = 0;
static char scenario_warning[160];

static int scenario_default_start_exp(void);
static int scenario_birth_start_exp(void);
static int scenario_birth_stat_cost_offset(void);
static bool scenario_grid_has_tag(int y, int x, cptr tag);
static bool scenario_place_map_entity(const scenario_map_entity* entity);
static bool scenario_parse_depth_span(
    cptr token, bool* relative, s16b* min_depth, s16b* max_depth, int line);
static void scenario_show_text_block(int text_idx);
static void scenario_show_message_block(int text_idx);
static void scenario_add_note_block(int text_idx);

static bool scenario_fail(int line, cptr message)
{
    scenario_error_line = line;
    my_strcpy(scenario_error, message, sizeof(scenario_error));
    return (FALSE);
}

/*
 * Record the first non-fatal parse warning so unknown directives can be
 * skipped while still leaving a useful warning summary for the player.
 */
static void scenario_warn(int line, cptr message)
{
    scenario_warning_count++;

    if (!scenario_warning[0])
    {
        scenario_warning_line = line;
        my_strcpy(scenario_warning, message, sizeof(scenario_warning));
    }
}

static void scenario_rstrip(char* buf)
{
    size_t n = strlen(buf);

    while (n && ((buf[n - 1] == '\n') || (buf[n - 1] == '\r')
                    || (buf[n - 1] == ' ') || (buf[n - 1] == '\t')))
    {
        buf[--n] = '\0';
    }
}

static char* scenario_trim(char* s)
{
    char* end;

    while ((*s == ' ') || (*s == '\t'))
        s++;

    end = s + strlen(s);
    while ((end > s) && ((end[-1] == ' ') || (end[-1] == '\t')))
        *--end = '\0';

    return (s);
}

static int scenario_split(char* buf, char** tokens, int max_tokens)
{
    int count = 0;
    char* s = buf;

    tokens[count++] = s;

    while ((count < max_tokens) && (s = strchr(s, ':')))
    {
        *s++ = '\0';
        tokens[count++] = s;
    }

    return (count);
}

static bool scenario_parse_int(cptr token, long* value)
{
    char* end = NULL;
    long parsed;

    if (!token || !token[0])
        return (FALSE);

    parsed = strtol(token, &end, 0);
    if (!end || *scenario_trim(end))
        return (FALSE);

    *value = parsed;
    return (TRUE);
}

static bool scenario_parse_bool(cptr token, bool* value)
{
    if (!token || !token[0])
        return (FALSE);

    if (!my_stricmp(token, "1") || !my_stricmp(token, "yes")
        || !my_stricmp(token, "true") || !my_stricmp(token, "on"))
    {
        *value = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(token, "0") || !my_stricmp(token, "no")
        || !my_stricmp(token, "false") || !my_stricmp(token, "off"))
    {
        *value = FALSE;
        return (TRUE);
    }

    return (FALSE);
}

static int scenario_parse_indexed_key(cptr key, cptr prefix, int max_value)
{
    size_t prefix_len = strlen(prefix);
    long index;

    if (my_strnicmp(key, prefix, prefix_len))
        return (-1);

    if (!scenario_parse_int(key + prefix_len, &index))
        return (-1);

    if ((index < 0) || (index >= max_value))
        return (-1);

    return ((int)index);
}

static int scenario_lookup_numeric_id(cptr token)
{
    long value;

    if (!token || !token[0])
        return (-1);

    if (token[0] == '#')
        token++;

    if (!scenario_parse_int(token, &value))
        return (-1);

    if (value < 0)
        return (-1);

    return ((int)value);
}

static int scenario_lookup_stat(cptr token)
{
    if (!my_stricmp(token, "STR") || !my_stricmp(token, "STRENGTH"))
        return (A_STR);
    if (!my_stricmp(token, "DEX") || !my_stricmp(token, "DEXTERITY"))
        return (A_DEX);
    if (!my_stricmp(token, "CON") || !my_stricmp(token, "CONSTITUTION"))
        return (A_CON);
    if (!my_stricmp(token, "GRA") || !my_stricmp(token, "GRACE"))
        return (A_GRA);

    return (-1);
}

static int scenario_lookup_skill(cptr token)
{
    if (!my_stricmp(token, "MEL") || !my_stricmp(token, "MELEE"))
        return (S_MEL);
    if (!my_stricmp(token, "ARC") || !my_stricmp(token, "ARCHERY"))
        return (S_ARC);
    if (!my_stricmp(token, "EVN") || !my_stricmp(token, "EVASION"))
        return (S_EVN);
    if (!my_stricmp(token, "STL") || !my_stricmp(token, "STEALTH"))
        return (S_STL);
    if (!my_stricmp(token, "PER") || !my_stricmp(token, "PERCEPTION"))
        return (S_PER);
    if (!my_stricmp(token, "WIL") || !my_stricmp(token, "WILL"))
        return (S_WIL);
    if (!my_stricmp(token, "SMT") || !my_stricmp(token, "SMITHING"))
        return (S_SMT);
    if (!my_stricmp(token, "SNG") || !my_stricmp(token, "SONG"))
        return (S_SNG);

    return (-1);
}

/* Resolve an ability token within a specific skill by name or numeric id. */
static int scenario_lookup_ability(int skilltype, cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);

    if ((skilltype < 0) || (skilltype >= S_MAX))
        return (-1);

    if (id >= 0)
    {
        for (i = 0; i < z_info->b_max; i++)
        {
            ability_type* b_ptr = &b_info[i];

            if (!b_ptr->name)
                continue;
            if (b_ptr->skilltype != skilltype)
                continue;
            if (b_ptr->abilitynum == id)
                return (id);
        }

        return (-1);
    }

    for (i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;
        if (b_ptr->skilltype != skilltype)
            continue;
        if (!my_stricmp(token, b_name + b_ptr->name))
            return (b_ptr->abilitynum);
    }

    return (-1);
}

static int scenario_lookup_race(cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);

    if (id >= 0)
    {
        if (id < z_info->p_max)
            return (id);
        return (-1);
    }

    for (i = 0; i < z_info->p_max; i++)
    {
        if (!my_stricmp(token, p_name + p_info[i].name))
            return (i);
    }

    return (-1);
}

static int scenario_lookup_house(cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);

    if (id >= 0)
    {
        if (id < z_info->c_max)
            return (id);
        return (-1);
    }

    for (i = 0; i < z_info->c_max; i++)
    {
        if (!my_stricmp(token, c_name + c_info[i].name))
            return (i);

        if (c_info[i].alt_name
            && !my_stricmp(token, c_name + c_info[i].alt_name))
            return (i);

        if (c_info[i].short_name
            && !my_stricmp(token, c_name + c_info[i].short_name))
            return (i);
    }

    return (-1);
}

static int scenario_lookup_monster(cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);

    if (id > 0)
    {
        if (id < z_info->r_max)
            return (id);
        return (0);
    }

    for (i = 1; i < z_info->r_max; i++)
    {
        if (!my_stricmp(token, r_name + r_info[i].name))
            return (i);
    }

    return (0);
}

/* Resolve one monster selector token for monster-related rule predicates. */
static int scenario_lookup_monster_selector(cptr token)
{
    int r_idx = scenario_lookup_monster(token);

    if (r_idx > 0)
        return (r_idx);
    if (!my_stricmp(token, "Man"))
        return (SCN_MONSTER_IS_MAN);
    if (!my_stricmp(token, "Elf"))
        return (SCN_MONSTER_IS_ELF);

    return (0);
}

static int scenario_lookup_object(cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);
    int match = 0;
    int matches = 0;
    char buf[80];

    if (id > 0)
    {
        if (id < z_info->k_max)
            return (id);
        return (0);
    }

    for (i = 1; i < z_info->k_max; i++)
    {
        strip_name(buf, i);
        if (!my_stricmp(token, buf))
        {
            match = i;
            matches++;
        }
    }

    if (matches == 1)
        return (match);

    return (0);
}

static int scenario_lookup_feature(cptr token)
{
    int i;
    int id = scenario_lookup_numeric_id(token);
    char token_buf[128];

    if (id >= 0)
    {
        if (id < z_info->f_max)
            return (id);
        return (-1);
    }

    my_strcpy(token_buf, token, sizeof(token_buf));
    token = scenario_trim(token_buf);

    for (i = 0; i < z_info->f_max; i++)
    {
        char feature_buf[128];

        my_strcpy(feature_buf, f_name + f_info[i].name, sizeof(feature_buf));
        if (!my_stricmp(token, scenario_trim(feature_buf)))
            return (i);
    }

    return (-1);
}

/* Intern one scenario-local oath name so rules can reference it by index. */
static int scenario_intern_oath(cptr token, int line)
{
    int i;
    char local[64];
    cptr name;

    if (!token || !token[0] || !my_stricmp(token, "None"))
        return (0);

    my_strcpy(local, token, sizeof(local));
    name = scenario_trim(local);
    if (!name[0])
        return (scenario_fail(line, "Missing oath name."));

    for (i = 0; i < scenario.oath_count; i++)
    {
        if (!my_stricmp(name, scenario.oaths[i].name))
            return (i + 1);
    }

    if (scenario.oath_count >= SCENARIO_MAX_OATHS)
    {
        scenario_fail(line, "Too many oath definitions.");
        return (-1);
    }

    my_strcpy(scenario.oaths[scenario.oath_count].name, name,
        sizeof(scenario.oaths[scenario.oath_count].name));
    scenario.oath_count++;
    return (scenario.oath_count);
}

/* Return whether the player has slain at least one monster of one race. */
static bool scenario_monster_has_been_slain(int r_idx)
{
    if ((r_idx <= 0) || (r_idx >= z_info->r_max))
        return (FALSE);

    return (l_list[r_idx].pkills > 0);
}

/* Return whether one monster context matches a parsed monster selector. */
static bool scenario_monster_matches(
    const scenario_event_context* context, int selector)
{
    monster_type* m_ptr;
    monster_race* r_ptr;

    if (!context || (context->monster_idx <= 0)
        || (context->monster_idx >= mon_max))
    {
        return (FALSE);
    }

    m_ptr = &mon_list[context->monster_idx];
    if (!m_ptr->r_idx)
        return (FALSE);

    r_ptr = &r_info[m_ptr->r_idx];
    if (selector > 0)
        return (m_ptr->r_idx == selector);
    if (selector == SCN_MONSTER_IS_MAN)
        return ((r_ptr->flags3 & RF3_MAN) != 0);
    if (selector == SCN_MONSTER_IS_ELF)
        return ((r_ptr->flags3 & RF3_ELF) != 0);

    return (FALSE);
}

/* Parse one CREATE_STAIRS value into a deferred return-stair feature. */
static bool scenario_parse_create_stairs_value(cptr token, s16b* feat)
{
    int parsed_feat;
    char local[128];

    if (!token || !feat)
        return (FALSE);

    my_strcpy(local, token, sizeof(local));
    token = scenario_trim(local);

    if (!my_stricmp(token, "none"))
    {
        *feat = 0;
        return (TRUE);
    }
    if (!my_stricmp(token, "less"))
    {
        *feat = FEAT_LESS;
        return (TRUE);
    }
    if (!my_stricmp(token, "more"))
    {
        *feat = FEAT_MORE;
        return (TRUE);
    }
    if (!my_stricmp(token, "less_shaft"))
    {
        *feat = FEAT_LESS_SHAFT;
        return (TRUE);
    }
    if (!my_stricmp(token, "more_shaft"))
    {
        *feat = FEAT_MORE_SHAFT;
        return (TRUE);
    }

    parsed_feat = scenario_lookup_feature(token);
    if ((parsed_feat == FEAT_LESS) || (parsed_feat == FEAT_MORE)
        || (parsed_feat == FEAT_LESS_SHAFT)
        || (parsed_feat == FEAT_MORE_SHAFT))
    {
        *feat = (s16b)parsed_feat;
        return (TRUE);
    }

    return (FALSE);
}

/* Parse the arguments of one has_item(object[,amount]) rule term. */
static bool scenario_parse_has_item_args(
    cptr token, s16b* k_idx, s16b* amount, int line)
{
    char local[128];
    char* open;
    char* close;
    char* comma;
    long parsed_amount = 1;
    int parsed_kind;

    if (!token || !k_idx || !amount)
        return (scenario_fail(line, "Invalid has_item condition."));

    my_strcpy(local, token, sizeof(local));
    open = strchr(local, '(');
    close = strrchr(local, ')');

    if (!open || !close || (close < open))
        return (scenario_fail(line, "Malformed has_item condition."));

    *close = '\0';
    open++;
    comma = strrchr(open, ',');
    if (comma)
    {
        *comma++ = '\0';
        comma = scenario_trim(comma);
        if (!scenario_parse_int(comma, &parsed_amount) || (parsed_amount <= 0))
            return (scenario_fail(line, "Invalid has_item amount."));
    }

    parsed_kind = scenario_lookup_object(scenario_trim(open));
    if (parsed_kind <= 0)
        return (scenario_fail(line, "Unknown object in has_item condition."));

    *k_idx = (s16b)parsed_kind;
    *amount = (s16b)parsed_amount;
    return (TRUE);
}

/* Count how many carried or equipped items match one object kind. */
static int scenario_count_item_kind(int k_idx)
{
    int i;
    int count = 0;

    if (k_idx <= 0)
        return (0);

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx || (o_ptr->k_idx != k_idx))
            continue;

        count += (o_ptr->number > 0) ? o_ptr->number : 1;
    }

    return (count);
}

static int scenario_lookup_slot(cptr token)
{
    int numeric = scenario_lookup_numeric_id(token);

    if ((numeric >= 0) && (numeric < INVEN_TOTAL))
        return (numeric);

    if (!my_stricmp(token, "PACK"))
        return (SCN_SLOT_PACK);
    if (!my_stricmp(token, "WIELD"))
        return (INVEN_WIELD);
    if (!my_stricmp(token, "BOW"))
        return (INVEN_BOW);
    if (!my_stricmp(token, "LEFT"))
        return (INVEN_LEFT);
    if (!my_stricmp(token, "RIGHT"))
        return (INVEN_RIGHT);
    if (!my_stricmp(token, "NECK"))
        return (INVEN_NECK);
    if (!my_stricmp(token, "LITE"))
        return (INVEN_LITE);
    if (!my_stricmp(token, "BODY"))
        return (INVEN_BODY);
    if (!my_stricmp(token, "OUTER"))
        return (INVEN_OUTER);
    if (!my_stricmp(token, "ARM"))
        return (INVEN_ARM);
    if (!my_stricmp(token, "HEAD"))
        return (INVEN_HEAD);
    if (!my_stricmp(token, "HANDS"))
        return (INVEN_HANDS);
    if (!my_stricmp(token, "FEET"))
        return (INVEN_FEET);
    if (!my_stricmp(token, "QUIVER1"))
        return (INVEN_QUIVER1);
    if (!my_stricmp(token, "QUIVER2"))
        return (INVEN_QUIVER2);

    return (-1);
}

/* Look up one saved scenario flag bit by name without creating it. */
static int scenario_find_flag(cptr token)
{
    int i;
    char local[SCENARIO_MAX_NAME_LEN];
    char trimmed[SCENARIO_MAX_NAME_LEN];

    if (!token || !token[0])
        return (-1);

    my_strcpy(local, token, sizeof(local));
    my_strcpy(trimmed, scenario_trim(local), sizeof(trimmed));
    if (!trimmed[0])
        return (-1);

    for (i = 0; i < scenario.flag_count; i++)
    {
        if (!my_stricmp(trimmed, scenario.flag_names[i]))
            return (i);
    }

    return (-1);
}

/* Look up or create one saved scenario flag bit by name. */
static int scenario_intern_flag(cptr token, int line)
{
    int i;
    char local[SCENARIO_MAX_NAME_LEN];
    char trimmed[SCENARIO_MAX_NAME_LEN];

    if (!token || !token[0])
    {
        scenario_fail(line, "Missing scenario flag name.");
        return (-1);
    }

    my_strcpy(local, token, sizeof(local));
    my_strcpy(trimmed, scenario_trim(local), sizeof(trimmed));
    if (!trimmed[0])
    {
        scenario_fail(line, "Missing scenario flag name.");
        return (-1);
    }

    i = scenario_find_flag(trimmed);
    if (i >= 0)
        return (i);

    if (scenario.flag_count >= SCENARIO_MAX_FLAGS)
    {
        scenario_fail(line, "Too many scenario flags.");
        return (-1);
    }

    my_strcpy(
        scenario.flag_names[scenario.flag_count], trimmed, SCENARIO_MAX_NAME_LEN);
    scenario.flag_count++;
    return (scenario.flag_count - 1);
}

/* Read one scenario-owned quest flag from the saved player state bitset. */
static bool scenario_get_flag(int idx)
{
    u32b bits;

    if (!p_ptr || (idx < 0) || (idx >= SCENARIO_MAX_FLAGS))
        return (FALSE);

    bits = (u32b)p_ptr->unused3;
    return ((bits & (1UL << idx)) != 0);
}

/* Write one scenario-owned quest flag into the saved player state bitset. */
static void scenario_set_flag(int idx, bool value)
{
    u32b bits;

    if (!p_ptr || (idx < 0) || (idx >= SCENARIO_MAX_FLAGS))
        return;

    bits = (u32b)p_ptr->unused3;
    if (value)
        bits |= (1UL << idx);
    else
        bits &= ~(1UL << idx);

    p_ptr->unused3 = (s32b)bits;
}

/* Look up one named scenario text block without creating it. */
static int scenario_find_text(cptr token)
{
    int i;
    char local[SCENARIO_MAX_NAME_LEN];
    char trimmed[SCENARIO_MAX_NAME_LEN];

    if (!token || !token[0])
        return (-1);

    my_strcpy(local, token, sizeof(local));
    my_strcpy(trimmed, scenario_trim(local), sizeof(trimmed));
    if (!trimmed[0])
        return (-1);

    for (i = 0; i < scenario.text_count; i++)
    {
        if (!my_stricmp(trimmed, scenario.texts[i].name))
            return (i);
    }

    return (-1);
}

/* Encode one scenario text block id into a note object's spare save data. */
static void scenario_note_set_text_idx(object_type* o_ptr, int text_idx)
{
    u32b encoded;

    if (!o_ptr || (text_idx < 0) || (text_idx >= SCENARIO_MAX_TEXTS))
        return;

    encoded = SCN_NOTE_TEXT_MAGIC
        | (((u32b)text_idx + 1) & SCN_NOTE_TEXT_INDEX_MASK);
    o_ptr->unused4 = (s32b)encoded;
}

/* Recover the scenario text block id carried by one tagged note object. */
static int scenario_note_text_idx(const object_type* o_ptr)
{
    u32b encoded;
    int idx;

    if (!o_ptr || (o_ptr->tval != TV_NOTE))
        return (-1);

    encoded = (u32b)o_ptr->unused4;
    if ((encoded & SCN_NOTE_TEXT_MASK) != SCN_NOTE_TEXT_MAGIC)
        return (-1);

    idx = (int)(encoded & SCN_NOTE_TEXT_INDEX_MASK) - 1;
    if ((idx < 0) || (idx >= scenario.text_count))
        return (-1);

    return (idx);
}

/* Return the authored tag name associated with one tagged scenario note. */
static cptr scenario_note_tag(const object_type* o_ptr)
{
    int idx = scenario_note_text_idx(o_ptr);

    if (idx < 0)
        return (NULL);

    return (scenario.texts[idx].name);
}

/* Return the authored scenario text shown when reading one tagged note. */
cptr scenario_note_text(const object_type* o_ptr)
{
    int idx = scenario_note_text_idx(o_ptr);

    if (idx < 0)
        return (NULL);

    return (scenario.texts[idx].text);
}

/* Look up or create one named multiline scenario text block. */
static int scenario_intern_text(cptr token, int line)
{
    int idx;
    char local[SCENARIO_MAX_NAME_LEN];
    char trimmed[SCENARIO_MAX_NAME_LEN];

    if (!token || !token[0])
    {
        scenario_fail(line, "Missing scenario text name.");
        return (-1);
    }

    my_strcpy(local, token, sizeof(local));
    my_strcpy(trimmed, scenario_trim(local), sizeof(trimmed));
    if (!trimmed[0])
    {
        scenario_fail(line, "Missing scenario text name.");
        return (-1);
    }

    idx = scenario_find_text(trimmed);
    if (idx >= 0)
        return (idx);

    if (scenario.text_count >= SCENARIO_MAX_TEXTS)
    {
        scenario_fail(line, "Too many scenario text blocks.");
        return (-1);
    }

    my_strcpy(scenario.texts[scenario.text_count].name, trimmed,
        sizeof(scenario.texts[scenario.text_count].name));
    scenario.texts[scenario.text_count].text[0] = '\0';
    scenario.text_count++;
    return (scenario.text_count - 1);
}

/* Append one authored line to a named scenario text block. */
static bool scenario_append_text_line(cptr name_token, cptr text_line, int line)
{
    int idx = scenario_intern_text(name_token, line);
    scenario_text_block* text;

    if (idx < 0)
        return (FALSE);

    text = &scenario.texts[idx];
    if (text->text[0])
    {
        if (strlen(text->text) + 1 >= sizeof(text->text))
            return (scenario_fail(line, "Scenario text block is too long."));
        my_strcat(text->text, "\n", sizeof(text->text));
    }

    my_strcat(text->text, text_line ? text_line : "", sizeof(text->text));
    if (strlen(text->text) >= sizeof(text->text) - 1)
        return (scenario_fail(line, "Scenario text block is too long."));

    return (TRUE);
}

/* Parse one OATH:name:field:value directive into the pending oath catalog. */
static bool scenario_parse_oath(cptr token, int line)
{
    char local[512];
    char* first;
    char* second;
    char* name;
    char* field;
    char* value;
    char* parts[3];
    int count;
    int oath_idx;
    int stat_idx;
    long amount;
    scenario_oath* oath;

    if (!token || !token[0])
        return (scenario_fail(line, "Malformed oath directive."));

    my_strcpy(local, token, sizeof(local));
    first = strchr(local, ':');
    if (!first)
        return (scenario_fail(line, "Malformed oath directive."));
    *first++ = '\0';

    second = strchr(first, ':');
    if (!second)
        return (scenario_fail(line, "Malformed oath directive."));
    *second++ = '\0';

    name = scenario_trim(local);
    field = scenario_trim(first);
    value = scenario_trim(second);
    if (!name[0] || !field[0] || !value[0])
        return (scenario_fail(line, "Malformed oath directive."));

    oath_idx = scenario_intern_oath(name, line);
    if (oath_idx < 0)
        return (FALSE);
    if (oath_idx == 0)
        return (scenario_fail(line, "Oath name cannot be None."));

    oath = &scenario.oaths[oath_idx - 1];
    if (!my_stricmp(field, "VOW"))
    {
        my_strcpy(oath->vow, value, sizeof(oath->vow));
        oath->vow_set = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(field, "RESTRICTION"))
    {
        my_strcpy(
            oath->restriction, value, sizeof(oath->restriction));
        oath->restriction_set = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(field, "REWARD_STAT"))
    {
        count = scenario_split(value, parts, 3);
        if (count < 2)
            return (scenario_fail(line, "REWARD_STAT needs a stat and amount."));

        stat_idx = scenario_lookup_stat(scenario_trim(parts[0]));
        if ((stat_idx < 0)
            || !scenario_parse_int(scenario_trim(parts[1]), &amount)
            || (amount == 0))
        {
            return (scenario_fail(line, "Invalid oath stat reward."));
        }

        oath->reward_stat = (byte)stat_idx;
        oath->reward_amount = (s16b)amount;
        oath->reward_stat_set = TRUE;
        return (TRUE);
    }

    return (scenario_fail(line, "Unknown oath directive."));
}

/* Validate and activate the pending oath catalog for the loaded scenario. */
static bool scenario_activate_oaths(void)
{
    int i;

    oath_clear_catalog();

    for (i = 0; i < scenario.oath_count; i++)
    {
        scenario_oath* oath = &scenario.oaths[i];

        if (!oath->name[0] || !oath->vow_set || !oath->restriction_set
            || !oath->reward_stat_set)
        {
            return (
                scenario_fail(0, "Each defined oath needs vow, restriction, and reward."));
        }

        if (!oath_add_definition(oath->name, oath->vow, oath->restriction,
                oath->reward_stat, oath->reward_amount))
        {
            return (scenario_fail(0, "Failed to activate one oath definition."));
        }
    }

    return (TRUE);
}

/* Resolve one scenario rule event token. */
static int scenario_lookup_event(cptr token)
{
    if (!my_stricmp(token, "ENTER_LEVEL"))
        return (SCN_EVENT_ENTER_LEVEL);
    if (!my_stricmp(token, "SEE_MONSTER"))
        return (SCN_EVENT_SEE_MONSTER);
    if (!my_stricmp(token, "USE_EXIT"))
        return (SCN_EVENT_USE_EXIT);
    if (!my_stricmp(token, "USE_UP_EXIT"))
        return (SCN_EVENT_USE_UP_EXIT);
    if (!my_stricmp(token, "USE_DOWN_EXIT"))
        return (SCN_EVENT_USE_DOWN_EXIT);
    if (!my_stricmp(token, "ESCAPE"))
        return (SCN_EVENT_ESCAPE);
    if (!my_stricmp(token, "START_SONG"))
        return (SCN_EVENT_START_SONG);
    if (!my_stricmp(token, "ATTACK_MONSTER"))
        return (SCN_EVENT_ATTACK_MONSTER);
    if (!my_stricmp(token, "DAMAGE_MONSTER"))
        return (SCN_EVENT_DAMAGE_MONSTER);
    if (!my_stricmp(token, "DEATH"))
        return (SCN_EVENT_DEATH);

    return (0);
}

/* Parse one rule condition term from a when= list. */
static bool scenario_parse_rule_term(
    scenario_rule* rule, cptr token, int line)
{
    char local[256];
    char* trimmed;
    char* open;
    char* close;
    char* eq;
    long value;
    int flag_idx;
    scenario_rule_term* term;

    if (!token || !token[0])
        return (scenario_fail(line, "Invalid rule condition."));
    if (rule->when_count >= SCENARIO_MAX_RULE_TERMS)
        return (scenario_fail(line, "Too many rule conditions."));

    my_strcpy(local, token, sizeof(local));
    trimmed = scenario_trim(local);
    term = &rule->when[rule->when_count];
    WIPE(term, scenario_rule_term);
    term->expected = TRUE;

    if (trimmed[0] == '!')
    {
        term->expected = FALSE;
        trimmed = scenario_trim(trimmed + 1);
    }

    if (!trimmed[0])
        return (scenario_fail(line, "Invalid rule condition."));

    if (!my_strnicmp(trimmed, "has_item(", 9))
    {
        term->kind = SCN_TERM_HAS_ITEM;
        if (!scenario_parse_has_item_args(trimmed, &term->value, &term->extra, line))
            return (FALSE);

        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "has_silmaril(", 13))
    {
        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close != open + 1))
            return (scenario_fail(line, "Malformed has_silmaril() condition."));

        term->kind = SCN_TERM_HAS_SILMARIL;
        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "monster_slain(", 14))
    {
        int r_idx;

        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close <= open + 1))
            return (scenario_fail(line, "Malformed monster_slain() condition."));

        *close = '\0';
        r_idx = scenario_lookup_monster(scenario_trim(open + 1));
        if (r_idx <= 0)
            return (scenario_fail(line, "Unknown monster in monster_slain()."));

        term->kind = SCN_TERM_MONSTER_SLAIN;
        term->value = (s16b)r_idx;
        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "chosen_oath(", 12))
    {
        int oath_idx;

        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close <= open + 1))
            return (scenario_fail(line, "Malformed chosen_oath() condition."));

        *close = '\0';
        oath_idx = scenario_intern_oath(scenario_trim(open + 1), line);
        if (oath_idx < 0)
            return (FALSE);

        term->kind = SCN_TERM_CHOSEN_OATH;
        term->value = (s16b)oath_idx;
        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "oath_broken(", 12))
    {
        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close != open + 1))
            return (scenario_fail(line, "Malformed oath_broken() condition."));

        term->kind = SCN_TERM_OATH_BROKEN;
        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "monster_is(", 11))
    {
        int selector;

        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close <= open + 1))
            return (scenario_fail(line, "Malformed monster_is() condition."));

        *close = '\0';
        selector = scenario_lookup_monster_selector(scenario_trim(open + 1));
        if (!selector)
            return (scenario_fail(line, "Unknown monster selector in monster_is()."));

        term->kind = SCN_TERM_MONSTER_IS;
        term->value = (s16b)selector;
        rule->when_count++;
        return (TRUE);
    }

    if (!my_strnicmp(trimmed, "monster_visible(", 16))
    {
        open = strchr(trimmed, '(');
        close = strrchr(trimmed, ')');
        if (!open || !close || (close != open + 1))
            return (scenario_fail(line, "Malformed monster_visible() condition."));

        term->kind = SCN_TERM_MONSTER_VISIBLE;
        rule->when_count++;
        return (TRUE);
    }

    eq = strchr(trimmed, '=');
    if (eq)
    {
        *eq++ = '\0';
        eq = scenario_trim(eq);

        if (!my_stricmp(trimmed, "depth"))
        {
            if (!scenario_parse_int(eq, &value))
                return (scenario_fail(line, "Invalid depth condition."));

            term->kind = SCN_TERM_DEPTH;
            term->value = (s16b)value;
            rule->when_count++;
            return (TRUE);
        }

        if (!my_stricmp(trimmed, "new_depth"))
        {
            if (!scenario_parse_int(eq, &value))
                return (scenario_fail(line, "Invalid new_depth condition."));

            term->kind = SCN_TERM_NEW_DEPTH;
            term->value = (s16b)value;
            rule->when_count++;
            return (TRUE);
        }

        return (scenario_fail(line, "Unknown rule condition."));
    }

    flag_idx = scenario_intern_flag(trimmed, line);
    if (flag_idx < 0)
        return (FALSE);

    term->kind = SCN_TERM_FLAG;
    term->flag_idx = (byte)flag_idx;
    rule->when_count++;
    return (TRUE);
}

/* Parse one comma-separated when= condition list into rule terms. */
static bool scenario_parse_rule_when(
    scenario_rule* rule, cptr token, int line)
{
    char local[256];
    char* part = NULL;
    char* cursor = NULL;
    int depth = 0;

    if (!token || !token[0])
        return (scenario_fail(line, "Missing rule condition."));

    my_strcpy(local, token, sizeof(local));
    part = local;
    cursor = local;

    while (TRUE)
    {
        if (*cursor == '(')
            depth++;
        else if ((*cursor == ')') && (depth > 0))
            depth--;

        if (((*cursor == ',') && (depth == 0)) || (*cursor == '\0'))
        {
            char saved = *cursor;

            *cursor = '\0';
            if (!scenario_parse_rule_term(rule, scenario_trim(part), line))
                return (FALSE);
            if (!saved)
                break;

            part = cursor + 1;
        }

        cursor++;
    }

    return (TRUE);
}

/* Parse one rule action token such as SET_FLAG or DISPLAY_TEXT. */
static bool scenario_parse_rule_action(
    scenario_rule* rule, cptr token, int line)
{
    char local[128];
    char* eq;
    int idx;
    long value;
    s16b stair_feat;

    if (!token || !token[0])
        return (TRUE);
    if (rule->action_count >= SCENARIO_MAX_RULE_ACTIONS)
        return (scenario_fail(line, "Too many rule actions."));

    if (!my_stricmp(token, "DENY_ACTION"))
    {
        rule->actions[rule->action_count].kind = SCN_ACTION_DENY_ACTION;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(token, "END_GAME"))
    {
        rule->actions[rule->action_count].kind = SCN_ACTION_END_GAME;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(token, "BREAK_OATH"))
    {
        rule->actions[rule->action_count].kind = SCN_ACTION_BREAK_OATH;
        rule->action_count++;
        return (TRUE);
    }

    my_strcpy(local, token, sizeof(local));
    eq = strchr(local, '=');
    if (!eq)
        return (scenario_fail(line, "Unknown rule action."));

    *eq++ = '\0';
    if (!my_stricmp(scenario_trim(local), "SET_FLAG"))
    {
        idx = scenario_intern_flag(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_SET_FLAG;
        rule->actions[rule->action_count].flag_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "DISPLAY_TEXT"))
    {
        idx = scenario_intern_text(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_DISPLAY_TEXT;
        rule->actions[rule->action_count].text_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "ADD_NOTE"))
    {
        idx = scenario_intern_text(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_ADD_NOTE;
        rule->actions[rule->action_count].text_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "CLEAR_FLAG"))
    {
        idx = scenario_intern_flag(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_CLEAR_FLAG;
        rule->actions[rule->action_count].flag_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "DISPLAY_MESSAGE"))
    {
        idx = scenario_intern_text(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_DISPLAY_MESSAGE;
        rule->actions[rule->action_count].text_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "CONFIRM_TEXT"))
    {
        idx = scenario_intern_text(scenario_trim(eq), line);
        if (idx < 0)
            return (FALSE);

        rule->actions[rule->action_count].kind = SCN_ACTION_CONFIRM_TEXT;
        rule->actions[rule->action_count].text_idx = (byte)idx;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "SET_NEW_DEPTH"))
    {
        if (!scenario_parse_int(scenario_trim(eq), &value) || (value < 0)
            || (value >= MAX_DEPTH))
        {
            return (scenario_fail(line, "Invalid SET_NEW_DEPTH value."));
        }

        rule->actions[rule->action_count].kind = SCN_ACTION_SET_NEW_DEPTH;
        rule->actions[rule->action_count].value = (s16b)value;
        rule->action_count++;
        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(local), "CREATE_STAIRS"))
    {
        if (!scenario_parse_create_stairs_value(scenario_trim(eq), &stair_feat))
            return (scenario_fail(line, "Invalid CREATE_STAIRS value."));

        rule->actions[rule->action_count].kind = SCN_ACTION_CREATE_STAIRS;
        rule->actions[rule->action_count].value = stair_feat;
        rule->action_count++;
        return (TRUE);
    }

    return (scenario_fail(line, "Unknown rule action."));
}

/* Parse one authored event rule line. */
static bool scenario_parse_rule(cptr token, int line)
{
    char local[512];
    char* parts[32];
    int count;
    int i;
    int event;
    scenario_rule* rule;

    if (scenario.rule_count >= SCENARIO_MAX_RULES)
        return (scenario_fail(line, "Too many scenario rules."));

    my_strcpy(local, token, sizeof(local));
    count = scenario_split(local, parts, N_ELEMENTS(parts));
    if (count < 2)
        return (scenario_fail(line, "Malformed rule directive."));

    event = scenario_lookup_event(scenario_trim(parts[0]));
    if (!event)
        return (scenario_fail(line, "Unknown rule event."));

    rule = &scenario.rules[scenario.rule_count++];
    WIPE(rule, scenario_rule);
    rule->event = (byte)event;

    for (i = 1; i < count; i++)
    {
        char* part = scenario_trim(parts[i]);

        if (!my_strnicmp(part, "tag=", 4))
        {
            my_strcpy(rule->tag, scenario_trim(part + 4), sizeof(rule->tag));
            if (!rule->tag[0])
                return (scenario_fail(line, "Rule tag cannot be empty."));
            rule->tag_set = TRUE;
        }
        else if (!my_strnicmp(part, "when=", 5))
        {
            if (!scenario_parse_rule_when(rule, scenario_trim(part + 5), line))
                return (FALSE);
        }
        else if (!scenario_parse_rule_action(rule, part, line))
        {
            return (FALSE);
        }
    }

    if (rule->action_count <= 0)
        return (scenario_fail(line, "Scenario rule needs at least one action."));

    return (TRUE);
}

/* Look up one authored MAP block by exact dungeon depth. */
static scenario_map* scenario_find_map(int depth)
{
    int i;

    for (i = 0; i < scenario.level_count; i++)
    {
        if (scenario.levels[i].depth_set && (scenario.levels[i].depth == depth))
            return (&scenario.levels[i]);
    }

    return (NULL);
}

/* Resolve one terrain legend inside one authored MAP block. */
static scenario_terrain_legend* scenario_find_terrain_legend_in(
    scenario_map* level, char symbol)
{
    int i;

    if (!level)
        return (NULL);

    for (i = 0; i < level->terrain_legend_count; i++)
    {
        if (level->terrain_legends[i].symbol == symbol)
            return (&level->terrain_legends[i]);
    }

    return (NULL);
}

static bool scenario_parse_cave_info_flag_token(
    u16b* info, cptr token, int line)
{
    char local[64];
    char* eq;
    bool value = TRUE;

    my_strcpy(local, token, sizeof(local));
    eq = strchr(local, '=');

    if (eq)
    {
        *eq++ = '\0';
        if (!scenario_parse_bool(scenario_trim(eq), &value))
            return (scenario_fail(line, "Invalid cave flag value."));
    }

    if (!my_stricmp(local, "MARK"))
    {
        if (value)
            *info |= CAVE_MARK;
        else
            *info &= ~CAVE_MARK;
    }
    else if (!my_stricmp(local, "GLOW"))
    {
        if (value)
            *info |= CAVE_GLOW;
        else
            *info &= ~CAVE_GLOW;
    }
    else if (!my_stricmp(local, "ICKY"))
    {
        if (value)
            *info |= CAVE_ICKY;
        else
            *info &= ~CAVE_ICKY;
    }
    else if (!my_stricmp(local, "ROOM"))
    {
        if (value)
            *info |= CAVE_ROOM;
        else
            *info &= ~CAVE_ROOM;
    }
    else if (!my_stricmp(local, "G_VAULT")
        || !my_stricmp(local, "GVAULT"))
    {
        if (value)
            *info |= CAVE_G_VAULT;
        else
            *info &= ~CAVE_G_VAULT;
    }
    else if (!my_stricmp(local, "HIDDEN"))
    {
        if (value)
            *info |= CAVE_HIDDEN;
        else
            *info &= ~CAVE_HIDDEN;
    }
    else
    {
        return (scenario_fail(line, "Unknown cave info flag."));
    }

    return (TRUE);
}

/* Parse optional modifiers on a P:ABILITY line. */
static bool scenario_parse_player_ability_modifier(
    bool* innate, bool* active, cptr token, int line)
{
    char local[64];
    char* eq;
    bool value;

    if (!my_stricmp(token, "ACTIVE") || !my_stricmp(token, "ON"))
    {
        *active = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(token, "INACTIVE") || !my_stricmp(token, "OFF"))
    {
        *active = FALSE;
        return (TRUE);
    }

    if (!my_stricmp(token, "INNATE"))
    {
        *innate = TRUE;
        return (TRUE);
    }

    my_strcpy(local, token, sizeof(local));
    eq = strchr(local, '=');
    if (!eq)
        return (scenario_fail(line, "Unknown player ability modifier."));

    *eq++ = '\0';
    if (!scenario_parse_bool(scenario_trim(eq), &value))
    {
        return (scenario_fail(
            line, "Invalid player ability modifier value."));
    }

    if (!my_stricmp(local, "INNATE"))
    {
        *innate = value;
        return (TRUE);
    }

    if (!my_stricmp(local, "ACTIVE"))
    {
        *active = value;
        return (TRUE);
    }

    return (scenario_fail(line, "Unknown player ability modifier."));
}

static bool scenario_parse_object_modifier(
    scenario_object_spec* spec, cptr token, int line)
{
    char key[32];
    char value[64];
    char local[96];
    char* eq;
    long parsed;
    bool flag;
    int index;

    if (!token || !token[0])
        return (TRUE);

    my_strcpy(local, token, sizeof(local));
    eq = strchr(local, '=');

    if (!eq)
    {
        if (!my_stricmp(local, "known"))
        {
            spec->known = TRUE;
            spec->known_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "aware"))
        {
            spec->aware = TRUE;
            spec->aware_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "tried"))
        {
            spec->tried = TRUE;
            spec->tried_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "everseen"))
        {
            spec->everseen = TRUE;
            spec->everseen_set = TRUE;
            return (TRUE);
        }

        return (scenario_fail(line, "Unknown object modifier."));
    }

    *eq++ = '\0';
    my_strcpy(key, scenario_trim(local), sizeof(key));
    my_strcpy(value, scenario_trim(eq), sizeof(value));

    if (!my_stricmp(key, "known"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid known flag."));
        spec->known = flag;
        spec->known_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "aware"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid aware flag."));
        spec->aware = flag;
        spec->aware_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "tried"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid tried flag."));
        spec->tried = flag;
        spec->tried_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "everseen"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid everseen flag."));
        spec->everseen = flag;
        spec->everseen_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "tag"))
    {
        my_strcpy(spec->tag, value, sizeof(spec->tag));
        if (!spec->tag[0])
            return (scenario_fail(line, "Object tag cannot be empty."));
        spec->tag_set = TRUE;
        return (TRUE);
    }

    if (!scenario_parse_int(value, &parsed))
        return (scenario_fail(line, "Invalid object modifier value."));

    if (!my_stricmp(key, "number"))
    {
        if ((parsed <= 0) || (parsed > 99))
            return (scenario_fail(line, "Invalid item stack size."));
        spec->number = (byte)parsed;
        spec->number_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "pval"))
    {
        spec->pval = (s16b)parsed;
        spec->pval_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "discount"))
    {
        spec->discount = (byte)parsed;
        spec->discount_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "weight"))
    {
        spec->weight = (s16b)parsed;
        spec->weight_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "name1"))
    {
        spec->name1 = (byte)parsed;
        spec->name1_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "name2"))
    {
        spec->name2 = (byte)parsed;
        spec->name2_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "timeout"))
    {
        spec->timeout = (s16b)parsed;
        spec->timeout_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "att"))
    {
        spec->att = (s16b)parsed;
        spec->att_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "evn"))
    {
        spec->evn = (s16b)parsed;
        spec->evn_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "dd"))
    {
        spec->dd = (byte)parsed;
        spec->dd_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "ds"))
    {
        spec->ds = (byte)parsed;
        spec->ds_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "pd"))
    {
        spec->pd = (byte)parsed;
        spec->pd_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "ps"))
    {
        spec->ps = (byte)parsed;
        spec->ps_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "pickup"))
    {
        spec->pickup = (byte)parsed;
        spec->pickup_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "ident"))
    {
        spec->ident = (u32b)parsed;
        spec->ident_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "marked"))
    {
        spec->marked = (byte)parsed;
        spec->marked_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "xtra1"))
    {
        spec->xtra1 = (byte)parsed;
        spec->xtra1_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "abilities"))
    {
        spec->abilities = (byte)parsed;
        spec->abilities_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "unused1"))
    {
        spec->unused1 = (s32b)parsed;
        spec->unused1_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "unused2"))
    {
        spec->unused2 = (s32b)parsed;
        spec->unused2_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "unused3"))
    {
        spec->unused3 = (s32b)parsed;
        spec->unused3_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "unused4"))
    {
        spec->unused4 = (s32b)parsed;
        spec->unused4_set = TRUE;
        return (TRUE);
    }

    index = scenario_parse_indexed_key(key, "skilltype", 8);
    if (index >= 0)
    {
        spec->skilltype[index] = (byte)parsed;
        spec->ability_slot_set[index] = TRUE;
        return (TRUE);
    }

    index = scenario_parse_indexed_key(key, "abilitynum", 8);
    if (index >= 0)
    {
        spec->abilitynum[index] = (byte)parsed;
        spec->ability_slot_set[index] = TRUE;
        return (TRUE);
    }

    return (scenario_fail(line, "Unknown object modifier."));
}

static bool scenario_parse_monster_modifier(
    scenario_monster_spec* spec, cptr token, int line)
{
    char key[32];
    char value[64];
    char local[96];
    char* eq;
    long parsed;
    bool flag;
    int index;

    if (!token || !token[0])
        return (TRUE);

    my_strcpy(local, token, sizeof(local));
    eq = strchr(local, '=');

    if (!eq)
    {
        if (!my_stricmp(local, "asleep"))
        {
            spec->sleep = TRUE;
            spec->sleep_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "awake"))
        {
            spec->sleep = FALSE;
            spec->sleep_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "home_wander"))
        {
            spec->home_wander = TRUE;
            spec->home_wander_set = TRUE;
            return (TRUE);
        }
        if (!my_stricmp(local, "no_open_doors"))
        {
            spec->no_open_doors = TRUE;
            spec->no_open_doors_set = TRUE;
            return (TRUE);
        }
        return (scenario_fail(line, "Unknown monster modifier."));
    }

    *eq++ = '\0';
    my_strcpy(key, scenario_trim(local), sizeof(key));
    my_strcpy(value, scenario_trim(eq), sizeof(value));

    if (!my_stricmp(key, "sleep"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid sleep flag."));
        spec->sleep = flag;
        spec->sleep_set = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(key, "home_wander"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid home_wander flag."));
        spec->home_wander = flag;
        spec->home_wander_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "tag"))
    {
        my_strcpy(spec->tag, value, sizeof(spec->tag));
        if (!spec->tag[0])
            return (scenario_fail(line, "Monster tag cannot be empty."));
        spec->tag_set = TRUE;
        return (TRUE);
    }

    if (!my_stricmp(key, "no_open_doors"))
    {
        if (!scenario_parse_bool(value, &flag))
            return (scenario_fail(line, "Invalid no_open_doors flag."));
        spec->no_open_doors = flag;
        spec->no_open_doors_set = TRUE;
        return (TRUE);
    }

    if (!scenario_parse_int(value, &parsed))
        return (scenario_fail(line, "Invalid monster modifier value."));

    if (!my_stricmp(key, "image_r_idx"))
    {
        spec->image_r_idx = (s16b)parsed;
        spec->image_r_idx_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "hp"))
    {
        spec->hp = (s16b)parsed;
        spec->hp_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "maxhp"))
    {
        spec->maxhp = (s16b)parsed;
        spec->maxhp_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "alertness"))
    {
        spec->alertness = (s16b)parsed;
        spec->alertness_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "skip_next_turn"))
    {
        spec->skip_next_turn = (byte)parsed;
        spec->skip_next_turn_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "mspeed"))
    {
        spec->mspeed = (byte)parsed;
        spec->mspeed_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "energy"))
    {
        spec->energy = (byte)parsed;
        spec->energy_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "stunned"))
    {
        spec->stunned = (byte)parsed;
        spec->stunned_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "confused"))
    {
        spec->confused = (byte)parsed;
        spec->confused_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "hasted"))
    {
        spec->hasted = (s16b)parsed;
        spec->hasted_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "slowed"))
    {
        spec->slowed = (s16b)parsed;
        spec->slowed_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "stance"))
    {
        spec->stance = (byte)parsed;
        spec->stance_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "morale"))
    {
        spec->morale = (s16b)parsed;
        spec->morale_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "tmp_morale"))
    {
        spec->tmp_morale = (s16b)parsed;
        spec->tmp_morale_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "noise"))
    {
        spec->noise = (byte)parsed;
        spec->noise_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "encountered"))
    {
        spec->encountered = (byte)parsed;
        spec->encountered_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "target_y"))
    {
        spec->target_y = (byte)parsed;
        spec->target_y_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "target_x"))
    {
        spec->target_x = (byte)parsed;
        spec->target_x_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "wandering_idx"))
    {
        spec->wandering_idx = (s16b)parsed;
        spec->wandering_idx_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "wandering_dist"))
    {
        spec->wandering_dist = (byte)parsed;
        spec->wandering_dist_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "mana"))
    {
        spec->mana = (byte)parsed;
        spec->mana_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "song"))
    {
        spec->song = (byte)parsed;
        spec->song_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "skip_this_turn"))
    {
        spec->skip_this_turn = (byte)parsed;
        spec->skip_this_turn_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "consecutive_attacks"))
    {
        spec->consecutive_attacks = (s16b)parsed;
        spec->consecutive_attacks_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "turns_stationary"))
    {
        spec->turns_stationary = (s16b)parsed;
        spec->turns_stationary_set = TRUE;
        return (TRUE);
    }
    if (!my_stricmp(key, "mflag"))
    {
        spec->mflag = (u32b)parsed;
        spec->mflag_set = TRUE;
        return (TRUE);
    }

    index = scenario_parse_indexed_key(key, "prev", ACTION_MAX);
    if (index >= 0)
    {
        spec->previous_action[index] = (byte)parsed;
        spec->previous_action_set[index] = TRUE;
        return (TRUE);
    }

    return (scenario_fail(line, "Unknown monster modifier."));
}

static bool scenario_parse_flag_line(char* spec, int line)
{
    char local[160];
    char* token;

    my_strcpy(local, spec, sizeof(local));

    for (token = strtok(local, " ,|\t"); token; token = strtok(NULL, " ,|\t"))
    {
        if (!my_stricmp(token, "LIT"))
        {
            scenario.flags |= SCN_FLAG_LIT;
        }
        else
        {
            return (scenario_fail(line, "Unknown scenario flag."));
        }
    }

    return (TRUE);
}

static bool scenario_append_row(
    char rows[SCENARIO_MAX_ROWS][SCENARIO_MAX_COLS + 1], int* row_count,
    int* width, cptr row, int line)
{
    size_t len = strlen(row);

    if (*row_count >= SCENARIO_MAX_ROWS)
        return (scenario_fail(line, "Scenario map is too tall."));

    if (!(*width))
    {
        if ((len <= 0) || (len > SCENARIO_MAX_COLS))
            return (scenario_fail(line, "Scenario map has invalid width."));
        *width = (int)len;
    }
    else if ((int)len != *width)
    {
        return (scenario_fail(line, "Scenario map rows must be rectangular."));
    }

    my_strcpy(rows[*row_count], row, SCENARIO_MAX_COLS + 1);
    (*row_count)++;
    return (TRUE);
}

static bool scenario_parse_terrain_legend(
    scenario_map* level, char* body, int line)
{
    char local[256];
    char* parts[24];
    int count;
    int feat = 0;
    int i;
    scenario_terrain_legend* legend;
    long info_value;
    char* eq;
    long chance_value;

    if (!level)
        return (scenario_fail(line, "T: requires an active MAP block."));

    if (level->terrain_legend_count >= SCENARIO_MAX_TERRAIN_LEGENDS)
        return (scenario_fail(line, "Too many terrain legends."));

    my_strcpy(local, body, sizeof(local));
    int base_idx;
    int modifier_idx;
    char symbol;
    bool good_flag;

    count = scenario_split(local, parts, 24);
    if (count < 2)
        return (scenario_fail(line, "Malformed terrain legend."));

    parts[0] = scenario_trim(parts[0]);
    if (!parts[0][0])
    {
        symbol = ':';
        base_idx = 1;
    }
    else
    {
        if (parts[0][1] || (parts[0][0] == ' '))
        {
            return (scenario_fail(line, "Invalid terrain legend symbol."));
        }

        symbol = parts[0][0];
        base_idx = 1;
    }

    if (count <= base_idx)
        return (scenario_fail(line, "Malformed terrain legend."));

    parts[base_idx] = scenario_trim(parts[base_idx]);
    if ((count > base_idx + 1) && parts[base_idx + 1])
        parts[base_idx + 1] = scenario_trim(parts[base_idx + 1]);

    if (!parts[base_idx][0])
    {
        return (scenario_fail(line, "Malformed terrain legend."));
    }

    legend = &level->terrain_legends[level->terrain_legend_count++];
    WIPE(legend, scenario_terrain_legend);
    legend->symbol = symbol;
    legend->type = 0;
    legend->feat = 0;
    legend->place = 0;
    legend->chance = 100;
    legend->info = 0;
    legend->entity_set = FALSE;
    legend->entity.chance = 100;
    legend->entity.drop_type = DROP_TYPE_NOT_DAMAGED;

    if (!my_stricmp(parts[base_idx], "FEATURE"))
    {
        if (count <= base_idx + 1)
            return (scenario_fail(line, "Malformed terrain legend."));

        feat = scenario_lookup_feature(parts[base_idx + 1]);
        if (feat < 0)
            return (scenario_fail(line, "Unknown terrain feature."));

        legend->type = SCN_TERRAIN_FEATURE;
        legend->feat = (byte)feat;
        modifier_idx = base_idx + 2;
    }
    else if (!my_stricmp(parts[base_idx], "PLACE"))
    {
        if (count <= base_idx + 1)
            return (scenario_fail(line, "Malformed terrain legend."));

        legend->type = SCN_TERRAIN_PLACE;

        if (!my_stricmp(parts[base_idx + 1], "CLOSED_DOOR"))
            legend->place = SCN_TERRAIN_PLACE_CLOSED_DOOR;
        else if (!my_stricmp(parts[base_idx + 1], "TRAP"))
            legend->place = SCN_TERRAIN_PLACE_TRAP;
        else if (!my_stricmp(parts[base_idx + 1], "FORGE"))
            legend->place = SCN_TERRAIN_PLACE_FORGE;
        else
            return (scenario_fail(line, "Unknown terrain placer."));

        modifier_idx = base_idx + 2;
    }
    else if (!my_stricmp(parts[base_idx], "FLOOR"))
    {
        legend->type = SCN_TERRAIN_FLOOR;
        legend->feat = FEAT_FLOOR;
        modifier_idx = base_idx + 1;
    }
    else
    {
        return (scenario_fail(line, "Unknown terrain legend type."));
    }

    for (i = modifier_idx; i < count; i++)
    {
        char* token = scenario_trim(parts[i]);

        my_strcpy(local, token, sizeof(local));
        eq = strchr(local, '=');

        if (!my_strnicmp(token, "info=", 5))
        {
            if (!scenario_parse_int(token + 5, &info_value))
                return (scenario_fail(line, "Invalid terrain info mask."));
            legend->info = (u16b)info_value;
        }
        else if (eq)
        {
            int idx;

            *eq++ = '\0';

            if (!my_stricmp(scenario_trim(local), "CHANCE"))
            {
                if (!scenario_parse_int(scenario_trim(eq), &chance_value)
                    || (chance_value < 0) || (chance_value > 100))
                {
                    return (scenario_fail(line, "Invalid terrain placer chance."));
                }

                legend->chance = (byte)chance_value;
            }
            else if (!my_stricmp(scenario_trim(local), "MONSTER"))
            {
                if (legend->entity_set && (legend->entity.kind != SCN_LEVEL_ENTITY_MONSTER))
                {
                    return (scenario_fail(
                        line, "A terrain legend cannot define multiple entity kinds."));
                }

                idx = scenario_lookup_monster(scenario_trim(eq));
                if (idx <= 0)
                    return (scenario_fail(line, "Unknown monster race."));

                legend->entity_set = TRUE;
                legend->entity.kind = SCN_LEVEL_ENTITY_MONSTER;
                legend->entity.monster.r_idx = (s16b)idx;
            }
            else if (!my_stricmp(scenario_trim(local), "OBJECT"))
            {
                if (legend->entity_set && (legend->entity.kind != SCN_LEVEL_ENTITY_OBJECT))
                {
                    return (scenario_fail(
                        line, "A terrain legend cannot define multiple entity kinds."));
                }

                idx = scenario_lookup_object(scenario_trim(eq));
                if (idx <= 0)
                {
                    return (scenario_fail(
                        line, "Unknown or ambiguous object kind."));
                }

                legend->entity_set = TRUE;
                legend->entity.kind = SCN_LEVEL_ENTITY_OBJECT;
                legend->entity.object.k_idx = (s16b)idx;
            }
            else if (!my_stricmp(scenario_trim(local), "GEN_MONSTER"))
            {
                if (legend->entity_set
                    && (legend->entity.kind != SCN_LEVEL_ENTITY_GEN_MONSTER))
                {
                    return (scenario_fail(
                        line, "A terrain legend cannot define multiple entity kinds."));
                }

                legend->entity_set = TRUE;
                legend->entity.kind = SCN_LEVEL_ENTITY_GEN_MONSTER;
                if (!scenario_parse_depth_span(scenario_trim(eq),
                        &legend->entity.relative_depth,
                        &legend->entity.min_depth, &legend->entity.max_depth, line))
                {
                    return (FALSE);
                }
            }
            else if (!my_stricmp(scenario_trim(local), "GEN_OBJECT"))
            {
                if (legend->entity_set
                    && (legend->entity.kind != SCN_LEVEL_ENTITY_GEN_OBJECT))
                {
                    return (scenario_fail(
                        line, "A terrain legend cannot define multiple entity kinds."));
                }

                legend->entity_set = TRUE;
                legend->entity.kind = SCN_LEVEL_ENTITY_GEN_OBJECT;
                if (!scenario_parse_depth_span(scenario_trim(eq),
                        &legend->entity.relative_depth,
                        &legend->entity.min_depth, &legend->entity.max_depth, line))
                {
                    return (FALSE);
                }
            }
            else if (!my_stricmp(scenario_trim(local), "GOOD"))
            {
                if (legend->entity.kind != SCN_LEVEL_ENTITY_GEN_OBJECT)
                {
                    return (scenario_fail(
                        line, "GOOD requires a generated object tile."));
                }

                if (!scenario_parse_bool(scenario_trim(eq), &good_flag))
                    return (scenario_fail(line, "Invalid GOOD flag."));
                legend->entity.good = good_flag;
            }
            else if (!my_stricmp(scenario_trim(local), "DROP"))
            {
                if (legend->entity.kind != SCN_LEVEL_ENTITY_GEN_OBJECT)
                {
                    return (scenario_fail(
                        line, "DROP requires a generated object tile."));
                }

                eq = scenario_trim(eq);
                if (!my_stricmp(eq, "NOT_DAMAGED"))
                    legend->entity.drop_type = DROP_TYPE_NOT_DAMAGED;
                else if (!my_stricmp(eq, "UNTHEMED"))
                    legend->entity.drop_type = DROP_TYPE_UNTHEMED;
                else if (!my_stricmp(eq, "CHEST"))
                    legend->entity.drop_type = DROP_TYPE_CHEST;
                else
                    return (scenario_fail(line, "Unknown generated object drop type."));
            }
            else
            {
                return (scenario_fail(line, "Unknown terrain legend modifier."));
            }
        }
        else if (!my_stricmp(token, "GOOD"))
        {
            if (legend->entity.kind != SCN_LEVEL_ENTITY_GEN_OBJECT)
            {
                return (scenario_fail(
                    line, "GOOD requires a generated object tile."));
            }

            legend->entity.good = TRUE;
        }
        else if (!scenario_parse_cave_info_flag_token(&legend->info, token, line))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}

/* Start one new authored MAP block with its display name. */
static scenario_map* scenario_add_map(cptr name, int line)
{
    scenario_map* level;
    char local_name[SCENARIO_MAX_NAME_LEN];

    my_strcpy(local_name, name ? name : "", sizeof(local_name));
    if (!scenario_trim(local_name)[0])
    {
        scenario_fail(line, "MAP name cannot be empty.");
        return (NULL);
    }

    if (scenario.level_count >= SCENARIO_MAX_LEVELS)
    {
        scenario_fail(line, "Too many scenario MAP blocks.");
        return (NULL);
    }

    level = &scenario.levels[scenario.level_count++];
    WIPE(level, scenario_map);
    my_strcpy(level->name, scenario_trim(local_name), sizeof(level->name));
    return (level);
}

/* Return the full generated map height for one authored MAP block. */
static int scenario_map_height(const scenario_map* level)
{
    if (!level)
        return (0);

    if (level->map_size_set)
        return (level->map_height);

    return (level->height);
}

/* Return the full generated map width for one authored MAP block. */
static int scenario_map_width(const scenario_map* level)
{
    if (!level)
        return (0);

    if (level->map_size_set)
        return (level->map_width);

    return (level->width);
}

/* Parse the generated map size for one MAP block. */
static bool scenario_parse_map_size(
    scenario_map* level, cptr spec, int line)
{
    char local[64];
    char* parts[3];
    int count;
    long value;

    if (!level)
        return (scenario_fail(line, "MAP_SIZE requires an active MAP block."));

    my_strcpy(local, spec, sizeof(local));
    count = scenario_split(local, parts, N_ELEMENTS(parts));
    if (count < 2)
        return (scenario_fail(line, "Malformed MAP_SIZE directive."));

    if (!scenario_parse_int(scenario_trim(parts[0]), &value) || (value <= 0)
        || (value > SCENARIO_MAX_ROWS))
    {
        return (scenario_fail(line, "Invalid authored map height."));
    }
    level->map_height = (int)value;

    if (!scenario_parse_int(scenario_trim(parts[1]), &value) || (value <= 0)
        || (value > SCENARIO_MAX_COLS))
    {
        return (scenario_fail(line, "Invalid authored map width."));
    }
    level->map_width = (int)value;
    level->map_size_set = TRUE;
    return (TRUE);
}

/* Parse the exact dungeon depth where one MAP block should be used. */
static bool scenario_parse_map_depth(scenario_map* level, cptr spec, int line)
{
    long value;

    if (!level)
        return (scenario_fail(line, "MAP_DEPTH requires an active MAP block."));

    if (!scenario_parse_int(spec, &value) || (value < 0) || (value >= MAX_DEPTH))
        return (scenario_fail(line, "Invalid MAP depth."));

    if (scenario_find_map((int)value) && (scenario_find_map((int)value) != level))
        return (scenario_fail(line, "Duplicate MAP depth."));

    level->depth = (s16b)value;
    level->depth_set = TRUE;
    return (TRUE);
}

/* Parse the top-left origin where one MAP block is stamped into its map. */
static bool scenario_parse_map_origin(
    scenario_map* level, cptr spec, int line)
{
    char local[64];
    char* parts[3];
    int count;
    long value;

    if (!level)
        return (scenario_fail(line, "MAP_ORIGIN requires an active MAP block."));

    my_strcpy(local, spec, sizeof(local));
    count = scenario_split(local, parts, N_ELEMENTS(parts));
    if (count < 2)
        return (scenario_fail(line, "Malformed MAP_ORIGIN directive."));

    if (!scenario_parse_int(scenario_trim(parts[0]), &value) || (value < 0))
        return (scenario_fail(line, "Invalid authored map origin y coordinate."));
    level->origin_y = (int)value;

    if (!scenario_parse_int(scenario_trim(parts[1]), &value) || (value < 0))
        return (scenario_fail(line, "Invalid authored map origin x coordinate."));
    level->origin_x = (int)value;

    level->origin_set = TRUE;
    return (TRUE);
}

/* Parse one sparse depth range like +1, +1..+4, or 20. */
static bool scenario_parse_depth_span(
    cptr token, bool* relative, s16b* min_depth, s16b* max_depth, int line)
{
    char local[64];
    char* split;
    char* left;
    char* right;
    long min_value;
    long max_value;
    bool rel_left;
    bool rel_right;

    if (!token || !token[0])
        return (scenario_fail(line, "Missing generation depth."));

    my_strcpy(local, token, sizeof(local));
    split = strstr(local, "..");
    if (split)
    {
        *split = '\0';
        right = split + 2;
    }
    else
    {
        right = NULL;
    }

    left = scenario_trim(local);
    if (!left[0])
        return (scenario_fail(line, "Missing generation depth."));

    rel_left = ((left[0] == '+') || (left[0] == '-'));
    if (!scenario_parse_int(left, &min_value))
        return (scenario_fail(line, "Invalid generation depth."));

    if (right)
    {
        right = scenario_trim(right);
        if (!right[0])
            return (scenario_fail(line, "Invalid generation depth range."));

        rel_right = ((right[0] == '+') || (right[0] == '-'));
        if (rel_left != rel_right)
        {
            return (scenario_fail(
                line, "Generation depth range must be consistently relative or absolute."));
        }

        if (!scenario_parse_int(right, &max_value))
            return (scenario_fail(line, "Invalid generation depth."));
    }
    else
    {
        max_value = min_value;
        rel_right = rel_left;
    }

    if ((min_value < SHRT_MIN) || (min_value > SHRT_MAX) || (max_value < SHRT_MIN)
        || (max_value > SHRT_MAX))
    {
        return (scenario_fail(line, "Generation depth is out of range."));
    }

    if (min_value > max_value)
    {
        long tmp = min_value;
        min_value = max_value;
        max_value = tmp;
    }

    *relative = rel_left;
    *min_depth = (s16b)min_value;
    *max_depth = (s16b)max_value;
    return (TRUE);
}

/* Parse MAP block flags that affect one authored special level. */
static bool scenario_parse_map_flags(
    scenario_map* level, cptr spec, int line)
{
    char local[160];
    char* token;

    if (!level)
        return (scenario_fail(line, "MAP_FLAGS requires an active MAP block."));

    my_strcpy(local, spec, sizeof(local));

    for (token = strtok(local, " ,|\t"); token; token = strtok(NULL, " ,|\t"))
    {
        if (!my_stricmp(token, "NONE"))
            continue;
        if (!my_stricmp(token, "GOOD_ITEM"))
            level->flags |= SCN_LEVEL_FLAG_GOOD_ITEM;
        else
            return (scenario_fail(line, "Unknown MAP flag."));
    }

    return (TRUE);
}

/* Parse one sparse entity placement inside a MAP block. */
static bool scenario_parse_map_entity(
    scenario_map* level, cptr body, int line)
{
    char local[512];
    char local2[128];
    char* parts[64];
    int count;
    long value;
    int idx;
    int i;
    scenario_map_entity* entity;

    if (!level)
        return (scenario_fail(line, "Sparse entities require an active MAP block."));

    my_strcpy(local, body, sizeof(local));
    count = scenario_split(local, parts, N_ELEMENTS(parts));
    if (count < 3)
        return (scenario_fail(line, "Malformed sparse entity directive."));

    if (!scenario_parse_int(scenario_trim(parts[0]), &value))
        return (scenario_fail(line, "Invalid entity y coordinate."));
    if ((value < 0) || (value >= SCENARIO_MAX_ROWS))
        return (scenario_fail(line, "Entity y coordinate is out of bounds."));

    if (!scenario_parse_int(scenario_trim(parts[1]), &value))
        return (scenario_fail(line, "Invalid entity x coordinate."));
    if ((value < 0) || (value >= SCENARIO_MAX_COLS))
        return (scenario_fail(line, "Entity x coordinate is out of bounds."));

    if (count < 3)
        return (scenario_fail(line, "Malformed sparse entity directive."));

    if (!my_stricmp(scenario_trim(parts[2]), "PLAYER"))
    {
        if (count != 3)
            return (scenario_fail(line, "PLAYER entity does not take modifiers."));

        if (!scenario_parse_int(scenario_trim(parts[0]), &value))
            return (FALSE);
        level->player_y = (int)value;

        if (!scenario_parse_int(scenario_trim(parts[1]), &value))
            return (FALSE);
        level->player_x = (int)value;

        level->player_pos_set = TRUE;
        return (TRUE);
    }

    if (level->entity_count >= SCENARIO_MAX_LEVEL_ENTITIES)
        return (scenario_fail(line, "Too many sparse entities in this MAP block."));

    entity = &level->entities[level->entity_count++];
    WIPE(entity, scenario_map_entity);
    entity->chance = 100;
    entity->drop_type = DROP_TYPE_NOT_DAMAGED;
    entity->y = atoi(scenario_trim(parts[0]));
    entity->x = atoi(scenario_trim(parts[1]));

    if (!my_stricmp(scenario_trim(parts[2]), "OBJECT"))
    {
        if (count < 4)
            return (scenario_fail(line, "Sparse object is missing its kind."));

        idx = scenario_lookup_object(scenario_trim(parts[3]));
        if (idx <= 0)
            return (scenario_fail(line, "Unknown or ambiguous object kind."));

        entity->kind = SCN_LEVEL_ENTITY_OBJECT;
        entity->object.k_idx = (s16b)idx;

        for (i = 4; i < count; i++)
        {
            if (!scenario_parse_object_modifier(
                    &entity->object, scenario_trim(parts[i]), line))
            {
                return (FALSE);
            }
        }

        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(parts[2]), "MONSTER"))
    {
        if (count < 4)
            return (scenario_fail(line, "Sparse monster is missing its race."));

        idx = scenario_lookup_monster(scenario_trim(parts[3]));
        if (idx <= 0)
            return (scenario_fail(line, "Unknown monster race."));

        entity->kind = SCN_LEVEL_ENTITY_MONSTER;
        entity->monster.r_idx = (s16b)idx;

        for (i = 4; i < count; i++)
        {
            if (!scenario_parse_monster_modifier(
                    &entity->monster, scenario_trim(parts[i]), line))
            {
                return (FALSE);
            }
        }

        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(parts[2]), "GEN_OBJECT"))
    {
        entity->kind = SCN_LEVEL_ENTITY_GEN_OBJECT;

        for (i = 3; i < count; i++)
        {
            char* token = scenario_trim(parts[i]);
            char* eq;

            my_strcpy(local2, token, sizeof(local2));
            eq = strchr(local2, '=');
            if (!eq)
                return (scenario_fail(line, "Unknown generated object modifier."));

            *eq++ = '\0';
            if (!my_stricmp(local2, "depth"))
            {
                if (!scenario_parse_depth_span(
                        scenario_trim(eq), &entity->relative_depth,
                        &entity->min_depth, &entity->max_depth, line))
                {
                    return (FALSE);
                }
            }
            else if (!my_stricmp(local2, "chance"))
            {
                if (!scenario_parse_int(scenario_trim(eq), &value) || (value < 0)
                    || (value > 100))
                {
                    return (scenario_fail(line, "Invalid generated object chance."));
                }
                entity->chance = (byte)value;
            }
            else if (!my_stricmp(local2, "good"))
            {
                bool good;

                if (!scenario_parse_bool(scenario_trim(eq), &good))
                    return (scenario_fail(line, "Invalid generated object good flag."));
                entity->good = good;
            }
            else if (!my_stricmp(local2, "drop"))
            {
                eq = scenario_trim(eq);
                if (!my_stricmp(eq, "NOT_DAMAGED"))
                    entity->drop_type = DROP_TYPE_NOT_DAMAGED;
                else if (!my_stricmp(eq, "UNTHEMED"))
                    entity->drop_type = DROP_TYPE_UNTHEMED;
                else if (!my_stricmp(eq, "CHEST"))
                    entity->drop_type = DROP_TYPE_CHEST;
                else
                    return (scenario_fail(line, "Unknown generated object drop type."));
            }
            else
            {
                return (scenario_fail(line, "Unknown generated object modifier."));
            }
        }

        return (TRUE);
    }

    if (!my_stricmp(scenario_trim(parts[2]), "GEN_MONSTER"))
    {
        entity->kind = SCN_LEVEL_ENTITY_GEN_MONSTER;

        for (i = 3; i < count; i++)
        {
            char* token = scenario_trim(parts[i]);
            char* eq;

            my_strcpy(local2, token, sizeof(local2));
            eq = strchr(local2, '=');
            if (!eq)
                return (scenario_fail(line, "Unknown generated monster modifier."));

            *eq++ = '\0';
            if (!my_stricmp(local2, "depth"))
            {
                if (!scenario_parse_depth_span(
                        scenario_trim(eq), &entity->relative_depth,
                        &entity->min_depth, &entity->max_depth, line))
                {
                    return (FALSE);
                }
            }
            else if (!my_stricmp(local2, "chance"))
            {
                if (!scenario_parse_int(scenario_trim(eq), &value) || (value < 0)
                    || (value > 100))
                {
                    return (scenario_fail(line, "Invalid generated monster chance."));
                }
                entity->chance = (byte)value;
            }
            else
            {
                return (scenario_fail(line, "Unknown generated monster modifier."));
            }
        }

        return (TRUE);
    }

    return (scenario_fail(line, "Unknown sparse entity type."));
}

/* Return whether a loaded scenario should seed the normal birth flow. */
bool scenario_birth_active(void)
{
    return (scenario.loaded);
}

/* Mirror reserved scenario flags onto the current live player state. */
static void scenario_sync_player_state(void)
{
    if (!p_ptr)
        return;

    if (scenario_truce_flag_idx >= 0)
        p_ptr->truce = scenario_get_flag(scenario_truce_flag_idx);
    if (scenario_escape_flag_idx >= 0)
        p_ptr->on_the_run = scenario_get_flag(scenario_escape_flag_idx);
}

/* Seed reserved scenario flags from older player state after loading a save. */
static void scenario_seed_reserved_flags(void)
{
    if (!p_ptr)
        return;

    if ((scenario_truce_flag_idx >= 0) && p_ptr->truce)
        scenario_set_flag(scenario_truce_flag_idx, TRUE);
    if ((scenario_escape_flag_idx >= 0) && p_ptr->on_the_run)
        scenario_set_flag(scenario_escape_flag_idx, TRUE);

    scenario_sync_player_state();
}

/* Check whether one fixed house choice belongs to the chosen race. */
static bool scenario_house_matches_race(void)
{
    u32b choice;

    if (!scenario.prace_set || !scenario.phouse_set)
        return (TRUE);
    if (scenario.prace >= z_info->p_max)
        return (FALSE);

    choice = p_info[scenario.prace].choice;
    return ((choice & (1UL << scenario.phouse)) != 0);
}

/* Validate any fixed stat presets against the birth stat budget. */
static bool scenario_validate_birth_stats(void)
{
    int i;
    bool any = FALSE;

    for (i = 0; i < A_MAX; i++)
    {
        if (scenario.stat_set[i])
        {
            any = TRUE;
            break;
        }
    }

    if (!any)
        return (TRUE);

    if (!scenario.prace_set || !scenario.phouse_set)
        return (scenario_fail(
            0, "Birth stat presets require a fixed player race and house."));

    for (i = 0; i < A_MAX; i++)
    {
        int raw;

        if (!scenario.stat_set[i])
            continue;

        raw = scenario.stat_base[i]
            - (p_info[scenario.prace].r_adj[i] + c_info[scenario.phouse].h_adj[i]);
        if (raw < 0)
        {
            return (scenario_fail(
                0, "A fixed birth stat is too low for the chosen race and house."));
        }
    }

    return (TRUE);
}

/* Validate any fixed skill presets for basic sanity. */
static bool scenario_validate_birth_skills(void)
{
    int i;

    for (i = 0; i < S_MAX; i++)
    {
        if (!scenario.skill_set[i])
            continue;
        if (scenario.skill_base[i] < 0)
            return (scenario_fail(0, "Fixed birth skills cannot be negative."));
    }

    return (TRUE);
}

/* Validate one depth-keyed authored MAP block. */
static bool scenario_validate_map(scenario_map* level)
{
    int map_height;
    int map_width;
    int origin_y;
    int origin_x;
    int y;

    if (!level)
        return (FALSE);

    if (!level->depth_set)
        return (scenario_fail(0, "Scenario MAP block must define MAP_DEPTH."));

    if (level->terrain_rows <= 0)
        return (scenario_fail(0, "Scenario MAP block must define map rows."));

    level->height = level->terrain_rows;
    map_height = scenario_map_height(level);
    map_width = scenario_map_width(level);
    origin_y = level->origin_set ? level->origin_y : 0;
    origin_x = level->origin_set ? level->origin_x : 0;

    if ((map_height <= 0) || (map_width <= 0))
        return (scenario_fail(0, "Scenario MAP block has invalid map bounds."));

    if ((origin_y < 0) || (origin_x < 0))
        return (scenario_fail(0, "Scenario MAP block has an invalid map origin."));

    if ((origin_y + level->height > map_height)
        || (origin_x + level->width > map_width))
    {
        return (scenario_fail(
            0, "Scenario level rows do not fit within the authored map bounds."));
    }

    for (y = 0; y < level->terrain_rows; y++)
    {
        int x;

        for (x = 0; x < level->width; x++)
        {
            char symbol = level->terrain[y][x];

            if (scenario_find_terrain_legend_in(level, symbol))
                continue;

            switch (symbol)
            {
            case '.':
            case '#':
            case '$':
            case '%':
            case ':':
            case ';':
            case '>':
            case '<':
            case '+':
            case 's':
            case '^':
            case '0':
            case '7':
            case ',':
                break;

            default:
                return (scenario_fail(
                    0, "Scenario level uses an undefined terrain symbol."));
            }
        }
    }

    if (!level->player_pos_set)
        return (scenario_fail(0, "Scenario level must define a player start."));

    if ((level->player_y < 0) || (level->player_y >= map_height)
        || (level->player_x < 0) || (level->player_x >= map_width))
    {
        return (scenario_fail(0, "Scenario level player start lies outside the terrain map."));
    }

    for (y = 0; y < level->entity_count; y++)
    {
        if ((level->entities[y].y < 0) || (level->entities[y].y >= map_height)
            || (level->entities[y].x < 0) || (level->entities[y].x >= map_width))
        {
            return (scenario_fail(0, "A sparse entity lies outside the terrain map."));
        }
    }

    for (y = 0; y < level->grid_tag_count; y++)
    {
        if ((level->grid_tags[y].y < 0) || (level->grid_tags[y].y >= map_height)
            || (level->grid_tags[y].x < 0) || (level->grid_tags[y].x >= map_width))
        {
            return (scenario_fail(0, "A tagged grid lies outside the terrain map."));
        }
    }

    return (TRUE);
}

static bool scenario_validate(void)
{
    int i;

    if (scenario.phouse_set && !scenario.prace_set)
    {
        return (scenario_fail(
            0, "A fixed player house requires a fixed player race."));
    }

    if (scenario.prace_set && scenario.phouse_set && !scenario_house_matches_race())
    {
        return (scenario_fail(
            0, "The fixed player house does not belong to the fixed race."));
    }

    if (!scenario_validate_birth_stats())
        return (FALSE);
    if (!scenario_validate_birth_skills())
        return (FALSE);

    for (i = 0; i < scenario.level_count; i++)
    {
        if (!scenario_validate_map(&scenario.levels[i]))
            return (FALSE);
    }

    return (TRUE);
}

static int scenario_default_start_exp(void)
{
    if (birth_fixed_exp)
        return (PY_FIXED_EXP);

    return (PY_START_EXP);
}

/* Return the birth XP budget that a loaded scenario should start from. */
static int scenario_birth_start_exp(void)
{
    if (scenario.exp_set)
        return (scenario.exp);

    return (scenario_default_start_exp());
}

/* Return the stat-budget cost contributed by free scenario stat presets. */
static int scenario_birth_stat_cost_offset(void)
{
    int i;
    int total_cost = 0;

    if (!scenario_birth_active() || !scenario.prace_set || !scenario.phouse_set
        || !rp_ptr || !hp_ptr)
    {
        return (0);
    }

    for (i = 0; i < A_MAX; i++)
    {
        int raw;

        if (!scenario.stat_set[i])
            continue;

        raw = scenario.stat_base[i] - (rp_ptr->r_adj[i] + hp_ptr->h_adj[i]);
        if (raw > 0)
            total_cost += ui_birth_stat_cost_for_value(raw);
    }

    return (total_cost);
}

/* Seed any fixed birth choices before the normal birth prompts begin. */
void scenario_birth_seed_choices(void)
{
    if (!scenario_birth_active())
        return;

    if (scenario.prace_set)
        p_ptr->prace = scenario.prace;
    if (scenario.phouse_set)
        p_ptr->phouse = scenario.phouse;
    if (scenario.full_name_set)
        my_strcpy(op_ptr->full_name, scenario.full_name, sizeof(op_ptr->full_name));
}

/* Seed background and stat presets once race and house are known. */
void scenario_birth_seed_background(void)
{
    int i;

    if (!scenario_birth_active())
        return;

    if (scenario.history[0])
        my_strcpy(p_ptr->history, scenario.history, sizeof(p_ptr->history));
    if (scenario.age_set)
        p_ptr->age = scenario.age;
    if (scenario.ht_set)
        p_ptr->ht = scenario.ht;
    if (scenario.wt_set)
        p_ptr->wt = scenario.wt;

    for (i = 0; i < A_MAX; i++)
    {
        int bonus;

        if (!scenario.stat_set[i])
            continue;

        bonus = rp_ptr->r_adj[i] + hp_ptr->h_adj[i];
        p_ptr->stat_base[i] = scenario.stat_base[i] - bonus;
    }
}

/* Seed fixed skill presets without spending the interactive XP budget yet. */
bool scenario_birth_seed_skills(void)
{
    int i;

    if (!scenario_birth_active())
        return (TRUE);

    for (i = 0; i < S_MAX; i++)
    {
        if (!scenario.skill_set[i])
            continue;

        p_ptr->skill_base[i] = scenario.skill_base[i];
    }
    return (TRUE);
}

/* Return whether scenario items replace the normal birth outfit. */
bool scenario_birth_overrides_outfit(void)
{
    return (scenario_birth_active() && (scenario.item_count > 0));
}

/* Return the birth XP budget after scenario presets are applied. */
int scenario_birth_get_start_exp(void)
{
    return (scenario_birth_active() ? scenario_birth_start_exp()
                                    : scenario_default_start_exp());
}

/* Return the free stat-budget offset granted by scenario stat presets. */
int scenario_birth_get_stat_cost_offset(void)
{
    return (scenario_birth_active() ? scenario_birth_stat_cost_offset() : 0);
}

/* Return whether the scenario fixes the player's race choice. */
bool scenario_birth_race_fixed(void)
{
    return (scenario_birth_active() && scenario.prace_set);
}

/* Return whether the scenario fixes the player's house choice. */
bool scenario_birth_house_fixed(void)
{
    return (scenario_birth_active() && scenario.phouse_set);
}

/* Return whether the scenario fixes every birth stat and can skip that screen. */
bool scenario_birth_stats_fixed(void)
{
    int i;

    if (!scenario_birth_active())
        return (FALSE);

    for (i = 0; i < A_MAX; i++)
    {
        if (!scenario.stat_set[i])
            return (FALSE);
    }

    return (TRUE);
}

/* Return whether the scenario fixes every birth skill and can skip that screen. */
bool scenario_birth_skills_fixed(void)
{
    int i;

    if (!scenario_birth_active())
        return (FALSE);

    for (i = 0; i < S_MAX; i++)
    {
        if (!scenario.skill_set[i])
            return (FALSE);
    }

    return (TRUE);
}

/* Return whether the scenario fixes the history step. */
bool scenario_birth_history_fixed(void)
{
    return (scenario_birth_active() && scenario.history[0]);
}

/* Return whether the scenario fixes age, height, and weight together. */
bool scenario_birth_ahw_fixed(void)
{
    return (scenario_birth_active() && scenario.age_set && scenario.ht_set
        && scenario.wt_set);
}

/* Return whether the scenario fixes the player name. */
bool scenario_birth_name_fixed(void)
{
    return (scenario_birth_active() && scenario.full_name_set);
}

/* Instantiate one authored object spec, including tagged note text wiring. */
static bool scenario_prepare_object(
    object_type* o_ptr, const scenario_object_spec* spec, bool default_known)
{
    int i;

    object_wipe(o_ptr);
    object_prep(o_ptr, spec->k_idx);

    if (spec->aware_set)
        k_info[spec->k_idx].aware = spec->aware;
    if (spec->tried_set)
        k_info[spec->k_idx].tried = spec->tried;
    if (spec->everseen_set)
        k_info[spec->k_idx].everseen = spec->everseen;

    if (spec->number_set)
        o_ptr->number = spec->number;
    if (spec->pval_set)
        o_ptr->pval = spec->pval;
    if (spec->discount_set)
        o_ptr->discount = spec->discount;
    if (spec->weight_set)
        o_ptr->weight = spec->weight;
    if (spec->name1_set)
        o_ptr->name1 = spec->name1;
    if (spec->name2_set)
        o_ptr->name2 = spec->name2;
    if (spec->timeout_set)
        o_ptr->timeout = spec->timeout;
    if (spec->att_set)
        o_ptr->att = spec->att;
    if (spec->evn_set)
        o_ptr->evn = spec->evn;
    if (spec->dd_set)
        o_ptr->dd = spec->dd;
    if (spec->ds_set)
        o_ptr->ds = spec->ds;
    if (spec->pd_set)
        o_ptr->pd = spec->pd;
    if (spec->ps_set)
        o_ptr->ps = spec->ps;
    if (spec->pickup_set)
        o_ptr->pickup = spec->pickup;
    if (spec->ident_set)
        o_ptr->ident = spec->ident;
    if (spec->marked_set)
        o_ptr->marked = spec->marked;
    if (spec->xtra1_set)
        o_ptr->xtra1 = spec->xtra1;
    if (spec->abilities_set)
        o_ptr->abilities = spec->abilities;
    if (spec->unused1_set)
        o_ptr->unused1 = spec->unused1;
    if (spec->unused2_set)
        o_ptr->unused2 = spec->unused2;
    if (spec->unused3_set)
        o_ptr->unused3 = spec->unused3;
    if (spec->unused4_set)
        o_ptr->unused4 = spec->unused4;

    if ((spec->k_idx > 0) && (k_info[spec->k_idx].tval == TV_NOTE)
        && spec->tag_set)
    {
        int text_idx;

        if (spec->unused4_set)
        {
            msg_format(
                "Scenario '%s': tagged notes cannot also set unused4.",
                scenario.source);
            message_flush();
            return (FALSE);
        }

        text_idx = scenario_find_text(spec->tag);
        if (text_idx < 0)
        {
            msg_format(
                "Scenario '%s': note tag '%s' has no matching X: text block.",
                scenario.source, spec->tag);
            message_flush();
            return (FALSE);
        }

        scenario_note_set_text_idx(o_ptr, text_idx);
    }

    for (i = 0; i < 8; i++)
    {
        if (spec->ability_slot_set[i])
        {
            o_ptr->skilltype[i] = spec->skilltype[i];
            o_ptr->abilitynum[i] = spec->abilitynum[i];
        }
    }

    if (spec->known_set)
    {
        if (spec->known)
            object_known(o_ptr);
        else
            o_ptr->ident &= ~(IDENT_KNOWN);
    }
    else if (default_known)
    {
        object_known(o_ptr);
    }

    return (TRUE);
}

static void scenario_apply_monster_spec(
    monster_type* m_ptr, const scenario_monster_spec* spec)
{
    int i;

    if (spec->image_r_idx_set)
        m_ptr->image_r_idx = spec->image_r_idx;
    if (spec->hp_set)
        m_ptr->hp = spec->hp;
    if (spec->maxhp_set)
        m_ptr->maxhp = spec->maxhp;
    if (spec->alertness_set)
        m_ptr->alertness = spec->alertness;
    if (spec->skip_next_turn_set)
        m_ptr->skip_next_turn = spec->skip_next_turn;
    if (spec->mspeed_set)
        m_ptr->mspeed = spec->mspeed;
    if (spec->energy_set)
        m_ptr->energy = spec->energy;
    if (spec->stunned_set)
        m_ptr->stunned = spec->stunned;
    if (spec->confused_set)
        m_ptr->confused = spec->confused;
    if (spec->hasted_set)
        m_ptr->hasted = spec->hasted;
    if (spec->slowed_set)
        m_ptr->slowed = spec->slowed;
    if (spec->stance_set)
        m_ptr->stance = spec->stance;
    if (spec->morale_set)
        m_ptr->morale = spec->morale;
    if (spec->tmp_morale_set)
        m_ptr->tmp_morale = spec->tmp_morale;
    if (spec->noise_set)
        m_ptr->noise = spec->noise;
    if (spec->encountered_set)
        m_ptr->encountered = spec->encountered;
    if (spec->target_y_set)
        m_ptr->target_y = spec->target_y;
    if (spec->target_x_set)
        m_ptr->target_x = spec->target_x;
    if (spec->wandering_idx_set)
        m_ptr->wandering_idx = spec->wandering_idx;
    if (spec->wandering_dist_set)
        m_ptr->wandering_dist = spec->wandering_dist;
    if (spec->mana_set)
        m_ptr->mana = spec->mana;
    if (spec->song_set)
        m_ptr->song = spec->song;
    if (spec->skip_this_turn_set)
        m_ptr->skip_this_turn = spec->skip_this_turn;
    if (spec->consecutive_attacks_set)
        m_ptr->consecutive_attacks = spec->consecutive_attacks;
    if (spec->turns_stationary_set)
        m_ptr->turns_stationary = spec->turns_stationary;
    if (spec->mflag_set)
        m_ptr->mflag = spec->mflag;
    if (spec->home_wander_set)
    {
        if (spec->home_wander)
            m_ptr->mflag |= (MFLAG_HOME_WANDER);
        else
            m_ptr->mflag &= ~(MFLAG_HOME_WANDER);
    }
    if (spec->no_open_doors_set)
    {
        if (spec->no_open_doors)
            m_ptr->mflag |= (MFLAG_NO_OPEN_DOORS);
        else
            m_ptr->mflag &= ~(MFLAG_NO_OPEN_DOORS);
    }

    for (i = 0; i < ACTION_MAX; i++)
    {
        if (spec->previous_action_set[i])
            m_ptr->previous_action[i] = spec->previous_action[i];
    }
}

static bool scenario_add_starting_item(const scenario_object_spec* spec)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    int slot = spec->slot;

    if (!scenario_prepare_object(i_ptr, spec, TRUE))
        return (FALSE);

    if (slot == SCN_SLOT_PACK)
    {
        return (inven_carry(i_ptr, TRUE) >= 0);
    }

    if ((slot < 0) || (slot >= INVEN_TOTAL))
        return (FALSE);

    if (inventory[slot].k_idx)
        return (FALSE);

    object_copy(&inventory[slot], i_ptr);

    if ((slot < INVEN_PACK) && (p_ptr->inven_cnt < slot + 1))
        p_ptr->inven_cnt = slot + 1;
    else if (slot >= INVEN_WIELD)
        p_ptr->equip_cnt++;

    if ((slot >= INVEN_WIELD) && !((slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2)))
        inventory[slot].number = 1;

    return (TRUE);
}

static bool scenario_place_terrain(int y, int x, char ch)
{
    scenario_map* level = NULL;
    scenario_terrain_legend* legend;
    scenario_map_entity entity;

    cave_info[y][x] = 0;

    if ((scenario_active_map_idx >= 0)
        && (scenario_active_map_idx < scenario.level_count))
    {
        level = &scenario.levels[scenario_active_map_idx];
    }

    legend = scenario_find_terrain_legend_in(level, ch);

    if (legend)
    {
        if (legend->type == SCN_TERRAIN_FEATURE)
        {
            cave_set_feat(y, x, legend->feat);
        }
        else if (legend->type == SCN_TERRAIN_FLOOR)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
        }
        else if (legend->type == SCN_TERRAIN_PLACE)
        {
            cave_set_feat(y, x, FEAT_FLOOR);
        }
        else
        {
            return (FALSE);
        }

        cave_info[y][x]
            = (cave_info[y][x] & ~SCENARIO_CAVE_INFO_FLAGS)
            | (legend->info & SCENARIO_CAVE_INFO_FLAGS);

        if ((legend->type == SCN_TERRAIN_PLACE)
            && ((legend->chance >= 100) || (rand_int(100) < legend->chance)))
        {
            switch (legend->place)
            {
            case SCN_TERRAIN_PLACE_CLOSED_DOOR:
                place_closed_door(y, x);
                break;
            case SCN_TERRAIN_PLACE_TRAP:
                place_trap(y, x);
                break;
            case SCN_TERRAIN_PLACE_FORGE:
                place_forge(y, x);
                break;
            default:
                return (FALSE);
            }
        }

        if (legend->entity_set)
        {
            entity = legend->entity;
            entity.y = y;
            entity.x = x;
            if (!scenario_place_map_entity(&entity))
                return (FALSE);
        }

        return (TRUE);
    }

    cave_info[y][x] |= CAVE_ROOM;
    if (scenario.flags & SCN_FLAG_LIT)
        cave_info[y][x] |= CAVE_GLOW;

    switch (ch)
    {
    case '.':
        cave_set_feat(y, x, FEAT_FLOOR);
        break;
    case '#':
        cave_set_feat(y, x, FEAT_WALL_INNER);
        break;
    case '$':
        cave_set_feat(y, x, FEAT_WALL_OUTER);
        break;
    case '%':
        cave_set_feat(y, x, FEAT_QUARTZ);
        break;
    case ':':
        cave_set_feat(y, x, FEAT_RUBBLE);
        break;
    case ';':
        cave_set_feat(y, x, FEAT_GLYPH);
        break;
    case '>':
        cave_set_feat(y, x, FEAT_MORE);
        break;
    case '<':
        cave_set_feat(y, x, FEAT_LESS);
        break;
    case '+':
        cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
        break;
    case 's':
        cave_set_feat(y, x, FEAT_SECRET);
        break;
    case '^':
        cave_set_feat(y, x, FEAT_TRAP_HEAD);
        break;
    case '0':
        place_forge(y, x);
        break;
    case '7':
        cave_set_feat(y, x, FEAT_CHASM);
        break;
    case ',':
        cave_set_feat(y, x, FEAT_SUNLIGHT);
        break;
    default:
        return (FALSE);
    }

    return (TRUE);
}

/* Roll one authored generated depth, relative to the current level if needed. */
static int scenario_roll_generated_depth(const scenario_map_entity* entity)
{
    int min_depth;
    int max_depth;

    if (!entity)
        return (p_ptr ? p_ptr->depth : 0);

    min_depth = entity->min_depth;
    max_depth = entity->max_depth;

    if (entity->relative_depth && p_ptr)
    {
        min_depth += p_ptr->depth;
        max_depth += p_ptr->depth;
    }

    min_depth = MAX(0, min_depth);
    max_depth = MIN(MAX_DEPTH - 1, max_depth);

    if (min_depth > max_depth)
        min_depth = max_depth;

    if (min_depth == max_depth)
        return (min_depth);

    return (rand_range(min_depth, max_depth));
}

static bool scenario_player_already_placed(void)
{
    return in_bounds(p_ptr->py, p_ptr->px) && (cave_m_idx[p_ptr->py][p_ptr->px] == -1);
}

static bool scenario_place_player(int y, int x)
{
    if (scenario_player_already_placed())
        return ((p_ptr->py == y) && (p_ptr->px == x));

    return (player_place(y, x) != 0);
}

static bool scenario_place_floor_object(
    int y, int x, const scenario_object_spec* spec)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    object_type* o_ptr;
    s16b o_idx;

    if (!scenario_prepare_object(i_ptr, spec, FALSE))
        return (FALSE);

    o_idx = o_pop();
    if (!o_idx)
        return (FALSE);

    o_ptr = &o_list[o_idx];
    object_copy(o_ptr, i_ptr);
    o_ptr->iy = y;
    o_ptr->ix = x;
    o_ptr->held_m_idx = 0;
    o_ptr->next_o_idx = cave_o_idx[y][x];
    cave_o_idx[y][x] = o_idx;

    return (TRUE);
}

static bool scenario_place_monster(int y, int x, const scenario_monster_spec* spec)
{
    s16b m_idx;
    bool sleeping = spec->sleep_set ? spec->sleep : TRUE;

    if (!place_monster_one(y, x, spec->r_idx, sleeping, TRUE, NULL))
        return (FALSE);

    m_idx = cave_m_idx[y][x];
    if (m_idx <= 0)
        return (FALSE);

    scenario_apply_monster_spec(&mon_list[m_idx], spec);
    if (spec->tag_set)
        my_strcpy(scenario_monster_tags[m_idx], spec->tag,
            sizeof(scenario_monster_tags[m_idx]));
    update_mon(m_idx, TRUE);
    return (TRUE);
}

/* Place one sparse entity from a MAP block onto the live dungeon. */
static bool scenario_place_map_entity(const scenario_map_entity* entity)
{
    int saved_level;

    if (!entity)
        return (FALSE);

    if ((entity->chance < 100) && (rand_int(100) >= entity->chance))
        return (TRUE);

    switch (entity->kind)
    {
    case SCN_LEVEL_ENTITY_OBJECT:
        return (scenario_place_floor_object(entity->y, entity->x, &entity->object));

    case SCN_LEVEL_ENTITY_MONSTER:
        return (scenario_place_monster(entity->y, entity->x, &entity->monster));

    case SCN_LEVEL_ENTITY_GEN_OBJECT:
        saved_level = object_level;
        object_level = scenario_roll_generated_depth(entity);
        place_object(
            entity->y, entity->x, entity->good, FALSE, entity->drop_type);
        object_level = saved_level;
        return (TRUE);

    case SCN_LEVEL_ENTITY_GEN_MONSTER:
        saved_level = monster_level;
        monster_level = scenario_roll_generated_depth(entity);
        place_monster(entity->y, entity->x, TRUE, TRUE, TRUE);
        monster_level = saved_level;
        return (TRUE);

    default:
        return (FALSE);
    }
}

/*
 * Rebuild wandering-group flow centers from the placed monsters so stale
 * flow state never leaks in from an earlier level.
 */
static void scenario_init_wandering_flows(void)
{
    int i;
    bool initialized[MAX_WANDERING_GROUPS];

    for (i = 0; i < MAX_WANDERING_GROUPS; i++)
    {
        int idx = FLOW_WANDERING_HEAD + i;

        flow_center_y[idx] = 0;
        flow_center_x[idx] = 0;
        update_center_y[idx] = 0;
        update_center_x[idx] = 0;
        wandering_pause[idx] = 0;
        initialized[i] = FALSE;
    }

    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        int local_idx;

        if (!m_ptr->r_idx)
            continue;
        if ((m_ptr->wandering_idx < FLOW_WANDERING_HEAD)
            || (m_ptr->wandering_idx > FLOW_WANDERING_TAIL))
        {
            continue;
        }

        local_idx = m_ptr->wandering_idx - FLOW_WANDERING_HEAD;
        if (initialized[local_idx])
            continue;

        update_flow(m_ptr->fy, m_ptr->fx, m_ptr->wandering_idx);
        initialized[local_idx] = TRUE;
    }
}

/* Test whether the current event context satisfies one scenario rule. */
static bool scenario_rule_when_matches(
    const scenario_rule* rule, const scenario_event_context* context)
{
    int i;

    for (i = 0; i < rule->when_count; i++)
    {
        const scenario_rule_term* term = &rule->when[i];
        bool actual = FALSE;

        switch (term->kind)
        {
        case SCN_TERM_FLAG:
            actual = scenario_get_flag(term->flag_idx);
            break;
        case SCN_TERM_DEPTH:
            actual = (context->depth == term->value);
            break;
        case SCN_TERM_NEW_DEPTH:
            actual = (context->new_depth == term->value);
            break;
        case SCN_TERM_HAS_SILMARIL:
            actual = context->has_silmaril;
            break;
        case SCN_TERM_MONSTER_SLAIN:
            actual = scenario_monster_has_been_slain(term->value);
            break;
        case SCN_TERM_HAS_ITEM:
            actual = (scenario_count_item_kind(term->value) >= term->extra);
            break;
        case SCN_TERM_CHOSEN_OATH:
            actual = chosen_oath(term->value);
            break;
        case SCN_TERM_OATH_BROKEN:
            actual = oath_broken_current();
            break;
        case SCN_TERM_MONSTER_IS:
            actual = scenario_monster_matches(context, term->value);
            break;
        case SCN_TERM_MONSTER_VISIBLE:
            actual = context->monster_visible;
            break;
        default:
            return (FALSE);
        }

        if (actual != term->expected)
            return (FALSE);
    }

    return (TRUE);
}

/* Queue the effects from one matching scenario rule for later commit. */
static void scenario_apply_rule_actions(
    const scenario_rule* rule, scenario_rule_effects* effects)
{
    int i;

    for (i = 0; i < rule->action_count; i++)
    {
        const scenario_rule_action* action = &rule->actions[i];

        if (action->kind == SCN_ACTION_SET_FLAG)
        {
            if (effects->set_flag_count < (int)N_ELEMENTS(effects->set_flag_idx))
                effects->set_flag_idx[effects->set_flag_count++]
                    = action->flag_idx;
        }
        else if (action->kind == SCN_ACTION_CLEAR_FLAG)
        {
            if (effects->clear_flag_count
                < (int)N_ELEMENTS(effects->clear_flag_idx))
            {
                effects->clear_flag_idx[effects->clear_flag_count++]
                    = action->flag_idx;
            }
        }
        else if (action->kind == SCN_ACTION_DISPLAY_TEXT)
        {
            if (effects->text_count < (int)N_ELEMENTS(effects->text_idx))
                effects->text_idx[effects->text_count++] = action->text_idx;
        }
        else if (action->kind == SCN_ACTION_DISPLAY_MESSAGE)
        {
            if (effects->message_count < (int)N_ELEMENTS(effects->message_idx))
                effects->message_idx[effects->message_count++] = action->text_idx;
        }
        else if (action->kind == SCN_ACTION_ADD_NOTE)
        {
            if (effects->note_count < (int)N_ELEMENTS(effects->note_idx))
                effects->note_idx[effects->note_count++] = action->text_idx;
        }
        else if (action->kind == SCN_ACTION_CONFIRM_TEXT)
        {
            effects->prompt_text_idx = action->text_idx;
        }
        else if (action->kind == SCN_ACTION_SET_NEW_DEPTH)
        {
            effects->new_depth = action->value;
            effects->new_depth_set = TRUE;
        }
        else if (action->kind == SCN_ACTION_CREATE_STAIRS)
        {
            effects->create_stair = action->value;
            effects->create_stair_set = TRUE;
        }
        else if (action->kind == SCN_ACTION_DENY_ACTION)
        {
            effects->deny_action = TRUE;
        }
        else if (action->kind == SCN_ACTION_END_GAME)
        {
            effects->end_game = TRUE;
        }
        else if (action->kind == SCN_ACTION_BREAK_OATH)
        {
            effects->break_oath = TRUE;
        }
    }
}

/* Show one rule's queued message and text blocks before confirmation. */
static void scenario_show_rule_effects(const scenario_rule_effects* effects)
{
    int i;

    if (!effects)
        return;

    for (i = 0; i < effects->message_count; i++)
        scenario_show_message_block(effects->message_idx[i]);
    for (i = 0; i < effects->text_count; i++)
        scenario_show_text_block(effects->text_idx[i]);
}

/* Commit one rule's queued side effects after any confirmation succeeds. */
static void scenario_commit_rule_effects(
    const scenario_rule_effects* effects, scenario_event_result* result)
{
    int i;
    bool sync_player = FALSE;

    if (!effects || !result)
        return;

    for (i = 0; i < effects->set_flag_count; i++)
    {
        scenario_set_flag(effects->set_flag_idx[i], TRUE);
        sync_player = TRUE;
    }

    for (i = 0; i < effects->clear_flag_count; i++)
    {
        scenario_set_flag(effects->clear_flag_idx[i], FALSE);
        sync_player = TRUE;
    }

    if (sync_player)
        scenario_sync_player_state();

    if (effects->break_oath)
        (void)oath_break_current();

    for (i = 0; i < effects->note_count; i++)
        scenario_add_note_block(effects->note_idx[i]);

    if (effects->new_depth_set)
    {
        result->new_depth = effects->new_depth;
        result->new_depth_set = TRUE;
    }
    if (effects->create_stair_set)
    {
        result->create_stair = effects->create_stair;
        result->create_stair_set = TRUE;
    }
    if (effects->deny_action)
        result->deny_action = TRUE;
    if (effects->end_game)
        result->end_game = TRUE;
}

/* Show one named scenario text block using raw embedded line breaks. */
static void scenario_show_text_block(int text_idx)
{
    char ch;
    char line[160];
    int row = 5;
    int col = 10;
    int i = 0;
    cptr s;

    if ((text_idx < 0) || (text_idx >= scenario.text_count))
        return;

    s = scenario.texts[text_idx].text;
    if (!s[0])
        return;

    screen_save();
    Term_clear();

    while (*s)
    {
        int len = 0;

        while (s[len] && (s[len] != '\n') && (len < (int)sizeof(line) - 1))
            len++;

        memcpy(line, s, len);
        line[len] = '\0';
        c_put_str(TERM_WHITE, line, row + i, col);

        if (s[len] == '\n')
            s += len + 1;
        else
            s += len;

        i++;
    }

    Term_fresh();

    while (1)
    {
        hide_cursor = TRUE;
        ch = inkey();
        hide_cursor = FALSE;

        if (ch != EOF)
            break;

        message_flush();
    }

    screen_load();
}

/* Show one named scenario text block as normal message lines. */
static void scenario_show_message_block(int text_idx)
{
    cptr s;

    if ((text_idx < 0) || (text_idx >= scenario.text_count))
        return;

    s = scenario.texts[text_idx].text;
    if (!s[0])
        return;

    while (*s)
    {
        char line[160];
        int len = 0;

        while (s[len] && (s[len] != '\n') && (len < (int)sizeof(line) - 1))
            len++;

        memcpy(line, s, len);
        line[len] = '\0';
        msg_print(line);

        if (s[len] == '\n')
            s += len + 1;
        else
            s += len;
    }
}

/* Append one named scenario text block to the run notes chronicle. */
static void scenario_add_note_block(int text_idx)
{
    cptr s;

    if ((text_idx < 0) || (text_idx >= scenario.text_count))
        return;

    s = scenario.texts[text_idx].text;
    if (!s[0])
        return;

    while (*s)
    {
        char line[160];
        int len = 0;

        while (s[len] && (s[len] != '\n') && (len < (int)sizeof(line) - 1))
            len++;

        memcpy(line, s, len);
        line[len] = '\0';
        do_cmd_note(line, p_ptr->depth);

        if (s[len] == '\n')
            s += len + 1;
        else
            s += len;
    }
}

/* Flatten one scenario text block into a one-line confirmation prompt. */
static cptr scenario_prompt_text(int text_idx, char* buf, size_t len)
{
    cptr s;
    size_t used = 0;

    if (!buf || !len)
        return ("");
    buf[0] = '\0';

    if ((text_idx < 0) || (text_idx >= scenario.text_count))
        return (buf);

    s = scenario.texts[text_idx].text;
    while (*s && (used + 1 < len))
    {
        char ch = *s++;

        if (ch == '\n')
            ch = ' ';

        if ((used > 0) && (buf[used - 1] == ' ') && (ch == ' '))
            continue;

        buf[used++] = ch;
    }

    buf[used] = '\0';
    return (buf);
}

/* Decide whether a tagged rule applies to the current event context. */
static bool scenario_rule_tag_matches(
    const scenario_rule* rule, const scenario_event_context* context)
{
    if (!rule->tag_set)
        return (TRUE);

    switch (context->event)
    {
    case SCN_EVENT_SEE_MONSTER:
    case SCN_EVENT_ATTACK_MONSTER:
    case SCN_EVENT_DAMAGE_MONSTER:
        if ((context->monster_idx <= 0) || (context->monster_idx >= MAX_MONSTERS))
            return (FALSE);
        return (!my_stricmp(rule->tag, scenario_monster_tags[context->monster_idx]));

    case SCN_EVENT_USE_EXIT:
    case SCN_EVENT_USE_UP_EXIT:
    case SCN_EVENT_USE_DOWN_EXIT:
        return (scenario_grid_has_tag(context->y, context->x, rule->tag));

    default:
        return (FALSE);
    }
}

/* Evaluate all rules that match one scenario event context. */
static scenario_event_result scenario_handle_event(
    const scenario_event_context* context)
{
    scenario_event_result result;
    scenario_rule_effects effects;
    int i;
    char prompt[256];

    WIPE(&result, scenario_event_result);

    if (!scenario.loaded || !context)
        return (result);

    for (i = 0; i < scenario.rule_count; i++)
    {
        scenario_rule* rule = &scenario.rules[i];

        if (rule->event != context->event)
            continue;
        if (!scenario_rule_tag_matches(rule, context))
            continue;
        if (!scenario_rule_when_matches(rule, context))
            continue;

        WIPE(&effects, scenario_rule_effects);
        effects.prompt_text_idx = -1;

        scenario_apply_rule_actions(rule, &effects);
        scenario_show_rule_effects(&effects);

        if ((effects.prompt_text_idx >= 0)
            && !get_check(scenario_prompt_text(
                effects.prompt_text_idx, prompt, sizeof(prompt))))
        {
            result.deny_action = TRUE;
            break;
        }

        scenario_commit_rule_effects(&effects, &result);
    }

    return (result);
}

/* Apply one terminal END_GAME result to the current run immediately. */
static bool scenario_finish_game_if_needed(const scenario_event_result* result)
{
    if (!result || !result->end_game)
        return (FALSE);

    p_ptr->is_dead = TRUE;
    p_ptr->energy_use = 100;
    p_ptr->leaving = TRUE;
    close_game();
    return (TRUE);
}

/* Seed the shared parts of one scenario event context from the live player. */
static void scenario_prepare_event_context(
    scenario_event_context* context, int event)
{
    if (!context)
        return;

    WIPE(context, scenario_event_context);
    context->event = (byte)event;

    if (!p_ptr)
        return;

    context->y = p_ptr->py;
    context->x = p_ptr->px;
    context->depth = p_ptr->depth;
    context->new_depth = p_ptr->depth;
    context->has_silmaril = (silmarils_possessed() > 0);
}

/* Trigger any ENTER_LEVEL rules for the current scenario. */
void scenario_handle_enter_level(void)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !p_ptr)
        return;

    scenario_prepare_event_context(&context, SCN_EVENT_ENTER_LEVEL);

    result = scenario_handle_event(&context);
    (void)scenario_finish_game_if_needed(&result);
}

/* Check whether one authored grid tag applies at the given map square. */
static bool scenario_grid_has_tag(int y, int x, cptr tag)
{
    int i;
    scenario_grid_tag* tags;
    int tag_count;
    scenario_map* level;

    if (!tag || !tag[0])
        return (FALSE);

    if ((scenario_active_map_idx < 0) || (scenario_active_map_idx >= scenario.level_count)
        || !p_ptr)
        return (FALSE);

    level = &scenario.levels[scenario_active_map_idx];
    if (level->depth != p_ptr->depth)
        return (FALSE);

    tags = level->grid_tags;
    tag_count = level->grid_tag_count;

    for (i = 0; i < tag_count; i++)
    {
        scenario_grid_tag* grid_tag = &tags[i];

        if ((grid_tag->y != y) || (grid_tag->x != x))
            continue;
        if (!my_stricmp(grid_tag->tag, tag))
            return (TRUE);
    }

    return (FALSE);
}

/* Trigger all SEE_MONSTER rules for one authored monster instance. */
void scenario_handle_monster_seen(monster_type* m_ptr)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !m_ptr || !m_ptr->r_idx)
        return;

    scenario_prepare_event_context(&context, SCN_EVENT_SEE_MONSTER);
    context.monster_idx = (int)(m_ptr - mon_list);
    context.monster_visible = m_ptr->ml ? TRUE : FALSE;

    result = scenario_handle_event(&context);
    (void)scenario_finish_game_if_needed(&result);
}

/* Trigger generic USE_EXIT rules for the current grid. */
bool scenario_handle_use_exit(int y, int x)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !p_ptr)
        return (FALSE);

    scenario_prepare_event_context(&context, SCN_EVENT_USE_EXIT);
    context.y = y;
    context.x = x;

    result = scenario_handle_event(&context);
    if (scenario_finish_game_if_needed(&result))
        return (TRUE);

    return (result.deny_action);
}

/* Trigger any USE_UP_EXIT rules for the current scenario. */
bool scenario_handle_use_up_exit(void)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !p_ptr)
        return (FALSE);

    scenario_prepare_event_context(&context, SCN_EVENT_USE_UP_EXIT);
    context.new_depth = p_ptr->depth - 1;

    result = scenario_handle_event(&context);
    if (scenario_finish_game_if_needed(&result))
        return (TRUE);
    if (!result.deny_action && result.create_stair_set)
        p_ptr->create_stair = result.create_stair;
    if (result.deny_action)
    {
        p_ptr->create_stair = FALSE;
        p_ptr->energy_use = 100;
    }

    return (result.deny_action);
}

/* Trigger any USE_DOWN_EXIT rules for the current scenario. */
bool scenario_handle_use_down_exit(int* new_depth)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !p_ptr)
        return (FALSE);

    scenario_prepare_event_context(&context, SCN_EVENT_USE_DOWN_EXIT);
    context.new_depth = new_depth ? *new_depth : p_ptr->depth + 1;

    result = scenario_handle_event(&context);
    if (scenario_finish_game_if_needed(&result))
        return (TRUE);
    if (!result.deny_action)
    {
        if (result.new_depth_set && new_depth)
            *new_depth = result.new_depth;
        if (result.create_stair_set)
            p_ptr->create_stair = result.create_stair;
    }
    if (result.deny_action)
        p_ptr->create_stair = FALSE;

    return (result.deny_action);
}

/* Trigger DEATH rules for the current scenario without altering death flow. */
void scenario_handle_death(void)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded)
        return;

    scenario_prepare_event_context(&context, SCN_EVENT_DEATH);

    result = scenario_handle_event(&context);
    (void)scenario_finish_game_if_needed(&result);
}

/* Trigger ESCAPE rules for the current scenario during the escape epilogue. */
void scenario_handle_escape(void)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded)
        return;

    scenario_prepare_event_context(&context, SCN_EVENT_ESCAPE);

    result = scenario_handle_event(&context);
    (void)scenario_finish_game_if_needed(&result);
}

/* Trigger START_SONG rules before the player begins singing. */
bool scenario_handle_start_song(void)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !p_ptr)
        return (FALSE);

    scenario_prepare_event_context(&context, SCN_EVENT_START_SONG);
    result = scenario_handle_event(&context);
    if (scenario_finish_game_if_needed(&result))
        return (TRUE);

    return (result.deny_action);
}

/* Return whether the active intact oath would break on START_SONG right now. */
bool scenario_current_oath_breaks_on_start_song(void)
{
    scenario_event_context context;
    int i;

    if (!scenario.loaded || !p_ptr || (p_ptr->oath_type <= 0)
        || oath_invalid(p_ptr->oath_type))
    {
        return (FALSE);
    }

    scenario_prepare_event_context(&context, SCN_EVENT_START_SONG);

    for (i = 0; i < scenario.rule_count; i++)
    {
        int j;
        scenario_rule* rule = &scenario.rules[i];

        if (rule->event != SCN_EVENT_START_SONG)
            continue;
        if (!scenario_rule_tag_matches(rule, &context))
            continue;
        if (!scenario_rule_when_matches(rule, &context))
            continue;

        for (j = 0; j < rule->action_count; j++)
        {
            if (rule->actions[j].kind == SCN_ACTION_BREAK_OATH)
                return (TRUE);
        }
    }

    return (FALSE);
}

/* Return whether the active intact oath would warn or break on one attack. */
bool scenario_current_oath_warns_on_attack(monster_type* m_ptr)
{
    scenario_event_context context;
    int i;

    if (!scenario.loaded || !p_ptr || (p_ptr->oath_type <= 0)
        || oath_invalid(p_ptr->oath_type) || !m_ptr || !m_ptr->r_idx)
    {
        return (FALSE);
    }

    scenario_prepare_event_context(&context, SCN_EVENT_ATTACK_MONSTER);
    context.monster_idx = (int)(m_ptr - mon_list);
    context.monster_visible = m_ptr->ml ? TRUE : FALSE;

    for (i = 0; i < scenario.rule_count; i++)
    {
        int j;
        scenario_rule* rule = &scenario.rules[i];

        if (rule->event != SCN_EVENT_ATTACK_MONSTER)
            continue;
        if (!scenario_rule_tag_matches(rule, &context))
            continue;
        if (!scenario_rule_when_matches(rule, &context))
            continue;

        for (j = 0; j < rule->action_count; j++)
        {
            if ((rule->actions[j].kind == SCN_ACTION_CONFIRM_TEXT)
                || (rule->actions[j].kind == SCN_ACTION_BREAK_OATH))
            {
                return (TRUE);
            }
        }
    }

    return (FALSE);
}

/* Trigger ATTACK_MONSTER rules before resolving one melee attack. */
bool scenario_handle_attack_monster(monster_type* m_ptr)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !m_ptr || !m_ptr->r_idx)
        return (FALSE);

    scenario_prepare_event_context(&context, SCN_EVENT_ATTACK_MONSTER);
    context.monster_idx = (int)(m_ptr - mon_list);
    context.monster_visible = m_ptr->ml ? TRUE : FALSE;

    result = scenario_handle_event(&context);
    if (scenario_finish_game_if_needed(&result))
        return (TRUE);

    return (result.deny_action);
}

/* Trigger DAMAGE_MONSTER rules after one visible monster takes damage. */
void scenario_handle_damage_monster(monster_type* m_ptr, int damage)
{
    scenario_event_context context;
    scenario_event_result result;

    if (!scenario.loaded || !m_ptr || !m_ptr->r_idx || (damage <= 0))
        return;

    scenario_prepare_event_context(&context, SCN_EVENT_DAMAGE_MONSTER);
    context.monster_idx = (int)(m_ptr - mon_list);
    context.monster_visible = m_ptr->ml ? TRUE : FALSE;

    result = scenario_handle_event(&context);
    (void)scenario_finish_game_if_needed(&result);
}

/* Mirror an external truce change back into the current scenario flags. */
void scenario_note_truce(bool active)
{
    if (!scenario.loaded || (scenario_truce_flag_idx < 0))
        return;

    scenario_set_flag(scenario_truce_flag_idx, active);
    scenario_sync_player_state();
}

/*
 * Clear any loaded scenario so a later start or load begins from a clean
 * pending state.
 */
void scenario_clear_pending(void)
{
    WIPE(&scenario, scenario_state);
    memset(scenario_monster_tags, 0, sizeof(scenario_monster_tags));
    oath_clear_catalog();
    scenario_active_map_idx = -1;
    scenario_truce_flag_idx = -1;
    scenario_escape_flag_idx = -1;
    scenario_error_line = 0;
    scenario_error[0] = '\0';
    scenario_warning_line = 0;
    scenario_warning_count = 0;
    scenario_warning[0] = '\0';
}

/* Parse a scenario definition file into the pending in-memory state. */
bool scenario_prepare_pending(cptr filename)
{
    char path[1024];
    FILE* fff;
    char buf[512];
    int line = 0;
    scenario_map* current_map = NULL;

    scenario_clear_pending();

    path_build(path, sizeof(path), ANGBAND_DIR_EDIT, filename);
    fff = my_fopen(path, "r");
    if (!fff)
    {
        msg_format("Cannot open scenario file '%s'.", filename);
        message_flush();
        return (FALSE);
    }

    my_strcpy(scenario.source, filename, sizeof(scenario.source));

    while (0 == my_fgets(fff, buf, sizeof(buf)))
    {
        char* s;

        line++;
        scenario_rstrip(buf);
        s = scenario_trim(buf);

        if (!s[0] || (s[0] == '#'))
            continue;

        if (!my_strnicmp(s, "V:", 2))
        {
            long version;

            if (!scenario_parse_int(scenario_trim(s + 2), &version)
                || (version != SCENARIO_VERSION))
            {
                scenario_fail(line, "Unsupported scenario version.");
                break;
            }
        }
        else if (!my_strnicmp(s, "MAP:", 4))
        {
            current_map = scenario_add_map(scenario_trim(s + 4), line);
            if (!current_map)
                break;
        }
        else if (!my_strnicmp(s, "MAP_DEPTH:", 10))
        {
            if (!scenario_parse_map_depth(current_map, scenario_trim(s + 10), line))
                break;
        }
        else if (!my_strnicmp(s, "MAP_FLAGS:", 10))
        {
            if (!scenario_parse_map_flags(current_map, scenario_trim(s + 10),
                    line))
            {
                break;
            }
        }
        else if (!my_strnicmp(s, "MAP_SIZE:", 9))
        {
            if (!scenario_parse_map_size(current_map, scenario_trim(s + 9),
                    line))
            {
                break;
            }
        }
        else if (!my_strnicmp(s, "MAP_ORIGIN:", 11))
        {
            if (!scenario_parse_map_origin(current_map, scenario_trim(s + 11), line))
            {
                break;
            }
        }
        else if (!my_strnicmp(s, "F:", 2))
        {
            if (!scenario_parse_flag_line(scenario_trim(s + 2), line))
                break;
        }
        else if (!my_strnicmp(s, "G:", 2))
        {
            char local[256];
            char* parts[4];
            int count;
            long value;
            scenario_grid_tag* entry;

            if (!current_map)
            {
                scenario_fail(line, "G: requires an active MAP block.");
                break;
            }

            if (current_map->grid_tag_count >= SCENARIO_MAX_GRID_TAGS)
            {
                scenario_fail(line, "Too many tagged grids.");
                break;
            }

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 4);
            if (count < 3)
            {
                scenario_fail(line, "Malformed tagged grid directive.");
                break;
            }

            entry = &current_map->grid_tags[current_map->grid_tag_count++];
            WIPE(entry, scenario_grid_tag);

            if (!scenario_parse_int(parts[0], &value))
            {
                scenario_fail(line, "Invalid tagged grid y coordinate.");
                break;
            }
            entry->y = (int)value;

            if (!scenario_parse_int(parts[1], &value))
            {
                scenario_fail(line, "Invalid tagged grid x coordinate.");
                break;
            }
            entry->x = (int)value;

            my_strcpy(entry->tag, scenario_trim(parts[2]), sizeof(entry->tag));
            if (!entry->tag[0])
            {
                scenario_fail(line, "Tagged grid name cannot be empty.");
                break;
            }
        }
        else if (!my_strnicmp(s, "T:", 2))
        {
            if (!current_map)
            {
                scenario_fail(line, "T: requires an active MAP block.");
                break;
            }
            if (!scenario_parse_terrain_legend(
                    current_map, scenario_trim(s + 2), line))
                break;
        }
        else if (!my_strnicmp(s, "P:", 2))
        {
            char local[512];
            char* parts[5];
            int count;
            long value;
            int idx;

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 5);

            if (count < 2)
            {
                scenario_fail(line, "Malformed player directive.");
                break;
            }

            parts[0] = scenario_trim(parts[0]);
            parts[1] = scenario_trim(parts[1]);

            if (!my_stricmp(parts[0], "RACE"))
            {
                idx = scenario_lookup_race(parts[1]);
                if (idx < 0)
                {
                    scenario_fail(line, "Unknown player race.");
                    break;
                }
                scenario.prace = (byte)idx;
                scenario.prace_set = TRUE;
            }
            else if (!my_stricmp(parts[0], "HOUSE"))
            {
                idx = scenario_lookup_house(parts[1]);
                if (idx < 0)
                {
                    scenario_fail(line, "Unknown player house.");
                    break;
                }
                scenario.phouse = (byte)idx;
                scenario.phouse_set = TRUE;
            }
            else if (!my_stricmp(parts[0], "NAME"))
            {
                my_strcpy(
                    scenario.full_name, parts[1], sizeof(scenario.full_name));
                scenario.full_name_set = TRUE;
            }
            else if (!my_stricmp(parts[0], "HISTORY"))
            {
                if (scenario.history[0])
                    my_strcat(
                        scenario.history, " ", sizeof(scenario.history));
                my_strcat(
                    scenario.history, parts[1], sizeof(scenario.history));
            }
            else if (!my_stricmp(parts[0], "STAT"))
            {
                if (count < 3)
                {
                    scenario_fail(line, "STAT needs a name and value.");
                    break;
                }

                idx = scenario_lookup_stat(parts[1]);
                if ((idx < 0)
                    || !scenario_parse_int(scenario_trim(parts[2]), &value))
                {
                    scenario_fail(line, "Invalid stat definition.");
                    break;
                }

                scenario.stat_base[idx] = (s16b)value;
                scenario.stat_set[idx] = TRUE;
            }
            else if (!my_stricmp(parts[0], "SKILL"))
            {
                if (count < 3)
                {
                    scenario_fail(line, "SKILL needs a name and value.");
                    break;
                }

                idx = scenario_lookup_skill(parts[1]);
                if ((idx < 0)
                    || !scenario_parse_int(scenario_trim(parts[2]), &value))
                {
                    scenario_fail(line, "Invalid skill definition.");
                    break;
                }

                scenario.skill_base[idx] = (s16b)value;
                scenario.skill_set[idx] = TRUE;
            }
            else if (!my_stricmp(parts[0], "ABILITY"))
            {
                bool innate = TRUE;
                bool active = TRUE;
                int ability;
                int i;

                if (count < 3)
                {
                    scenario_fail(line, "ABILITY needs a skill and name.");
                    break;
                }

                idx = scenario_lookup_skill(parts[1]);
                ability = scenario_lookup_ability(idx, scenario_trim(parts[2]));
                if ((idx < 0) || (ability < 0))
                {
                    scenario_fail(line, "Unknown player ability.");
                    break;
                }

                for (i = 3; i < count; i++)
                {
                    if (!scenario_parse_player_ability_modifier(
                            &innate, &active, scenario_trim(parts[i]), line))
                        break;
                }
                if (i < count)
                    break;

                scenario.innate_ability[idx][ability] = innate;
                scenario.active_ability[idx][ability] = active;
            }
            else if (!my_stricmp(parts[0], "POS"))
            {
                scenario_fail(
                    line,
                    "P:POS is obsolete; define E:<y>:<x>:PLAYER inside a MAP block.");
                break;
            }
            else
            {
                if (!scenario_parse_int(parts[1], &value))
                {
                    scenario_fail(line, "Invalid numeric player value.");
                    break;
                }

                if (!my_stricmp(parts[0], "AGE"))
                {
                    scenario.age = (s16b)value;
                    scenario.age_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "HEIGHT"))
                {
                    scenario.ht = (s16b)value;
                    scenario.ht_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "WEIGHT"))
                {
                    scenario.wt = (s16b)value;
                    scenario.wt_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "DEPTH"))
                {
                    scenario.depth = (s16b)value;
                    scenario.depth_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "MAX_DEPTH"))
                {
                    scenario.max_depth = (s16b)value;
                    scenario.max_depth_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "EXP"))
                {
                    scenario.exp = (s32b)value;
                    scenario.exp_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "NEW_EXP"))
                {
                    scenario.new_exp = (s32b)value;
                    scenario.new_exp_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "FOOD"))
                {
                    scenario.food = (s16b)value;
                    scenario.food_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "HP"))
                {
                    scenario.chp = (s16b)value;
                    scenario.chp_set = TRUE;
                }
                else if (!my_stricmp(parts[0], "VOICE"))
                {
                    scenario.csp = (s16b)value;
                    scenario.csp_set = TRUE;
                }
                else
                {
                    scenario_fail(line, "Unknown player directive.");
                    break;
                }
            }
        }
        else if (!my_strnicmp(s, "I:", 2))
        {
            char local[512];
            char* parts[48];
            int count;
            int i;
            int slot;
            int k_idx;
            scenario_object_spec* spec;

            if (scenario.item_count >= SCENARIO_MAX_ITEMS)
            {
                scenario_fail(line, "Too many starting items.");
                break;
            }

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 48);
            if (count < 2)
            {
                scenario_fail(line, "Malformed item directive.");
                break;
            }

            slot = scenario_lookup_slot(scenario_trim(parts[0]));
            if (slot < 0)
            {
                scenario_fail(line, "Unknown item slot.");
                break;
            }

            k_idx = scenario_lookup_object(scenario_trim(parts[1]));
            if (k_idx <= 0)
            {
                scenario_fail(line, "Unknown or ambiguous object kind.");
                break;
            }

            spec = &scenario.items[scenario.item_count++];
            WIPE(spec, scenario_object_spec);
            spec->slot = (byte)slot;
            spec->k_idx = (s16b)k_idx;

            for (i = 2; i < count; i++)
            {
                if (!scenario_parse_object_modifier(
                        spec, scenario_trim(parts[i]), line))
                    break;
            }
            if (i < count)
                break;
        }
        else if (!my_strnicmp(s, "M:", 2))
        {
            if (!current_map)
            {
                scenario_fail(line, "M: requires an active MAP block.");
                break;
            }
            if (!scenario_append_row(current_map->terrain, &current_map->terrain_rows,
                    &current_map->width, s + 2, line))
                break;
        }
        else if (!my_strnicmp(s, "X:", 2))
        {
            char local[512];
            char* split;

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            split = strchr(local, ':');
            if (!split)
            {
                scenario_fail(line, "Malformed text block directive.");
                break;
            }

            *split++ = '\0';
            if (!scenario_append_text_line(
                    scenario_trim(local), split, line))
            {
                break;
            }
        }
        else if (!my_strnicmp(s, "OATH:", 5))
        {
            if (!scenario_parse_oath(scenario_trim(s + 5), line))
                break;
        }
        else if (!my_strnicmp(s, "E:", 2))
        {
            if (!current_map)
            {
                scenario_fail(line, "E: requires an active MAP block.");
                break;
            }
            if (!scenario_parse_map_entity(current_map,
                    scenario_trim(s + 2), line))
            {
                break;
            }
        }
        else if (!my_strnicmp(s, "O:", 2) || !my_strnicmp(s, "N:", 2)
            || !my_strnicmp(s, "L:", 2))
        {
            scenario_fail(
                line, "O:/N:/L: are obsolete; use MAP-local E: entries instead.");
            break;
        }
        else if (!my_strnicmp(s, "R:", 2))
        {
            if (!scenario_parse_rule(scenario_trim(s + 2), line))
                break;
        }
        else
        {
            scenario_warn(line, "Ignored unknown scenario directive.");
        }
    }

    my_fclose(fff);

    if (!scenario_error[0] && !scenario_validate())
    {
        /* Validation stores the error. */
    }
    if (!scenario_error[0] && !scenario_activate_oaths())
    {
        /* Activation stores the error. */
    }

    if (scenario_error[0])
    {
        if (scenario_error_line > 0)
            msg_format("Scenario '%s' line %d: %s", filename, scenario_error_line,
                scenario_error);
        else
            msg_format("Scenario '%s': %s", filename, scenario_error);

        message_flush();
        scenario_clear_pending();
        return (FALSE);
    }

    if (scenario_warning_count > 0)
    {
        msg_format("Scenario '%s': ignored %d unknown directive line%s; first at line %d.",
            filename, scenario_warning_count,
            (scenario_warning_count == 1) ? "" : "s", scenario_warning_line);
    }

    scenario_truce_flag_idx = scenario_find_flag("truce_active");
    scenario_escape_flag_idx = scenario_find_flag("escape_phase");
    scenario.loaded = TRUE;
    scenario_active_map_idx = -1;
    if (p_ptr)
    {
        scenario_map* level = scenario_find_map(p_ptr->depth);
        int i;

        for (i = 0; level && (i < scenario.level_count); i++)
        {
            if (&scenario.levels[i] == level)
            {
                scenario_active_map_idx = i;
                break;
            }
        }
    }
    scenario_seed_reserved_flags();
    return (TRUE);
}

/* Apply mapless scenario overrides after a normal birth-backed start. */
static bool scenario_apply_post_birth_overrides(void)
{
    int i;
    int j;

    p_ptr->unused3 = 0;

    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = scenario.innate_ability[i][j];
            p_ptr->active_ability[i][j] = scenario.active_ability[i][j];
        }
    }

    p_ptr->depth = scenario.depth_set ? scenario.depth : 1;
    if (scenario.max_depth_set)
        p_ptr->max_depth = scenario.max_depth;
    else if (p_ptr->max_depth < p_ptr->depth)
        p_ptr->max_depth = p_ptr->depth;
    if (scenario.exp_set)
        p_ptr->exp = scenario.exp;
    if (scenario.new_exp_set)
        p_ptr->new_exp = scenario.new_exp;
    else if (p_ptr->new_exp > p_ptr->exp)
        p_ptr->new_exp = p_ptr->exp;
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_duration = 0;
    if (scenario.food_set)
        p_ptr->food = scenario.food;

    for (i = 0; i < scenario.item_count; i++)
    {
        if (!scenario_add_starting_item(&scenario.items[i]))
        {
            msg_format("Scenario '%s': failed to grant starting item %d.",
                scenario.source, i + 1);
            message_flush();
            return (FALSE);
        }
    }

    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);
    update_stuff();

    if (scenario.chp_set)
    {
        p_ptr->chp = MIN(scenario.chp, p_ptr->mhp);
        p_ptr->chp_frac = 0;
    }

    if (scenario.csp_set)
    {
        p_ptr->csp = MIN(scenario.csp, p_ptr->msp);
        p_ptr->csp_frac = 0;
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_BASIC | PR_MISC | PR_EQUIPPY | PR_RESIST | PR_EXP);

    scenario_sync_player_state();
    return (TRUE);
}

/* Start a new game from the pending scenario through the normal birth flow. */
bool scenario_start_pending_new_game(void)
{
    s16b game_type;
    s16b quest_id;

    if (!scenario.loaded)
        return (FALSE);

    game_type = p_ptr->game_type;
    quest_id = p_ptr->unused2;

    player_birth();
    p_ptr->game_type = game_type;
    p_ptr->unused2 = quest_id;
    return (scenario_apply_post_birth_overrides());
}

bool scenario_pending_map_generation(void)
{
    if (!scenario.loaded || !p_ptr)
        return (FALSE);

    return (scenario_find_map(p_ptr->depth) != NULL);
}

/*
 * Materialize one authored MAP block, then place the player and sparse
 * entities on top of it.
 */
bool scenario_generate_pending_map(void)
{
    int map_height;
    int map_width;
    int origin_y;
    int origin_x;
    int y;
    int i;
    scenario_map* level;

    if (!scenario_pending_map_generation())
        return (FALSE);

    level = scenario_find_map(p_ptr->depth);
    if (!level)
        return (FALSE);

    scenario_active_map_idx = -1;
    for (i = 0; i < scenario.level_count; i++)
    {
        if (&scenario.levels[i] == level)
        {
            scenario_active_map_idx = i;
            break;
        }
    }

    map_height = scenario_map_height(level);
    map_width = scenario_map_width(level);
    origin_y = level->origin_set ? level->origin_y : 0;
    origin_x = level->origin_set ? level->origin_x : 0;

    memset(scenario_monster_tags, 0, sizeof(scenario_monster_tags));

    p_ptr->cur_map_hgt = (byte)map_height;
    p_ptr->cur_map_wid = (byte)map_width;

    if (level->flags & SCN_LEVEL_FLAG_GOOD_ITEM)
        good_item_flag = TRUE;

    if (level->map_size_set)
    {
        int x;

        for (y = 0; y < map_height; y++)
        {
            for (x = 0; x < map_width; x++)
            {
                cave_info[y][x] = 0;
                cave_set_feat(y, x, FEAT_WALL_EXTRA);
            }
        }

        for (y = 0; y < map_height; y++)
        {
            cave_set_feat(y, 0, FEAT_WALL_PERM);
            cave_set_feat(y, map_width - 1, FEAT_WALL_PERM);
        }

        for (x = 0; x < map_width; x++)
        {
            cave_set_feat(0, x, FEAT_WALL_PERM);
            cave_set_feat(map_height - 1, x, FEAT_WALL_PERM);
        }
    }

    for (y = 0; y < level->height; y++)
    {
        int x;

        for (x = 0; x < level->width; x++)
        {
            int target_y = origin_y + y;
            int target_x = origin_x + x;
            char symbol = level->terrain[y][x];

            if (symbol == ' ')
                continue;

            if (!scenario_place_terrain(target_y, target_x, symbol))
                return (FALSE);
        }
    }

    if (!scenario_place_player(level->player_y, level->player_x))
        return (FALSE);

    for (y = 0; y < level->entity_count; y++)
    {
        if (!scenario_place_map_entity(&level->entities[y]))
            return (FALSE);
    }

    scenario_init_wandering_flows();
    return (TRUE);
}

static void scenario_export_header(FILE* fff, cptr filename)
{
    cptr basename = strrchr(filename, '/');

    if (basename)
        basename++;
    else
        basename = filename;

    fprintf(fff, "# File: %s\n", basename);
    fprintf(fff, "#\n");
    fprintf(fff, "# Generated from the current live game state. Edit by hand if you want,\n");
    fprintf(fff, "# or regenerate it with the scenario exporter to refresh this MAP block.\n");
    fprintf(fff, "#\n");
    fprintf(fff, "# Quick legend:\n");
    fprintf(fff, "# V:<n> format version\n");
    fprintf(fff, "# P:<field>:<value> player state or birth preset data\n");
    fprintf(fff, "# P:ABILITY:<skill>:<ability>[:INNATE=0][:ACTIVE=0] player ability state\n");
    fprintf(fff, "# I:<slot>:<object>[:key=value...] starting inventory or equipment\n");
    fprintf(fff, "# MAP:<name> begin one authored map block with metadata\n");
    fprintf(fff, "# MAP_DEPTH:<n> exact dungeon depth where that map is used\n");
    fprintf(fff, "# MAP_FLAGS:<flag[,flag...]> extra generation hints for that map\n");
    fprintf(fff, "# MAP_SIZE:<h>:<w> full generated map size for a compact MAP block\n");
    fprintf(fff, "# MAP_ORIGIN:<y>:<x> top-left stamp position of its M: rows\n");
    fprintf(fff, "# T:<symbol>:FEATURE|PLACE|FLOOR:... tile legend used by M: rows\n");
    fprintf(fff, "#   FLOOR tiles can add MONSTER=..., OBJECT=..., GEN_MONSTER=...,\n");
    fprintf(fff, "#   GEN_OBJECT=..., GOOD=1, or DROP=CHEST while still starting from floor\n");
    fprintf(fff, "# M:<symbols...> one map row using the T: legend above\n");
    fprintf(fff, "# E:<y>:<x>:PLAYER|MONSTER|OBJECT|GEN_MONSTER|GEN_OBJECT:... sparse\n");
    fprintf(fff, "#   entity used inside MAP blocks for player starts and exceptions\n");
    fprintf(fff, "# G:<y>:<x>:<tag> tag one grid for rule matching\n");
    fprintf(fff, "# X:<text-id>:<line> named multiline text block (repeatable)\n");
    fprintf(fff, "# OATH:<name>:VOW|RESTRICTION|REWARD_STAT:<value>\n");
    fprintf(fff, "# R:<event>[:tag=<tag>][:when=<term,!term...>][:ACTION...] event rule\n");
    fprintf(fff, "# Events include ENTER_LEVEL, USE_EXIT, USE_UP_EXIT, USE_DOWN_EXIT,\n");
    fprintf(fff, "# SEE_MONSTER, START_SONG, ATTACK_MONSTER, DAMAGE_MONSTER,\n");
    fprintf(fff, "# ESCAPE, and DEATH.\n");
    fprintf(fff, "# Common when= terms include depth=<n>, new_depth=<n>,\n");
    fprintf(fff, "# has_item(<object>[,<amount>]), has_silmaril(),\n");
    fprintf(fff, "# monster_slain(<monster>), chosen_oath(<name>|None),\n");
    fprintf(fff, "# oath_broken(), monster_is(<selector>), monster_visible(),\n");
    fprintf(fff, "# and named quest flags.\n");
    fprintf(fff, "# Rule actions include SET_FLAG, CLEAR_FLAG, DISPLAY_TEXT,\n");
    fprintf(fff, "# DISPLAY_MESSAGE, ADD_NOTE, CONFIRM_TEXT, BREAK_OATH,\n");
    fprintf(fff, "# DENY_ACTION, END_GAME,\n");
    fprintf(fff, "# SET_NEW_DEPTH=<n>, and CREATE_STAIRS=<stair|none>.\n");
    fprintf(fff, "# Map rows are optional: without authored MAP blocks, the\n");
    fprintf(fff, "# scenario uses normal birth and procedural level generation while still\n");
    fprintf(fff, "# applying its P:/X:/R: data.\n");
}

static void scenario_export_wrapped_text(
    FILE* fff, cptr prefix, cptr text, int width)
{
    const char* start = text;

    while (start && *start)
    {
        const char* end = start + strlen(start);
        const char* split = end;

        if ((int)(end - start) > width)
        {
            split = start + width;
            while ((split > start) && (*split != ' '))
                split--;
            if (split == start)
                split = start + width;
        }

        fprintf(fff, "%s%.*s\n", prefix, (int)(split - start), start);

        start = split;
        while (*start == ' ')
            start++;
    }
}

static void scenario_export_object_modifiers(
    FILE* fff, const object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    cptr note_tag = scenario_note_tag(o_ptr);
    int i;

    if (o_ptr->number != 1)
        fprintf(fff, ":number=%u", (unsigned)o_ptr->number);
    if (o_ptr->pval != k_ptr->pval)
        fprintf(fff, ":pval=%d", o_ptr->pval);
    if (o_ptr->discount)
        fprintf(fff, ":discount=%u", (unsigned)o_ptr->discount);
    if (o_ptr->weight != k_ptr->weight)
        fprintf(fff, ":weight=%d", o_ptr->weight);
    if (o_ptr->name1)
        fprintf(fff, ":name1=%u", (unsigned)o_ptr->name1);
    if (o_ptr->name2)
        fprintf(fff, ":name2=%u", (unsigned)o_ptr->name2);
    if (o_ptr->timeout)
        fprintf(fff, ":timeout=%d", o_ptr->timeout);
    if (o_ptr->att != k_ptr->att)
        fprintf(fff, ":att=%d", o_ptr->att);
    if (o_ptr->dd != k_ptr->dd)
        fprintf(fff, ":dd=%u", (unsigned)o_ptr->dd);
    if (o_ptr->ds != k_ptr->ds)
        fprintf(fff, ":ds=%u", (unsigned)o_ptr->ds);
    if (o_ptr->evn != k_ptr->evn)
        fprintf(fff, ":evn=%d", o_ptr->evn);
    if (o_ptr->pd != k_ptr->pd)
        fprintf(fff, ":pd=%u", (unsigned)o_ptr->pd);
    if (o_ptr->ps != k_ptr->ps)
        fprintf(fff, ":ps=%u", (unsigned)o_ptr->ps);
    if (o_ptr->pickup)
        fprintf(fff, ":pickup=%u", (unsigned)o_ptr->pickup);
    if (o_ptr->ident)
        fprintf(fff, ":ident=0x%lx", (unsigned long)o_ptr->ident);
    if (o_ptr->marked)
        fprintf(fff, ":marked=%u", (unsigned)o_ptr->marked);
    if (o_ptr->xtra1)
        fprintf(fff, ":xtra1=%u", (unsigned)o_ptr->xtra1);
    if (o_ptr->abilities)
        fprintf(fff, ":abilities=%u", (unsigned)o_ptr->abilities);
    if (note_tag && note_tag[0])
        fprintf(fff, ":tag=%s", note_tag);

    for (i = 0; i < 8; i++)
    {
        if (o_ptr->skilltype[i] || o_ptr->abilitynum[i])
        {
            fprintf(fff, ":skilltype%d=%u:abilitynum%d=%u", i,
                (unsigned)o_ptr->skilltype[i], i,
                (unsigned)o_ptr->abilitynum[i]);
        }
    }

    if (o_ptr->unused1)
        fprintf(fff, ":unused1=%ld", (long)o_ptr->unused1);
    if (o_ptr->unused2)
        fprintf(fff, ":unused2=%ld", (long)o_ptr->unused2);
    if (o_ptr->unused3)
        fprintf(fff, ":unused3=%ld", (long)o_ptr->unused3);
    if (o_ptr->unused4 && !(note_tag && note_tag[0]))
        fprintf(fff, ":unused4=%ld", (long)o_ptr->unused4);

    if (k_ptr->aware)
        fprintf(fff, ":aware=1");
    if (k_ptr->tried)
        fprintf(fff, ":tried=1");
    if (k_ptr->everseen)
        fprintf(fff, ":everseen=1");
}

static void scenario_export_monster_modifiers(
    FILE* fff, const monster_type* m_ptr, int m_idx)
{
    int i;
    u32b exported_mflag
        = m_ptr->mflag & ~(MFLAG_HOME_WANDER | MFLAG_NO_OPEN_DOORS);

    fprintf(fff, ":hp=%d:maxhp=%d:alertness=%d", m_ptr->hp, m_ptr->maxhp,
        m_ptr->alertness);
    fprintf(fff, ":mspeed=%u:energy=%u:stance=%u", (unsigned)m_ptr->mspeed,
        (unsigned)m_ptr->energy, (unsigned)m_ptr->stance);
    fprintf(fff, ":morale=%d:tmp_morale=%d", m_ptr->morale, m_ptr->tmp_morale);

    if (m_ptr->image_r_idx)
        fprintf(fff, ":image_r_idx=%d", m_ptr->image_r_idx);
    if (m_ptr->skip_next_turn)
        fprintf(fff, ":skip_next_turn=%u", (unsigned)m_ptr->skip_next_turn);
    if (m_ptr->stunned)
        fprintf(fff, ":stunned=%u", (unsigned)m_ptr->stunned);
    if (m_ptr->confused)
        fprintf(fff, ":confused=%u", (unsigned)m_ptr->confused);
    if (m_ptr->hasted)
        fprintf(fff, ":hasted=%d", m_ptr->hasted);
    if (m_ptr->slowed)
        fprintf(fff, ":slowed=%d", m_ptr->slowed);
    if (m_ptr->noise)
        fprintf(fff, ":noise=%u", (unsigned)m_ptr->noise);
    if (m_ptr->encountered)
        fprintf(fff, ":encountered=%u", (unsigned)m_ptr->encountered);
    if (m_ptr->target_y)
        fprintf(fff, ":target_y=%u", (unsigned)m_ptr->target_y);
    if (m_ptr->target_x)
        fprintf(fff, ":target_x=%u", (unsigned)m_ptr->target_x);
    if (m_ptr->wandering_idx)
        fprintf(fff, ":wandering_idx=%d", m_ptr->wandering_idx);
    if (m_ptr->wandering_dist)
        fprintf(fff, ":wandering_dist=%u", (unsigned)m_ptr->wandering_dist);
    if (m_ptr->mana)
        fprintf(fff, ":mana=%u", (unsigned)m_ptr->mana);
    if (m_ptr->song)
        fprintf(fff, ":song=%u", (unsigned)m_ptr->song);
    if (m_ptr->skip_this_turn)
        fprintf(fff, ":skip_this_turn=%u", (unsigned)m_ptr->skip_this_turn);
    if (m_ptr->consecutive_attacks)
        fprintf(fff, ":consecutive_attacks=%d", m_ptr->consecutive_attacks);
    if (m_ptr->turns_stationary)
        fprintf(fff, ":turns_stationary=%d", m_ptr->turns_stationary);
    if ((m_idx > 0) && (m_idx < MAX_MONSTERS) && scenario_monster_tags[m_idx][0])
        fprintf(fff, ":tag=%s", scenario_monster_tags[m_idx]);
    if (m_ptr->mflag & MFLAG_HOME_WANDER)
        fprintf(fff, ":home_wander=1");
    if (m_ptr->mflag & MFLAG_NO_OPEN_DOORS)
        fprintf(fff, ":no_open_doors=1");
    if (exported_mflag)
        fprintf(fff, ":mflag=0x%lx", (unsigned long)exported_mflag);

    for (i = 0; i < ACTION_MAX; i++)
    {
        if (m_ptr->previous_action[i])
            fprintf(fff, ":prev%d=%u", i, (unsigned)m_ptr->previous_action[i]);
    }
}

static int scenario_export_find_combo(byte feat, u16b info,
    scenario_terrain_legend* legends, int* legend_count)
{
    int i;
    cptr candidates = NULL;
    char symbol = '\0';

    for (i = 0; i < *legend_count; i++)
    {
        if ((legends[i].feat == feat) && (legends[i].info == info))
            return (i);
    }

    if ((feat == FEAT_FLOOR) || (feat == FEAT_RAGE_FLOOR))
        candidates = (info & CAVE_MARK) ? "`'_." : "._`'";
    else if (feat == FEAT_SUNLIGHT)
        candidates = (info & CAVE_MARK) ? "~," : ",~";
    else if (feat == FEAT_QUARTZ)
        candidates = (info & CAVE_MARK) ? "&%" : "%&";
    else if ((feat == FEAT_OPEN) || (feat == FEAT_BROKEN)
        || (feat == FEAT_WARDED) || (feat == FEAT_WARDED2)
        || (feat == FEAT_WARDED3))
        candidates = (info & CAVE_MARK) ? "\\/-" : "/\\-";
    else if ((feat >= FEAT_DOOR_HEAD) && (feat <= FEAT_DOOR_TAIL))
        candidates = (info & CAVE_MARK) ? "=+|12345678" : "+=|12345678";
    else if (feat == FEAT_SECRET)
        candidates = "sS";
    else if (feat == FEAT_RUBBLE)
        candidates = (info & CAVE_MARK) ? "Rr" : "rR";
    else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
        candidates = (info & CAVE_MARK) ? "HX$#WM" : "$#HXWM";
    else if ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
        candidates = (feat == FEAT_TRAP_FALSE_FLOOR) ? "?" : "^!?";
    else if (feat == FEAT_GLYPH)
        candidates = ";g";
    else if ((feat == FEAT_LESS) || (feat == FEAT_LESS_SHAFT))
        candidates = "<{";
    else if ((feat == FEAT_MORE) || (feat == FEAT_MORE_SHAFT))
        candidates = ">}";
    else if ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL))
        candidates = "0Oo";
    else if (feat == FEAT_CHASM)
        candidates = "7";

    if (candidates)
    {
        for (; *candidates; candidates++)
        {
            bool used = FALSE;

            for (i = 0; i < *legend_count; i++)
            {
                if (legends[i].symbol == *candidates)
                {
                    used = TRUE;
                    break;
                }
            }

            if (!used)
            {
                symbol = *candidates;
                break;
            }
        }
    }

    if (!symbol)
    {
        int ch;

        for (ch = '!'; ch <= '~'; ch++)
        {
            bool used = FALSE;

            if ((ch == ':') || (ch == '@'))
                continue;

            for (i = 0; i < *legend_count; i++)
            {
                if (legends[i].symbol == (char)ch)
                {
                    used = TRUE;
                    break;
                }
            }

            if (!used)
            {
                symbol = (char)ch;
                break;
            }
        }
    }

    if (!symbol)
        return (-1);

    legends[*legend_count].symbol = symbol;
    legends[*legend_count].feat = feat;
    legends[*legend_count].info = info;
    (*legend_count)++;
    return (*legend_count - 1);
}

static void scenario_export_terrain_flags(FILE* fff, u16b info)
{
    if (info & CAVE_MARK)
        fprintf(fff, ":MARK");
    if (info & CAVE_GLOW)
        fprintf(fff, ":GLOW");
    if (info & CAVE_ICKY)
        fprintf(fff, ":ICKY");
    if (info & CAVE_ROOM)
        fprintf(fff, ":ROOM");
    if (info & CAVE_G_VAULT)
        fprintf(fff, ":G_VAULT");
    if (info & CAVE_HIDDEN)
        fprintf(fff, ":HIDDEN");
}

static cptr scenario_export_race_token(int prace, char* buf, size_t len)
{
    if ((prace >= 0) && (prace < z_info->p_max) && p_info[prace].name)
    {
        cptr name = p_name + p_info[prace].name;

        if (name[0] && (scenario_lookup_race(name) == prace))
        {
            my_strcpy(buf, name, len);
            return (buf);
        }
    }

    strnfmt(buf, len, "#%d", prace);
    return (buf);
}

static cptr scenario_export_house_token(int phouse, char* buf, size_t len)
{
    if ((phouse >= 0) && (phouse < z_info->c_max) && c_info[phouse].name)
    {
        cptr name = c_name + c_info[phouse].name;

        if (name[0] && (scenario_lookup_house(name) == phouse))
        {
            my_strcpy(buf, name, len);
            return (buf);
        }
    }

    strnfmt(buf, len, "#%d", phouse);
    return (buf);
}

static cptr scenario_export_slot_token(int slot, char* buf, size_t len)
{
    switch (slot)
    {
    case INVEN_WIELD:
        my_strcpy(buf, "WIELD", len);
        return (buf);
    case INVEN_BOW:
        my_strcpy(buf, "BOW", len);
        return (buf);
    case INVEN_LEFT:
        my_strcpy(buf, "LEFT", len);
        return (buf);
    case INVEN_RIGHT:
        my_strcpy(buf, "RIGHT", len);
        return (buf);
    case INVEN_NECK:
        my_strcpy(buf, "NECK", len);
        return (buf);
    case INVEN_LITE:
        my_strcpy(buf, "LITE", len);
        return (buf);
    case INVEN_BODY:
        my_strcpy(buf, "BODY", len);
        return (buf);
    case INVEN_OUTER:
        my_strcpy(buf, "OUTER", len);
        return (buf);
    case INVEN_ARM:
        my_strcpy(buf, "ARM", len);
        return (buf);
    case INVEN_HEAD:
        my_strcpy(buf, "HEAD", len);
        return (buf);
    case INVEN_HANDS:
        my_strcpy(buf, "HANDS", len);
        return (buf);
    case INVEN_FEET:
        my_strcpy(buf, "FEET", len);
        return (buf);
    case INVEN_QUIVER1:
        my_strcpy(buf, "QUIVER1", len);
        return (buf);
    case INVEN_QUIVER2:
        my_strcpy(buf, "QUIVER2", len);
        return (buf);
    default:
        strnfmt(buf, len, "%d", slot);
        return (buf);
    }
}

static cptr scenario_export_object_token(int k_idx, char* buf, size_t len)
{
    char name[80];

    if ((k_idx > 0) && (k_idx < z_info->k_max))
    {
        strip_name(name, k_idx);
        if (name[0] && (scenario_lookup_object(name) == k_idx))
        {
            my_strcpy(buf, name, len);
            return (buf);
        }
    }

    strnfmt(buf, len, "#%d", k_idx);
    return (buf);
}

/* Export an ability using a readable token when it round-trips safely. */
static cptr scenario_export_ability_token(
    int skilltype, int abilitynum, char* buf, size_t len)
{
    int i;

    for (i = 0; i < z_info->b_max; i++)
    {
        ability_type* b_ptr = &b_info[i];

        if (!b_ptr->name)
            continue;
        if (b_ptr->skilltype != skilltype)
            continue;
        if (b_ptr->abilitynum == abilitynum)
            return (b_name + b_ptr->name);
    }

    strnfmt(buf, len, "#%d", abilitynum);
    return (buf);
}

static cptr scenario_export_feature_token(int feat, char* buf, size_t len)
{
    if ((feat >= 0) && (feat < z_info->f_max) && f_info[feat].name)
    {
        char feature_buf[128];
        cptr name;

        my_strcpy(feature_buf, f_name + f_info[feat].name, sizeof(feature_buf));
        name = scenario_trim(feature_buf);

        if (name[0] && (scenario_lookup_feature(name) == feat))
        {
            my_strcpy(buf, name, len);
            return (buf);
        }
    }

    strnfmt(buf, len, "#%d", feat);
    return (buf);
}

/* Export one CREATE_STAIRS value using a readable scenario token. */
static cptr scenario_export_create_stairs_token(int feat, char* buf, size_t len)
{
    if (!feat)
    {
        my_strcpy(buf, "none", len);
        return (buf);
    }

    return (scenario_export_feature_token(feat, buf, len));
}

static cptr scenario_export_monster_token(int r_idx, char* buf, size_t len)
{
    if ((r_idx > 0) && (r_idx < z_info->r_max) && r_info[r_idx].name)
    {
        cptr name = r_name + r_info[r_idx].name;

        if (name[0] && (scenario_lookup_monster(name) == r_idx))
        {
            my_strcpy(buf, name, len);
            return (buf);
        }
    }

    strnfmt(buf, len, "#%d", r_idx);
    return (buf);
}

/* Export one scenario text block using the repeatable X: line format. */
static void scenario_export_text_block(FILE* fff, cptr name, cptr text)
{
    cptr start = text;

    if (!name || !name[0])
        return;

    if (!text || !text[0])
    {
        fprintf(fff, "X:%s:\n", name);
        return;
    }

    while (start)
    {
        cptr end = strchr(start, '\n');

        if (end)
        {
            fprintf(fff, "X:%s:%.*s\n", name, (int)(end - start), start);
            start = end + 1;
        }
        else
        {
            fprintf(fff, "X:%s:%s\n", name, start);
            break;
        }
    }
}

/* Export one active oath definition back into the OATH: directive syntax. */
static void scenario_export_oath(FILE* fff, const scenario_oath* oath)
{
    if (!fff || !oath || !oath->name[0])
        return;

    fprintf(fff, "OATH:%s:VOW:%s\n", oath->name, oath->vow);
    fprintf(fff, "OATH:%s:RESTRICTION:%s\n", oath->name, oath->restriction);
    fprintf(fff, "OATH:%s:REWARD_STAT:%s:%d\n", oath->name,
        stat_names_full[oath->reward_stat], oath->reward_amount);
}

/* Export one scenario rule back into the authored event/action syntax. */
static void scenario_export_rule(FILE* fff, const scenario_rule* rule)
{
    int i;
    cptr event = NULL;

    if (!rule)
        return;

    switch (rule->event)
    {
    case SCN_EVENT_ENTER_LEVEL:
        event = "ENTER_LEVEL";
        break;
    case SCN_EVENT_SEE_MONSTER:
        event = "SEE_MONSTER";
        break;
    case SCN_EVENT_USE_EXIT:
        event = "USE_EXIT";
        break;
    case SCN_EVENT_USE_UP_EXIT:
        event = "USE_UP_EXIT";
        break;
    case SCN_EVENT_USE_DOWN_EXIT:
        event = "USE_DOWN_EXIT";
        break;
    case SCN_EVENT_ESCAPE:
        event = "ESCAPE";
        break;
    case SCN_EVENT_START_SONG:
        event = "START_SONG";
        break;
    case SCN_EVENT_ATTACK_MONSTER:
        event = "ATTACK_MONSTER";
        break;
    case SCN_EVENT_DAMAGE_MONSTER:
        event = "DAMAGE_MONSTER";
        break;
    case SCN_EVENT_DEATH:
        event = "DEATH";
        break;
    default:
        return;
    }

    fprintf(fff, "R:%s", event);
    if (rule->tag_set)
        fprintf(fff, ":tag=%s", rule->tag);
    if (rule->when_count > 0)
    {
        fprintf(fff, ":when=");
        for (i = 0; i < rule->when_count; i++)
        {
            const scenario_rule_term* term = &rule->when[i];

            if (i > 0)
                fprintf(fff, ",");
            if (!term->expected)
                fprintf(fff, "!");

            if (term->kind == SCN_TERM_FLAG)
            {
                fprintf(fff, "%s", scenario.flag_names[term->flag_idx]);
            }
            else if (term->kind == SCN_TERM_DEPTH)
            {
                fprintf(fff, "depth=%d", term->value);
            }
            else if (term->kind == SCN_TERM_NEW_DEPTH)
            {
                fprintf(fff, "new_depth=%d", term->value);
            }
            else if (term->kind == SCN_TERM_HAS_SILMARIL)
            {
                fprintf(fff, "has_silmaril()");
            }
            else if (term->kind == SCN_TERM_MONSTER_SLAIN)
            {
                char monster_buf[80];

                fprintf(fff, "monster_slain(%s)",
                    scenario_export_monster_token(
                        term->value, monster_buf, sizeof(monster_buf)));
            }
            else if (term->kind == SCN_TERM_HAS_ITEM)
            {
                char object_buf[80];

                fprintf(fff, "has_item(%s,%d)",
                    scenario_export_object_token(
                        term->value, object_buf, sizeof(object_buf)),
                    term->extra);
            }
            else if (term->kind == SCN_TERM_CHOSEN_OATH)
            {
                fprintf(fff, "chosen_oath(%s)",
                    (term->value == 0) ? "None" : oath_name(term->value));
            }
            else if (term->kind == SCN_TERM_OATH_BROKEN)
            {
                fprintf(fff, "oath_broken()");
            }
            else if (term->kind == SCN_TERM_MONSTER_IS)
            {
                if (term->value == SCN_MONSTER_IS_MAN)
                    fprintf(fff, "monster_is(Man)");
                else if (term->value == SCN_MONSTER_IS_ELF)
                    fprintf(fff, "monster_is(Elf)");
                else
                {
                    char monster_buf[80];

                    fprintf(fff, "monster_is(%s)",
                        scenario_export_monster_token(
                            term->value, monster_buf, sizeof(monster_buf)));
                }
            }
            else if (term->kind == SCN_TERM_MONSTER_VISIBLE)
            {
                fprintf(fff, "monster_visible()");
            }
        }
    }

    for (i = 0; i < rule->action_count; i++)
    {
        const scenario_rule_action* action = &rule->actions[i];

        if (action->kind == SCN_ACTION_SET_FLAG)
        {
            fprintf(fff, ":SET_FLAG=%s", scenario.flag_names[action->flag_idx]);
        }
        else if (action->kind == SCN_ACTION_CLEAR_FLAG)
        {
            fprintf(fff, ":CLEAR_FLAG=%s", scenario.flag_names[action->flag_idx]);
        }
        else if (action->kind == SCN_ACTION_DISPLAY_TEXT)
        {
            fprintf(
                fff, ":DISPLAY_TEXT=%s", scenario.texts[action->text_idx].name);
        }
        else if (action->kind == SCN_ACTION_DISPLAY_MESSAGE)
        {
            fprintf(fff, ":DISPLAY_MESSAGE=%s",
                scenario.texts[action->text_idx].name);
        }
        else if (action->kind == SCN_ACTION_ADD_NOTE)
        {
            fprintf(
                fff, ":ADD_NOTE=%s", scenario.texts[action->text_idx].name);
        }
        else if (action->kind == SCN_ACTION_CONFIRM_TEXT)
        {
            fprintf(fff, ":CONFIRM_TEXT=%s",
                scenario.texts[action->text_idx].name);
        }
        else if (action->kind == SCN_ACTION_BREAK_OATH)
        {
            fprintf(fff, ":BREAK_OATH");
        }
        else if (action->kind == SCN_ACTION_SET_NEW_DEPTH)
        {
            fprintf(fff, ":SET_NEW_DEPTH=%d", action->value);
        }
        else if (action->kind == SCN_ACTION_CREATE_STAIRS)
        {
            char feat_buf[128];

            fprintf(fff, ":CREATE_STAIRS=%s",
                scenario_export_create_stairs_token(
                    action->value, feat_buf, sizeof(feat_buf)));
        }
        else if (action->kind == SCN_ACTION_DENY_ACTION)
        {
            fprintf(fff, ":DENY_ACTION");
        }
        else if (action->kind == SCN_ACTION_END_GAME)
        {
            fprintf(fff, ":END_GAME");
        }
    }

    fprintf(fff, "\n");
}

/* Dump the current player and level state to a human-readable scenario file. */
bool scenario_export_current(cptr filename)
{
    FILE* fff;
    int y;
    int x;
    char path[1024];
    char map_name[SCENARIO_MAX_NAME_LEN];
    char token[160];
    scenario_map* active_map = NULL;
    scenario_terrain_legend legends[SCENARIO_MAX_TERRAIN_LEGENDS];
    int legend_count = 0;

    path_parse(path, sizeof(path), filename);
    fff = my_fopen(path, "w");
    if (!fff)
    {
        msg_format("Cannot write scenario file '%s'.", filename);
        message_flush();
        return (FALSE);
    }

    scenario_export_header(fff, path);

    fprintf(fff, "# Format version.\n");
    fprintf(fff, "V:%d\n\n", SCENARIO_VERSION);

    fprintf(fff, "# Player state.\n");
    fprintf(fff, "P:RACE:%s\n",
        scenario_export_race_token(p_ptr->prace, token, sizeof(token)));
    fprintf(fff, "P:HOUSE:%s\n",
        scenario_export_house_token(p_ptr->phouse, token, sizeof(token)));
    fprintf(fff, "P:NAME:%s\n", op_ptr->full_name);
    if (p_ptr->history[0])
        scenario_export_wrapped_text(fff, "P:HISTORY:", p_ptr->history, 72);
    fprintf(fff, "P:AGE:%d\n", p_ptr->age);
    fprintf(fff, "P:HEIGHT:%d\n", p_ptr->ht);
    fprintf(fff, "P:WEIGHT:%d\n", p_ptr->wt);
    fprintf(fff, "P:DEPTH:%d\n", p_ptr->depth);
    fprintf(fff, "P:MAX_DEPTH:%d\n", p_ptr->max_depth);
    fprintf(fff, "P:EXP:%ld\n", (long)p_ptr->exp);
    fprintf(fff, "P:NEW_EXP:%ld\n", (long)p_ptr->new_exp);
    fprintf(fff, "P:FOOD:%d\n", p_ptr->food);
    fprintf(fff, "P:HP:%d\n", p_ptr->chp);
    fprintf(fff, "P:VOICE:%d\n", p_ptr->csp);

    for (y = 0; y < A_MAX; y++)
        fprintf(fff, "P:STAT:%s:%d\n", stat_names[y], p_ptr->stat_base[y]);
    for (y = 0; y < S_MAX; y++)
        fprintf(fff, "P:SKILL:%s:%d\n", skill_names_full[y], p_ptr->skill_base[y]);
    for (y = 0; y < z_info->b_max; y++)
    {
        ability_type* b_ptr = &b_info[y];
        bool innate;
        bool active;

        if (!b_ptr->name)
            continue;

        innate = p_ptr->innate_ability[b_ptr->skilltype][b_ptr->abilitynum] ? TRUE
                                                                             : FALSE;
        active = p_ptr->active_ability[b_ptr->skilltype][b_ptr->abilitynum] ? TRUE
                                                                             : FALSE;

        if (!innate && !active)
            continue;

        fprintf(fff, "P:ABILITY:%s:%s", skill_names_full[b_ptr->skilltype],
            scenario_export_ability_token(
                b_ptr->skilltype, b_ptr->abilitynum, token, sizeof(token)));
        if (!innate)
            fprintf(fff, ":INNATE=0");
        if (!active)
            fprintf(fff, ":ACTIVE=0");
        fprintf(fff, "\n");
    }

    fprintf(fff, "\n");

    fprintf(fff, "# Starting inventory and equipped items.\n");
    for (y = 0; y < INVEN_TOTAL; y++)
    {
        object_type* o_ptr = &inventory[y];

        if (!o_ptr->k_idx)
            continue;

        fprintf(fff, "I:%s:%s",
            scenario_export_slot_token(y, token, sizeof(token)),
            scenario_export_object_token(o_ptr->k_idx, token + 80, sizeof(token) - 80));
        scenario_export_object_modifiers(fff, o_ptr);
        fprintf(fff, "\n");
    }

    fprintf(fff, "\n");

    if ((scenario_active_map_idx >= 0) && (scenario_active_map_idx < scenario.level_count)
        && scenario.levels[scenario_active_map_idx].depth_set
        && (scenario.levels[scenario_active_map_idx].depth == p_ptr->depth))
    {
        active_map = &scenario.levels[scenario_active_map_idx];
    }

    if (active_map)
    {
        if (active_map->name[0])
            my_strcpy(map_name, active_map->name, sizeof(map_name));
        else
            strnfmt(map_name, sizeof(map_name), "Depth %d", p_ptr->depth);
    }
    else
    {
        strnfmt(map_name, sizeof(map_name), "Depth %d", p_ptr->depth);
    }

    fprintf(fff, "# Authored start map.\n");
    fprintf(fff, "MAP:%s\n", map_name);
    fprintf(fff, "MAP_DEPTH:%d\n", p_ptr->depth);
    if (good_item_flag || (active_map && (active_map->flags & SCN_LEVEL_FLAG_GOOD_ITEM)))
        fprintf(fff, "MAP_FLAGS:GOOD_ITEM\n");
    fprintf(fff, "MAP_SIZE:%d:%d\n", p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    fprintf(fff, "MAP_ORIGIN:0:0\n\n");

    fprintf(fff, "# Terrain legend for the map rows below.\n");
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            u16b info = cave_info[y][x] & SCENARIO_CAVE_INFO_FLAGS;

            if (scenario_export_find_combo(
                    cave_feat[y][x], info, legends, &legend_count)
                < 0)
            {
                my_fclose(fff);
                msg_print("Too many distinct terrain combinations to export.");
                message_flush();
                return (FALSE);
            }
        }
    }

    for (y = 0; y < legend_count; y++)
    {
        fprintf(fff, "T:%c:FEATURE:%s", legends[y].symbol,
            scenario_export_feature_token(
                legends[y].feat, token, sizeof(token)));
        scenario_export_terrain_flags(fff, legends[y].info);
        fprintf(fff, "\n");
    }

    fprintf(fff, "\n");

    fprintf(fff, "# Map rows using the terrain legend above.\n");
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        char row[SCENARIO_MAX_COLS + 1];

        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            int idx = scenario_export_find_combo(cave_feat[y][x],
                cave_info[y][x] & SCENARIO_CAVE_INFO_FLAGS, legends,
                &legend_count);
            row[x] = legends[idx].symbol;
        }
        row[p_ptr->cur_map_wid] = '\0';
        fprintf(fff, "M:%s\n", row);
    }

    fprintf(fff, "\n");

    fprintf(fff, "# Sparse MAP entities.\n");
    fprintf(fff, "E:%d:%d:PLAYER\n", p_ptr->py, p_ptr->px);

    for (y = 1; y < o_max; y++)
    {
        object_type* o_ptr = &o_list[y];

        if (!o_ptr->k_idx || o_ptr->held_m_idx)
            continue;

        fprintf(fff, "E:%u:%u:OBJECT:%s", (unsigned)o_ptr->iy, (unsigned)o_ptr->ix,
            scenario_export_object_token(o_ptr->k_idx, token, sizeof(token)));
        scenario_export_object_modifiers(fff, o_ptr);
        fprintf(fff, "\n");
    }

    for (y = 1; y < mon_max; y++)
    {
        monster_type* m_ptr = &mon_list[y];

        if (!m_ptr->r_idx)
            continue;

        fprintf(fff, "E:%u:%u:MONSTER:%s", (unsigned)m_ptr->fy, (unsigned)m_ptr->fx,
            scenario_export_monster_token(m_ptr->r_idx, token, sizeof(token)));
        scenario_export_monster_modifiers(fff, m_ptr, y);
        fprintf(fff, "\n");
    }

    if (active_map && (active_map->grid_tag_count > 0))
    {
        fprintf(fff, "\n# Tagged grids.\n");
        for (y = 0; y < active_map->grid_tag_count; y++)
        {
            scenario_grid_tag* tag = &active_map->grid_tags[y];

            fprintf(fff, "G:%d:%d:%s\n", tag->y, tag->x, tag->tag);
        }
    }

    if (scenario.text_count > 0)
    {
        fprintf(fff, "\n# Scenario text blocks.\n");
        for (y = 0; y < scenario.text_count; y++)
        {
            scenario_export_text_block(
                fff, scenario.texts[y].name, scenario.texts[y].text);
        }
    }

    if (scenario.oath_count > 0)
    {
        fprintf(fff, "\n# Oath definitions.\n");
        for (y = 0; y < scenario.oath_count; y++)
            scenario_export_oath(fff, &scenario.oaths[y]);
    }

    if (scenario.rule_count > 0)
    {
        fprintf(fff, "\n# Scenario rules.\n");
        for (y = 0; y < scenario.rule_count; y++)
            scenario_export_rule(fff, &scenario.rules[y]);
    }

    my_fclose(fff);
    return (TRUE);
}
