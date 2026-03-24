/* File: ui-abilities.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Ability, oath, and song menu presentation helpers kept separate from cmd4.c.
 */

#include "angband.h"

#include "ui-abilities.h"

static cptr ui_oath_names[UI_OATH_COUNT] = {
    "Nothing",
    "Mercy",
    "Silence",
    "Iron",
};

static cptr ui_oath_desc1_text[UI_OATH_COUNT] = {
    "Nothing",
    "to leave Angband without shedding blood of Man or Elf",
    "to leave Angband as you came, grim and silent",
    "that none will daunt you from facing Morgoth forthwith",
};

static cptr ui_oath_desc2_text[UI_OATH_COUNT] = {
    "Nothing",
    "attack Men or Elves",
    "sing",
    "go up stairs without a Silmaril",
};

static cptr ui_oath_reward_text[UI_OATH_COUNT] = {
    "Nothing",
    "+1 Grace",
    "+1 Strength",
    "+2 Constitution",
};

/* Returns a safe oath index for shared text lookups. */
static int ui_oath_index(int oath)
{
    if ((oath < 0) || (oath >= UI_OATH_COUNT))
        return 0;

    return oath;
}

/* Returns the display name for one oath. */
cptr ui_oath_name(int oath)
{
    return ui_oath_names[ui_oath_index(oath)];
}

/* Returns the long-form vow description for one oath. */
cptr ui_oath_desc1(int oath)
{
    return ui_oath_desc1_text[ui_oath_index(oath)];
}

/* Returns the restriction text for one oath. */
cptr ui_oath_desc2(int oath)
{
    return ui_oath_desc2_text[ui_oath_index(oath)];
}

/* Returns the reward text for one oath. */
cptr ui_oath_reward(int oath)
{
    return ui_oath_reward_text[ui_oath_index(oath)];
}

/* Returns the preferred terminal attr for one ability menu entry. */
byte ui_ability_menu_attr(int skilltype, ability_type* b_ptr,
    bool (*prereqs_fn)(int skilltype, int abilitynum))
{
    if (!b_ptr || !prereqs_fn)
        return TERM_L_DARK;

    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
            return p_ptr->active_ability[skilltype][b_ptr->abilitynum]
                ? TERM_WHITE
                : TERM_RED;

        return p_ptr->active_ability[skilltype][b_ptr->abilitynum]
            ? TERM_L_GREEN
            : TERM_RED;
    }

    if (prereqs_fn(skilltype, b_ptr->abilitynum))
        return TERM_SLATE;

    return TERM_L_DARK;
}

/* Formats the visible label for one ability menu entry. */
void ui_ability_menu_label(
    int skilltype, ability_type* b_ptr, char* buf, size_t buf_size)
{
    if (!b_ptr || !buf || (buf_size == 0))
        return;

    if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
        && (p_ptr->bane_type > 0))
    {
        strnfmt(buf, buf_size, "%c) %s-%s", (char)'a' + b_ptr->abilitynum,
            bane_name[p_ptr->bane_type], b_name + b_ptr->name);
    }
    else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
        && (p_ptr->oath_type > 0))
    {
        strnfmt(buf, buf_size, "%c) %s: %s", (char)'a' + b_ptr->abilitynum,
            b_name + b_ptr->name, ui_oath_name(p_ptr->oath_type));
    }
    else
    {
        strnfmt(buf, buf_size, "%c) %s", (char)'a' + b_ptr->abilitynum,
            b_name + b_ptr->name);
    }
}

/* Builds the semantic details text for one ability menu entry. */
int ui_ability_menu_build_details(int skilltype, ability_type* b_ptr, byte attr,
    char* text, byte* attrs, size_t size,
    bool (*prereqs_fn)(int skilltype, int abilitynum),
    int (*abilities_in_skill_fn)(int skilltype),
    int (*bane_type_killed_fn)(int bane_type),
    int (*bane_bonus_aux_fn)(void))
{
    ui_text_builder builder;
    char buf[160];

    if (!b_ptr || !text || !attrs || (size == 0) || !prereqs_fn
        || !abilities_in_skill_fn)
        return 0;

    ui_text_builder_init(&builder, text, attrs, size);
    ui_text_builder_append_line(&builder, "Abilities", TERM_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    ui_ability_menu_label(skilltype, b_ptr, buf, sizeof(buf));
    ui_text_builder_append_line(&builder, buf, attr);
    ui_text_builder_newline(&builder, TERM_WHITE);

    if (b_text && b_ptr->text)
    {
        ui_text_builder_append(&builder, b_text + b_ptr->text, TERM_L_WHITE);
        if ((builder.off > 0) && (builder.text[builder.off - 1] != '\n'))
            ui_text_builder_newline(&builder, TERM_L_WHITE);
        ui_text_builder_newline(&builder, TERM_WHITE);
    }

    if (!p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        ui_text_builder_append_line(&builder, "Prerequisites:", attr);

        strnfmt(buf, sizeof(buf), "%d skill points (you have %d)", b_ptr->level,
            p_ptr->skill_base[skilltype]);
        ui_text_builder_append_line(&builder, buf, TERM_L_DARK);
        if (b_ptr->level <= p_ptr->skill_base[skilltype])
        {
            strnfmt(buf, sizeof(buf), "%d skill points", b_ptr->level);
            ui_text_builder_append_line(&builder, buf, TERM_SLATE);
        }

        if (!p_ptr->active_ability[S_PER][PER_QUICK_STUDY])
        {
            int j;

            for (j = 0; j < b_ptr->prereqs; j++)
            {
                cptr prereq_name = b_name
                    + (&b_info[ability_index(b_ptr->prereq_skilltype[j],
                           b_ptr->prereq_abilitynum[j])])
                          ->name;
                byte prereq_attr = p_ptr->innate_ability[b_ptr->prereq_skilltype[j]]
                                                     [b_ptr->prereq_abilitynum[j]]
                    ? TERM_SLATE
                    : TERM_L_DARK;

                if (j == 0)
                    strnfmt(buf, sizeof(buf), "%s", prereq_name);
                else
                    strnfmt(buf, sizeof(buf), "or %s", prereq_name);
                ui_text_builder_append_line(&builder, buf, prereq_attr);
            }
        }
        else if (b_ptr->prereqs > 0)
        {
            ui_text_builder_append_line(&builder, "Quick Study", TERM_GREEN);
        }

        if (prereqs_fn(skilltype, b_ptr->abilitynum))
        {
            int exp_cost = (abilities_in_skill_fn(skilltype) + 1) * 500;
            byte cost_attr = TERM_L_DARK;

            exp_cost -= 500 * affinity_level(skilltype);
            if (exp_cost < 0)
                exp_cost = 0;
            if (exp_cost <= p_ptr->new_exp)
                cost_attr = TERM_SLATE;

            ui_text_builder_newline(&builder, TERM_WHITE);
            ui_text_builder_append_line(&builder, "Current price:", cost_attr);
            strnfmt(buf, sizeof(buf), "%d experience (you have %d)", exp_cost,
                p_ptr->new_exp);
            ui_text_builder_append_line(&builder, buf, TERM_L_DARK);
            if (exp_cost <= p_ptr->new_exp)
            {
                strnfmt(buf, sizeof(buf), "%d experience", exp_cost);
                ui_text_builder_append_line(&builder, buf, TERM_SLATE);
            }
        }
    }
    else if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
        && (p_ptr->bane_type > 0) && bane_type_killed_fn && bane_bonus_aux_fn)
    {
        strnfmt(buf, sizeof(buf), "%s-Bane:", bane_name[p_ptr->bane_type]);
        ui_text_builder_append_line(&builder, buf, TERM_WHITE);
        strnfmt(buf, sizeof(buf), "%d slain, giving a %+d bonus",
            bane_type_killed_fn(p_ptr->bane_type), bane_bonus_aux_fn());
        ui_text_builder_append_line(&builder, buf, TERM_WHITE);
    }
    else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
        && (p_ptr->oath_type > 0))
    {
        ui_text_builder_append(&builder, "Oath: ", TERM_WHITE);
        ui_text_builder_append_line(
            &builder, ui_oath_name(p_ptr->oath_type), TERM_L_BLUE);

        strnfmt(buf, sizeof(buf), "You have sworn not to %s.",
            ui_oath_desc2(p_ptr->oath_type));
        ui_text_builder_append_line(&builder, buf, TERM_L_WHITE);

        if (oath_invalid(p_ptr->oath_type))
        {
            ui_text_builder_append_line(
                &builder, "You are an oathbreaker.", TERM_RED);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "Bonus: %s.", ui_oath_reward(p_ptr->oath_type));
            ui_text_builder_append_line(&builder, buf, TERM_WHITE);
        }
    }

    return ui_text_builder_length(&builder);
}

/* Returns the display name for one song menu selection. */
static cptr ui_song_menu_name(int song)
{
    int idx;

    if ((song < 0) || (song >= SNG_WOVEN_THEMES))
        return NULL;

    idx = ability_index(S_SNG, song);
    if (idx < 0)
        return NULL;

    return b_name + b_info[idx].name;
}

/* Picks the default highlight for the change-song menu. */
int ui_song_menu_default_highlight(void)
{
    int i;
    int highlight = 1;

    if (p_ptr->song1 != SNG_NOTHING)
    {
        for (i = 0; i < SNG_WOVEN_THEMES; i++)
        {
            if (!p_ptr->active_ability[S_SNG][i])
                continue;

            highlight++;
            if (i == p_ptr->song1)
                return highlight;
        }
    }

    for (i = 0; i < SNG_WOVEN_THEMES; i++)
    {
        if (p_ptr->active_ability[S_SNG][i])
            return highlight + 1;
    }

    return 1;
}

/* Builds the extra status text shown beside the change-song menu. */
void ui_song_menu_build_extra_details(char* details, size_t details_size)
{
    cptr song1_name = ui_song_menu_name(p_ptr->song1);
    cptr song2_name = ui_song_menu_name(p_ptr->song2);

    if (!details || (details_size == 0))
        return;

    details[0] = '\0';

    if (song1_name || song2_name)
    {
        strnfmt(details, details_size, "Current song: %s",
            song1_name ? song1_name : "(none)");
        if (song2_name)
        {
            my_strcat(details, "\nMinor theme: ", details_size);
            my_strcat(details, song2_name, details_size);
        }
    }
    else
    {
        my_strcpy(details, "You are not currently singing.", details_size);
    }
}

/* Builds the simple-menu entries used by the change-song menu. */
int ui_song_menu_build_entries(ui_simple_menu_entry* entries, char labels[][80],
    char details[][1024], int max_entries)
{
    int i;
    int entry_count = 0;

    if (!entries || !labels || !details || (max_entries <= 0))
        return 0;

    entries[entry_count].key = 's';
    entries[entry_count].row = 2;
    strnfmt(labels[entry_count], sizeof(labels[entry_count]), "s) Stop singing");
    strnfmt(details[entry_count], sizeof(details[entry_count]),
        "End your current song. If you are weaving themes, this also ends "
        "the minor theme.");
    entries[entry_count].label = labels[entry_count];
    entries[entry_count].details = details[entry_count];
    entry_count++;

    for (i = 0; (i < SNG_WOVEN_THEMES) && (entry_count < max_entries); i++)
    {
        ability_type* b_ptr;

        if (!p_ptr->active_ability[S_SNG][i])
            continue;

        b_ptr = &b_info[ability_index(S_SNG, i)];
        strnfmt(labels[entry_count], sizeof(labels[entry_count]), "%c) %s",
            (char)'a' + i, b_name + b_ptr->name);
        details[entry_count][0] = '\0';
        if (b_text && b_ptr->text)
            my_strcpy(details[entry_count], b_text + b_ptr->text,
                sizeof(details[entry_count]));
        if (p_ptr->song1 == i)
        {
            my_strcat(details[entry_count], "\n\nThis is your current song.",
                sizeof(details[entry_count]));
        }
        else if (p_ptr->song2 == i)
        {
            my_strcat(details[entry_count],
                "\n\nThis is your current minor theme.",
                sizeof(details[entry_count]));
        }
        if (chosen_oath(OATH_SILENCE) && !oath_invalid(OATH_SILENCE))
        {
            my_strcat(details[entry_count],
                "\n\nSinging will break your oath of silence.",
                sizeof(details[entry_count]));
        }
        entries[entry_count].key = 'a' + i;
        entries[entry_count].row = entry_count + 2;
        entries[entry_count].label = labels[entry_count];
        entries[entry_count].details = details[entry_count];
        entry_count++;
    }

    if ((p_ptr->song2 != SNG_NOTHING) && (entry_count < max_entries))
    {
        entries[entry_count].key = 'x';
        entries[entry_count].row = entry_count + 2;
        strnfmt(labels[entry_count], sizeof(labels[entry_count]),
            "x) Exchange themes");
        strnfmt(details[entry_count], sizeof(details[entry_count]),
            "Swap your main song and minor theme.");
        entries[entry_count].label = labels[entry_count];
        entries[entry_count].details = details[entry_count];
        entry_count++;
    }

    return entry_count;
}
