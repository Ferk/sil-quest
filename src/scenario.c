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

#include "angband.h"

#define SCENARIO_VERSION 1

#define SCENARIO_MAX_ROWS MAX_DUNGEON_HGT
#define SCENARIO_MAX_COLS MAX_DUNGEON_WID
#define SCENARIO_MAX_ITEMS 64
#define SCENARIO_MAX_ENTITY_LEGENDS 96
#define SCENARIO_MAX_TERRAIN_LEGENDS 96
#define SCENARIO_MAX_FLOOR_OBJECTS 512
#define SCENARIO_MAX_FLOOR_MONSTERS MAX_MONSTERS

#define SCN_FLAG_LIT 0x01

#define SCN_SLOT_PACK 255

#define SCN_ENTITY_MONSTER 1
#define SCN_ENTITY_OBJECT 2

#define SCENARIO_CAVE_INFO_FLAGS                                              \
    (CAVE_MARK | CAVE_GLOW | CAVE_ICKY | CAVE_ROOM | CAVE_G_VAULT | CAVE_HIDDEN)

typedef struct scenario_object_spec scenario_object_spec;
typedef struct scenario_monster_spec scenario_monster_spec;
typedef struct scenario_entity_legend scenario_entity_legend;
typedef struct scenario_terrain_legend scenario_terrain_legend;
typedef struct scenario_floor_object scenario_floor_object;
typedef struct scenario_floor_monster scenario_floor_monster;
typedef struct scenario_state scenario_state;

struct scenario_object_spec
{
    byte slot;
    s16b k_idx;

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

    byte previous_action[ACTION_MAX];
    bool previous_action_set[ACTION_MAX];

    bool sleep;
    bool sleep_set;
};

struct scenario_entity_legend
{
    char symbol;
    byte type;
    scenario_object_spec object;
    scenario_monster_spec monster;
};

struct scenario_terrain_legend
{
    char symbol;
    byte feat;
    u16b info;
};

struct scenario_floor_object
{
    int y;
    int x;
    scenario_object_spec object;
};

struct scenario_floor_monster
{
    int y;
    int x;
    scenario_monster_spec monster;
};

struct scenario_state
{
    bool loaded;
    bool level_pending;

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

    int width;
    int height;

    int terrain_rows;
    int entity_rows;

    bool player_pos_set;
    int player_y;
    int player_x;

    int item_count;
    scenario_object_spec items[SCENARIO_MAX_ITEMS];

    int entity_legend_count;
    scenario_entity_legend entity_legends[SCENARIO_MAX_ENTITY_LEGENDS];

    int terrain_legend_count;
    scenario_terrain_legend terrain_legends[SCENARIO_MAX_TERRAIN_LEGENDS];

    int floor_object_count;
    scenario_floor_object floor_objects[SCENARIO_MAX_FLOOR_OBJECTS];

    int floor_monster_count;
    scenario_floor_monster floor_monsters[SCENARIO_MAX_FLOOR_MONSTERS];

    char terrain[SCENARIO_MAX_ROWS][SCENARIO_MAX_COLS + 1];
    char entities[SCENARIO_MAX_ROWS][SCENARIO_MAX_COLS + 1];
};

static scenario_state scenario;
static int scenario_error_line = 0;
static char scenario_error[160];
static int scenario_warning_line = 0;
static int scenario_warning_count = 0;
static char scenario_warning[160];

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

static scenario_entity_legend* scenario_find_entity_legend(char symbol)
{
    int i;

    for (i = 0; i < scenario.entity_legend_count; i++)
    {
        if (scenario.entity_legends[i].symbol == symbol)
            return (&scenario.entity_legends[i]);
    }

    return (NULL);
}

static scenario_terrain_legend* scenario_find_terrain_legend(char symbol)
{
    int i;

    for (i = 0; i < scenario.terrain_legend_count; i++)
    {
        if (scenario.terrain_legends[i].symbol == symbol)
            return (&scenario.terrain_legends[i]);
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
    cptr row, int line)
{
    size_t len = strlen(row);

    if (*row_count >= SCENARIO_MAX_ROWS)
        return (scenario_fail(line, "Scenario map is too tall."));

    if (!scenario.width)
    {
        if ((len <= 0) || (len > SCENARIO_MAX_COLS))
            return (scenario_fail(line, "Scenario map has invalid width."));
        scenario.width = (int)len;
    }
    else if ((int)len != scenario.width)
    {
        return (scenario_fail(line, "Scenario map rows must be rectangular."));
    }

    my_strcpy(rows[*row_count], row, SCENARIO_MAX_COLS + 1);
    (*row_count)++;
    return (TRUE);
}

static bool scenario_parse_terrain_legend(char* body, int line)
{
    char local[256];
    char* parts[24];
    int count;
    int feat;
    int i;
    scenario_terrain_legend* legend;
    long info_value;

    if (scenario.terrain_legend_count >= SCENARIO_MAX_TERRAIN_LEGENDS)
        return (scenario_fail(line, "Too many terrain legends."));

    my_strcpy(local, body, sizeof(local));
    count = scenario_split(local, parts, 24);
    if (count < 3)
        return (scenario_fail(line, "Malformed terrain legend."));

    parts[0] = scenario_trim(parts[0]);
    parts[1] = scenario_trim(parts[1]);
    parts[2] = scenario_trim(parts[2]);

    if (!parts[0][0] || parts[0][1] || (parts[0][0] == ' ')
        || (parts[0][0] == '@'))
    {
        return (scenario_fail(line, "Invalid terrain legend symbol."));
    }

    if (my_stricmp(parts[1], "FEATURE"))
        return (scenario_fail(line, "Unknown terrain legend type."));

    feat = scenario_lookup_feature(parts[2]);
    if (feat < 0)
        return (scenario_fail(line, "Unknown terrain feature."));

    legend = &scenario.terrain_legends[scenario.terrain_legend_count++];
    legend->symbol = parts[0][0];
    legend->feat = (byte)feat;
    legend->info = 0;

    for (i = 3; i < count; i++)
    {
        char* token = scenario_trim(parts[i]);

        if (!my_strnicmp(token, "info=", 5))
        {
            if (!scenario_parse_int(token + 5, &info_value))
                return (scenario_fail(line, "Invalid terrain info mask."));
            legend->info = (u16b)info_value;
        }
        else if (!scenario_parse_cave_info_flag_token(&legend->info, token, line))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}

static bool scenario_validate(void)
{
    int y;

    if (!scenario.prace_set)
        return (scenario_fail(0, "Scenario must define a player race."));

    if (!scenario.phouse_set)
        return (scenario_fail(0, "Scenario must define a player house."));

    if (scenario.terrain_rows <= 0)
        return (scenario_fail(0, "Scenario must define a terrain map."));

    scenario.height = scenario.terrain_rows;

    if (scenario.entity_rows && (scenario.entity_rows != scenario.terrain_rows))
    {
        return (scenario_fail(
            0, "Entity map must have the same height as terrain map."));
    }

    for (y = 0; y < scenario.terrain_rows; y++)
    {
        int x;

        for (x = 0; x < scenario.width; x++)
        {
            char symbol = scenario.terrain[y][x];

            if (scenario_find_terrain_legend(symbol))
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
                    0, "Terrain map uses an undefined terrain symbol."));
            }
        }
    }

    if (scenario.entity_rows > 0)
    {
        for (y = 0; y < scenario.entity_rows; y++)
        {
            int x;

            for (x = 0; x < scenario.width; x++)
            {
                char symbol = scenario.entities[y][x];

                if ((symbol == '.') || (symbol == ' '))
                    continue;

                if (symbol == '@')
                {
                    scenario.player_pos_set = TRUE;
                    scenario.player_y = y;
                    scenario.player_x = x;
                    continue;
                }

                if (!scenario_find_entity_legend(symbol))
                {
                    return (scenario_fail(
                        0, "Entity map uses an undefined entity symbol."));
                }
            }
        }
    }

    if (!scenario.player_pos_set)
        return (scenario_fail(0, "Scenario must define a player start."));

    if ((scenario.player_y < 0) || (scenario.player_y >= scenario.height)
        || (scenario.player_x < 0) || (scenario.player_x >= scenario.width))
    {
        return (scenario_fail(0, "Player start lies outside the terrain map."));
    }

    for (y = 0; y < scenario.floor_object_count; y++)
    {
        if ((scenario.floor_objects[y].y < 0)
            || (scenario.floor_objects[y].y >= scenario.height)
            || (scenario.floor_objects[y].x < 0)
            || (scenario.floor_objects[y].x >= scenario.width))
        {
            return (scenario_fail(0, "A floor object lies outside the terrain map."));
        }
    }

    for (y = 0; y < scenario.floor_monster_count; y++)
    {
        if ((scenario.floor_monsters[y].y < 0)
            || (scenario.floor_monsters[y].y >= scenario.height)
            || (scenario.floor_monsters[y].x < 0)
            || (scenario.floor_monsters[y].x >= scenario.width))
        {
            return (
                scenario_fail(0, "A monster lies outside the terrain map."));
        }
    }

    return (TRUE);
}

static void scenario_init_notes(void)
{
    int i;
    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    for (i = 0; i < NOTES_LENGTH; i++)
        notes_buffer[i] = '\0';

    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    strnfmt(month, sizeof(month), "%.2s", raw_date + 5);
    atomonth(atoi(month), month, sizeof(month));

    if (*(raw_date + 7) == '0')
        strnfmt(clean_date, sizeof(clean_date), "%.1s %.3s %.4s", raw_date + 8,
            month, raw_date + 1);
    else
        strnfmt(clean_date, sizeof(clean_date), "%.2s %.3s %.4s", raw_date + 7,
            month, raw_date + 1);

    my_strcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    my_strcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    my_strcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    message_add(" ", MSG_GENERIC, 0);
    message_add("  ", MSG_GENERIC, 0);
    message_add("====================", MSG_GENERIC, 0);
    message_add("  ", MSG_GENERIC, 0);
    message_add(" ", MSG_GENERIC, 0);
}

static int scenario_default_start_exp(void)
{
    if (birth_fixed_exp)
        return (PY_FIXED_EXP);

    return (PY_START_EXP);
}

static void scenario_roll_body_values(void)
{
    if (!scenario.age_set)
        p_ptr->age = rand_range(rp_ptr->b_age, rp_ptr->m_age);
    else
        p_ptr->age = scenario.age;

    if (!scenario.ht_set)
        p_ptr->ht = Rand_normal(rp_ptr->b_ht, rp_ptr->m_ht);
    else
        p_ptr->ht = scenario.ht;

    if (!scenario.wt_set)
    {
        p_ptr->wt = Rand_normal(rp_ptr->b_wt, rp_ptr->m_wt);
        p_ptr->wt += p_ptr->ht / 5;
    }
    else
    {
        p_ptr->wt = scenario.wt;
    }
}

static void scenario_prepare_object(
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

    scenario_prepare_object(i_ptr, spec, TRUE);

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
    scenario_terrain_legend* legend = scenario_find_terrain_legend(ch);

    cave_info[y][x] = 0;

    if (legend)
    {
        cave_set_feat(y, x, legend->feat);
        cave_info[y][x]
            = (cave_info[y][x] & ~SCENARIO_CAVE_INFO_FLAGS)
            | (legend->info & SCENARIO_CAVE_INFO_FLAGS);
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

    scenario_prepare_object(i_ptr, spec, FALSE);

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
    update_mon(m_idx, TRUE);
    return (TRUE);
}

static bool scenario_place_entity(int y, int x, char symbol)
{
    scenario_entity_legend* legend;

    if ((symbol == '.') || (symbol == ' '))
        return (TRUE);

    if (symbol == '@')
        return (scenario_place_player(y, x));

    legend = scenario_find_entity_legend(symbol);
    if (!legend)
        return (FALSE);

    if (legend->type == SCN_ENTITY_MONSTER)
        return (scenario_place_monster(y, x, &legend->monster));

    if (legend->type == SCN_ENTITY_OBJECT)
        return (scenario_place_floor_object(y, x, &legend->object));

    return (FALSE);
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

/*
 * Clear any loaded scenario so a later start or load begins from a clean
 * pending state.
 */
void scenario_clear_pending(void)
{
    WIPE(&scenario, scenario_state);
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
        else if (!my_strnicmp(s, "F:", 2))
        {
            if (!scenario_parse_flag_line(scenario_trim(s + 2), line))
                break;
        }
        else if (!my_strnicmp(s, "T:", 2))
        {
            if (!scenario_parse_terrain_legend(scenario_trim(s + 2), line))
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
                if (count < 3)
                {
                    scenario_fail(line, "POS needs y and x.");
                    break;
                }

                if (!scenario_parse_int(parts[1], &value))
                {
                    scenario_fail(line, "Invalid player y coordinate.");
                    break;
                }
                scenario.player_y = (int)value;

                if (!scenario_parse_int(scenario_trim(parts[2]), &value))
                {
                    scenario_fail(line, "Invalid player x coordinate.");
                    break;
                }
                scenario.player_x = (int)value;
                scenario.player_pos_set = TRUE;
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
        else if (!my_strnicmp(s, "O:", 2))
        {
            char local[512];
            char* parts[48];
            int count;
            int i;
            long value;
            int k_idx;
            scenario_floor_object* entry;

            if (scenario.floor_object_count >= SCENARIO_MAX_FLOOR_OBJECTS)
            {
                scenario_fail(line, "Too many floor objects.");
                break;
            }

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 48);
            if (count < 3)
            {
                scenario_fail(line, "Malformed floor object directive.");
                break;
            }

            entry = &scenario.floor_objects[scenario.floor_object_count++];
            WIPE(entry, scenario_floor_object);

            if (!scenario_parse_int(parts[0], &value))
            {
                scenario_fail(line, "Invalid floor object y coordinate.");
                break;
            }
            entry->y = (int)value;

            if (!scenario_parse_int(parts[1], &value))
            {
                scenario_fail(line, "Invalid floor object x coordinate.");
                break;
            }
            entry->x = (int)value;

            k_idx = scenario_lookup_object(scenario_trim(parts[2]));
            if (k_idx <= 0)
            {
                scenario_fail(line, "Unknown or ambiguous object kind.");
                break;
            }
            entry->object.k_idx = (s16b)k_idx;

            for (i = 3; i < count; i++)
            {
                if (!scenario_parse_object_modifier(
                        &entry->object, scenario_trim(parts[i]), line))
                    break;
            }
            if (i < count)
                break;
        }
        else if (!my_strnicmp(s, "N:", 2))
        {
            char local[512];
            char* parts[64];
            int count;
            int i;
            long value;
            int r_idx;
            scenario_floor_monster* entry;

            if (scenario.floor_monster_count >= SCENARIO_MAX_FLOOR_MONSTERS)
            {
                scenario_fail(line, "Too many monsters.");
                break;
            }

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 64);
            if (count < 3)
            {
                scenario_fail(line, "Malformed monster directive.");
                break;
            }

            entry = &scenario.floor_monsters[scenario.floor_monster_count++];
            WIPE(entry, scenario_floor_monster);

            if (!scenario_parse_int(parts[0], &value))
            {
                scenario_fail(line, "Invalid monster y coordinate.");
                break;
            }
            entry->y = (int)value;

            if (!scenario_parse_int(parts[1], &value))
            {
                scenario_fail(line, "Invalid monster x coordinate.");
                break;
            }
            entry->x = (int)value;

            r_idx = scenario_lookup_monster(scenario_trim(parts[2]));
            if (r_idx <= 0)
            {
                scenario_fail(line, "Unknown monster race.");
                break;
            }
            entry->monster.r_idx = (s16b)r_idx;

            for (i = 3; i < count; i++)
            {
                if (!scenario_parse_monster_modifier(
                        &entry->monster, scenario_trim(parts[i]), line))
                    break;
            }
            if (i < count)
                break;
        }
        else if (!my_strnicmp(s, "L:", 2))
        {
            char local[512];
            char* parts[64];
            int count;
            int i;
            int idx;
            scenario_entity_legend* legend;

            if (scenario.entity_legend_count >= SCENARIO_MAX_ENTITY_LEGENDS)
            {
                scenario_fail(line, "Too many entity legends.");
                break;
            }

            my_strcpy(local, scenario_trim(s + 2), sizeof(local));
            count = scenario_split(local, parts, 64);
            if (count < 3)
            {
                scenario_fail(line, "Malformed legend directive.");
                break;
            }

            legend = &scenario.entity_legends[scenario.entity_legend_count++];
            WIPE(legend, scenario_entity_legend);
            parts[0] = scenario_trim(parts[0]);
            parts[1] = scenario_trim(parts[1]);
            parts[2] = scenario_trim(parts[2]);

            if (!parts[0][0] || parts[0][1] || (parts[0][0] == '.')
                || (parts[0][0] == '@') || (parts[0][0] == ' '))
            {
                scenario_fail(line, "Invalid legend symbol.");
                break;
            }

            legend->symbol = parts[0][0];

            if (!my_stricmp(parts[1], "MONSTER"))
            {
                idx = scenario_lookup_monster(parts[2]);
                if (idx <= 0)
                {
                    scenario_fail(line, "Unknown monster race.");
                    break;
                }

                legend->type = SCN_ENTITY_MONSTER;
                legend->monster.r_idx = (s16b)idx;
                for (i = 3; i < count; i++)
                {
                    if (!scenario_parse_monster_modifier(
                            &legend->monster, scenario_trim(parts[i]), line))
                        break;
                }
            }
            else if (!my_stricmp(parts[1], "OBJECT"))
            {
                idx = scenario_lookup_object(parts[2]);
                if (idx <= 0)
                {
                    scenario_fail(line, "Unknown or ambiguous object kind.");
                    break;
                }

                legend->type = SCN_ENTITY_OBJECT;
                legend->object.k_idx = (s16b)idx;
                for (i = 3; i < count; i++)
                {
                    if (!scenario_parse_object_modifier(
                            &legend->object, scenario_trim(parts[i]), line))
                        break;
                }
            }
            else
            {
                scenario_fail(line, "Unknown legend type.");
                break;
            }

            if (i < count)
                break;
        }
        else if (!my_strnicmp(s, "M:", 2))
        {
            if (!scenario_append_row(
                    scenario.terrain, &scenario.terrain_rows, s + 2, line))
                break;
        }
        else if (!my_strnicmp(s, "E:", 2))
        {
            if (!scenario_append_row(
                    scenario.entities, &scenario.entity_rows, s + 2, line))
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

    scenario.loaded = TRUE;
    scenario.level_pending = FALSE;
    return (TRUE);
}

/*
 * Start a new game from the pending scenario, rebuilding player state from
 * the scenario definition instead of a binary savefile.
 */
bool scenario_start_pending_new_game(void)
{
    int i;
    int j;
    s16b game_type;
    s16b quest_id;

    if (!scenario.loaded)
        return (FALSE);

    game_type = p_ptr->game_type;
    quest_id = p_ptr->unused2;

    player_birth_wipe();

    p_ptr->game_type = game_type;
    p_ptr->unused2 = quest_id;
    p_ptr->prace = scenario.prace;
    p_ptr->phouse = scenario.phouse;

    rp_ptr = &p_info[p_ptr->prace];
    hp_ptr = &c_info[p_ptr->phouse];

    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_base[i] = 0;

    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = scenario.innate_ability[i][j];
            p_ptr->active_ability[i][j] = scenario.active_ability[i][j];
        }
    }

    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];

    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];

    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->hitpoint_warn = 3;
        op_ptr->delay_factor = 5;
    }

    for (i = 0; i < z_info->e_max; i++)
        e_info[i].aware = FALSE;

    if (scenario.full_name_set)
        my_strcpy(op_ptr->full_name, scenario.full_name, sizeof(op_ptr->full_name));
    else
        my_strcpy(op_ptr->full_name, "Adventurer", sizeof(op_ptr->full_name));

    op_ptr->base_name[0] = '\0';

    if (scenario.history[0])
        my_strcpy(p_ptr->history, scenario.history, sizeof(p_ptr->history));

    scenario_roll_body_values();

    p_ptr->depth = scenario.depth_set ? scenario.depth : 1;
    p_ptr->max_depth
        = scenario.max_depth_set ? scenario.max_depth : p_ptr->depth;
    p_ptr->exp = scenario.exp_set ? scenario.exp : scenario_default_start_exp();
    p_ptr->new_exp = scenario.new_exp_set ? scenario.new_exp : p_ptr->exp;
    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_duration = 0;
    p_ptr->food = scenario.food_set ? scenario.food : (PY_FOOD_FULL - 1);
    p_ptr->artefacts = 0;

    for (i = 0; i < A_MAX; i++)
    {
        if (scenario.stat_set[i])
            p_ptr->stat_base[i] = scenario.stat_base[i];
        p_ptr->stat_drain[i] = 0;
    }

    for (i = 0; i < S_MAX; i++)
    {
        if (scenario.skill_set[i])
            p_ptr->skill_base[i] = scenario.skill_base[i];
    }

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

    p_ptr->chp = scenario.chp_set ? MIN(scenario.chp, p_ptr->mhp) : p_ptr->mhp;
    p_ptr->chp_frac = 0;

    p_ptr->csp = scenario.csp_set ? MIN(scenario.csp, p_ptr->msp) : p_ptr->msp;
    p_ptr->csp_frac = 0;

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_BASIC | PR_MISC | PR_EQUIPPY | PR_RESIST | PR_EXP);

    scenario_init_notes();
    scenario.level_pending = TRUE;
    return (TRUE);
}

bool scenario_pending_level_generation(void)
{
    return (scenario.loaded && scenario.level_pending);
}

/*
 * Materialize the pending scenario map, then place the player, authored
 * floor contents, and any legend-driven entities on top of it.
 */
bool scenario_generate_pending_level(void)
{
    int y;

    if (!scenario_pending_level_generation())
        return (FALSE);

    p_ptr->cur_map_hgt = (byte)scenario.height;
    p_ptr->cur_map_wid = (byte)scenario.width;

    for (y = 0; y < scenario.height; y++)
    {
        int x;

        for (x = 0; x < scenario.width; x++)
        {
            if (!scenario_place_terrain(y, x, scenario.terrain[y][x]))
                return (FALSE);
        }
    }

    if (!scenario_place_player(scenario.player_y, scenario.player_x))
        return (FALSE);

    for (y = 0; y < scenario.floor_object_count; y++)
    {
        if (!scenario_place_floor_object(scenario.floor_objects[y].y,
                scenario.floor_objects[y].x, &scenario.floor_objects[y].object))
        {
            return (FALSE);
        }
    }

    for (y = 0; y < scenario.floor_monster_count; y++)
    {
        if (!scenario_place_monster(scenario.floor_monsters[y].y,
                scenario.floor_monsters[y].x, &scenario.floor_monsters[y].monster))
        {
            return (FALSE);
        }
    }

    for (y = 0; y < scenario.entity_rows; y++)
    {
        int x;

        for (x = 0; x < scenario.width; x++)
        {
            if (!scenario_place_entity(y, x, scenario.entities[y][x]))
                return (FALSE);
        }
    }

    scenario_init_wandering_flows();

    scenario.level_pending = FALSE;
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
    fprintf(fff, "# Generated from a savefile. Edit by hand if you want, or regenerate it\n");
    fprintf(fff, "# with the scenario exporter to refresh the exact snapshot.\n");
    fprintf(fff, "#\n");
    fprintf(fff, "# Quick legend:\n");
    fprintf(fff, "# V:<n> format version\n");
    fprintf(fff, "# P:<field>:<value> player state\n");
    fprintf(fff,
        "# P:ABILITY:<skill>:<ability>[:INNATE=0][:ACTIVE=0] player ability state\n");
    fprintf(fff, "# I:<slot>:<object>[:key=value...] starting inventory or equipment\n");
    fprintf(fff, "# T:<symbol>:FEATURE:<feature>[:flags...] terrain legend used by M: rows\n");
    fprintf(fff, "# M:<symbols...> one map row using the T: legend above\n");
    fprintf(fff, "# O:<y>:<x>:<object>[:key=value...] floor object\n");
    fprintf(fff, "# N:<y>:<x>:<monster>[:key=value...] monster\n");
    fprintf(fff, "#\n");
    fprintf(fff, "# Readable names are preferred. The exporter falls back to #<id> when a\n");
    fprintf(fff, "# name would be ambiguous or unsafe to round-trip.\n");
    fprintf(fff, "# The map also tries to use vault-like glyphs where possible, such as #\n");
    fprintf(fff, "# for walls, . for floors, %% for quartz, + for doors, and </> for stairs.\n\n");
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
    if (o_ptr->unused4)
        fprintf(fff, ":unused4=%ld", (long)o_ptr->unused4);

    if (k_ptr->aware)
        fprintf(fff, ":aware=1");
    if (k_ptr->tried)
        fprintf(fff, ":tried=1");
    if (k_ptr->everseen)
        fprintf(fff, ":everseen=1");
}

static void scenario_export_monster_modifiers(
    FILE* fff, const monster_type* m_ptr)
{
    int i;

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
    if (m_ptr->mflag)
        fprintf(fff, ":mflag=0x%lx", (unsigned long)m_ptr->mflag);

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

/* Dump the current player and level state to a human-readable scenario file. */
bool scenario_export_current(cptr filename)
{
    FILE* fff;
    int y;
    int x;
    char path[1024];
    char token[160];
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
    fprintf(fff, "P:POS:%d:%d\n", p_ptr->py, p_ptr->px);

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

    fprintf(fff, "# Floor objects.\n");
    for (y = 1; y < o_max; y++)
    {
        object_type* o_ptr = &o_list[y];

        if (!o_ptr->k_idx || o_ptr->held_m_idx)
            continue;

        fprintf(fff, "O:%u:%u:%s", (unsigned)o_ptr->iy, (unsigned)o_ptr->ix,
            scenario_export_object_token(o_ptr->k_idx, token, sizeof(token)));
        scenario_export_object_modifiers(fff, o_ptr);
        fprintf(fff, "\n");
    }

    fprintf(fff, "\n# Monsters.\n");
    for (y = 1; y < mon_max; y++)
    {
        monster_type* m_ptr = &mon_list[y];

        if (!m_ptr->r_idx)
            continue;

        fprintf(fff, "N:%u:%u:%s", (unsigned)m_ptr->fy, (unsigned)m_ptr->fx,
            scenario_export_monster_token(m_ptr->r_idx, token, sizeof(token)));
        scenario_export_monster_modifiers(fff, m_ptr);
        fprintf(fff, "\n");
    }

    my_fclose(fff);
    return (TRUE);
}
