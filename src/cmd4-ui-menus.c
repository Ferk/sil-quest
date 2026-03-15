/*
 * Additional UI-model menu helpers kept separate from the Angband-derived
 * command flow in cmd4.c.
 */

extern char* oath_desc2[];
extern char* oath_reward[];
static int bane_type_killed(int i);
static int bane_bonus_aux(void);

static byte ability_menu_attr(int skilltype, ability_type* b_ptr)
{
    if (p_ptr->have_ability[skilltype][b_ptr->abilitynum])
    {
        if (p_ptr->innate_ability[skilltype][b_ptr->abilitynum])
        {
            if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                return TERM_WHITE;
            else
                return TERM_RED;
        }
        else
        {
            if (p_ptr->active_ability[skilltype][b_ptr->abilitynum])
                return TERM_L_GREEN;
            else
                return TERM_RED;
        }
    }

    if (prereqs(skilltype, b_ptr->abilitynum))
        return TERM_SLATE;

    return TERM_L_DARK;
}

static void ability_menu_label(
    int skilltype, ability_type* b_ptr, char* buf, size_t buf_size)
{
    if ((skilltype == S_PER) && (b_ptr->abilitynum == PER_BANE)
        && (p_ptr->bane_type > 0))
    {
        strnfmt(buf, buf_size, "%c) %s-%s", (char)'a' + b_ptr->abilitynum,
            bane_name[p_ptr->bane_type], (b_name + b_ptr->name));
    }
    else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
        && (p_ptr->oath_type > 0))
    {
        strnfmt(buf, buf_size, "%c) %s: %s", (char)'a' + b_ptr->abilitynum,
            (b_name + b_ptr->name), oath_name[p_ptr->oath_type]);
    }
    else
    {
        strnfmt(buf, buf_size, "%c) %s", (char)'a' + b_ptr->abilitynum,
            (b_name + b_ptr->name));
    }
}

static int build_ability_menu_semantic_text(
    int skilltype, ability_type* b_ptr, byte attr, char* text, byte* attrs,
    size_t size)
{
    ui_text_builder builder;
    char buf[160];

    if (!b_ptr || !text || !attrs || (size == 0))
        return 0;

    ui_text_builder_init(&builder, text, attrs, size);
    ui_text_builder_append_line(&builder, "Abilities", TERM_WHITE);
    ui_text_builder_newline(&builder, TERM_WHITE);

    ability_menu_label(skilltype, b_ptr, buf, sizeof(buf));
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

        if (prereqs(skilltype, b_ptr->abilitynum))
        {
            int exp_cost = (abilities_in_skill(skilltype) + 1) * 500;
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
        && (p_ptr->bane_type > 0))
    {
        strnfmt(buf, sizeof(buf), "%s-Bane:", bane_name[p_ptr->bane_type]);
        ui_text_builder_append_line(&builder, buf, TERM_WHITE);
        strnfmt(buf, sizeof(buf), "%d slain, giving a %+d bonus",
            bane_type_killed(p_ptr->bane_type), bane_bonus_aux());
        ui_text_builder_append_line(&builder, buf, TERM_WHITE);
    }
    else if ((skilltype == S_WIL) && (b_ptr->abilitynum == WIL_OATH)
        && (p_ptr->oath_type > 0))
    {
        ui_text_builder_append(&builder, "Oath: ", TERM_WHITE);
        ui_text_builder_append_line(&builder, oath_name[p_ptr->oath_type],
            TERM_L_BLUE);

        strnfmt(buf, sizeof(buf), "You have sworn not to %s.",
            oath_desc2[p_ptr->oath_type]);
        ui_text_builder_append_line(&builder, buf, TERM_L_WHITE);

        if (oath_invalid(p_ptr->oath_type))
            ui_text_builder_append_line(
                &builder, "You are an oathbreaker.", TERM_RED);
        else
        {
            strnfmt(buf, sizeof(buf), "Bonus: %s.", oath_reward[p_ptr->oath_type]);
            ui_text_builder_append_line(&builder, buf, TERM_WHITE);
        }
    }

    return ui_text_builder_length(&builder);
}

typedef struct command_menu_entry command_menu_entry;
struct command_menu_entry
{
    int key;
    int row;
    cptr label;
    cptr details;
};

static cptr song_menu_name(int song)
{
    int idx;

    if ((song < 0) || (song >= SNG_WOVEN_THEMES))
        return NULL;

    idx = ability_index(S_SNG, song);
    if (idx < 0)
        return NULL;

    return b_name + b_info[idx].name;
}

static int song_menu_default_highlight(void)
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

static void build_song_menu_extra_details(char* details, size_t details_size)
{
    cptr song1_name = song_menu_name(p_ptr->song1);
    cptr song2_name = song_menu_name(p_ptr->song2);

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

static int build_song_menu_entries(command_menu_entry* entries,
    char labels[][80], char details[][1024], int max_entries)
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

static void render_command_menu(cptr title, int title_row, int col,
    const command_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details)
{
    int i;
    char menu_text[1024];
    byte menu_attrs[1024];
    char menu_details[2048];
    byte menu_details_attrs[2048];
    ui_text_builder menu_builder;
    ui_text_builder details_builder;

    ui_text_builder_init(&menu_builder, menu_text, menu_attrs, sizeof(menu_text));
    ui_text_builder_init(
        &details_builder, menu_details, menu_details_attrs, sizeof(menu_details));

    Term_putstr(col - 3, title_row, -1, TERM_WHITE, title);
    ui_text_builder_append_line(&menu_builder, title, TERM_WHITE);
    ui_text_builder_newline(&menu_builder, TERM_WHITE);
    ui_text_builder_append_line(&menu_builder,
        "Use 8/2 to move, Enter to choose, or click an option.", TERM_SLATE);

    ui_menu_begin();
    ui_menu_set_text(
        menu_text, menu_attrs, ui_text_builder_length(&menu_builder));

    for (i = 0; i < entry_count; i++)
    {
        byte attr = (i + 1 == highlight) ? TERM_L_BLUE : TERM_WHITE;
        size_t label_len = strlen(entries[i].label);

        Term_putstr(col, entries[i].row, -1, attr, entries[i].label);
        ui_menu_add(col, entries[i].row, (int)label_len, 1, entries[i].key,
            (i + 1 == highlight), TERM_WHITE, entries[i].label);
    }

    if ((highlight >= 1) && (highlight <= entry_count))
    {
        const command_menu_entry* selected = &entries[highlight - 1];

        ui_text_builder_append_line(
            &details_builder, selected->label, TERM_L_WHITE);
        ui_text_builder_newline(&details_builder, TERM_WHITE);
        ui_text_builder_append_line(
            &details_builder, selected->details, TERM_SLATE);
    }

    if (extra_details && extra_details[0] != '\0')
    {
        if (ui_text_builder_length(&details_builder) > 0)
        {
            ui_text_builder_newline(&details_builder, TERM_WHITE);
            ui_text_builder_newline(&details_builder, TERM_WHITE);
        }
        ui_text_builder_append_line(&details_builder, extra_details, TERM_SLATE);
    }

    ui_menu_set_details(
        menu_details, menu_details_attrs, ui_text_builder_length(&details_builder));
    ui_menu_end();
}

static int command_menu_read_action(
    int* highlight, const command_menu_entry* entries, int entry_count)
{
    int i;
    char ch;

    if (entry_count <= 0)
        return ESCAPE;

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > entry_count)
        *highlight = entry_count;

    hide_cursor = TRUE;
    ch = inkey();
    hide_cursor = FALSE;

    if (ch == ESCAPE)
        return ESCAPE;

    if (ch == '8')
    {
        *highlight = (*highlight + entry_count - 2) % entry_count + 1;
        return 0;
    }

    if (ch == '2')
    {
        *highlight = *highlight % entry_count + 1;
        return 0;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' '))
        return entries[*highlight - 1].key;

    for (i = 0; i < entry_count; i++)
    {
        if (ch == entries[i].key)
        {
            *highlight = i + 1;
            return entries[i].key;
        }
    }

    return -1;
}
