/* SPDX-License-Identifier: EUPL-1.2 */
/*
 * Copyright (c) 2026 Fernando Carmona Varo
 *
 * Small dialogue runtime used by quest-authored conversations.
 */

#include "dialogue.h"
#include "ui-model.h"

typedef enum dialogue_test_node dialogue_test_node;

enum dialogue_test_node
{
    DIALOGUE_TEST_GREETING,
    DIALOGUE_TEST_SERVICE,
    DIALOGUE_TEST_SEALED_PATH,
    DIALOGUE_TEST_MEMORY,
    DIALOGUE_TEST_SEALED_BY,
    DIALOGUE_TEST_END
};

static bool dialogue_test_asked_service = FALSE;
static bool dialogue_test_heard_sealed_path = FALSE;
static bool dialogue_force_attack_active_state = FALSE;

static bool dialogue_web_active = FALSE;
static dialogue_test_node dialogue_web_node = DIALOGUE_TEST_GREETING;
static unsigned int dialogue_web_revision = 1;
static char dialogue_web_body[1024];
static char dialogue_web_options[512];

static void dialogue_web_close_internal(void);

static void dialogue_web_touch(void)
{
    dialogue_web_revision++;
    if (dialogue_web_revision == 0)
        dialogue_web_revision = 1;
    ui_front_invalidate();
}

static void dialogue_web_set_body(cptr text)
{
    my_strcpy(dialogue_web_body, text ? text : "", sizeof(dialogue_web_body));
}

static void dialogue_web_set_options(cptr options_json)
{
    my_strcpy(dialogue_web_options, options_json ? options_json : "[]",
        sizeof(dialogue_web_options));
}

static void dialogue_web_publish_node(void)
{
    switch (dialogue_web_node)
    {
    case DIALOGUE_TEST_GREETING:
        dialogue_web_set_body(
            "The doorward studies you in silence, then lowers his spear.\n\n"
            "\"Few come to this forgotten post. Fewer still come with questions.\"");
        dialogue_web_set_options(
            "[{\"key\":97,\"label\":\"Ask who he serves.\"},"
            "{\"key\":98,\"label\":\"Ask about the sealed path.\"},"
            "{\"key\":99,\"label\":\"Leave him to his watch.\"}]");
        break;

    case DIALOGUE_TEST_SERVICE:
        dialogue_test_asked_service = TRUE;
        dialogue_web_set_body(
            "\"I serve memory first, and my hidden lord second. Both are poor masters.\"");
        dialogue_web_set_options(
            "[{\"key\":97,\"label\":\"Ask what he remembers.\"},"
            "{\"key\":98,\"label\":\"Return to other matters.\"}]");
        break;

    case DIALOGUE_TEST_SEALED_PATH:
        dialogue_test_heard_sealed_path = TRUE;
        dialogue_web_set_body(
            "\"Beyond this place lies an errand of knives, oaths, and half-truths.\"\n\n"
            "\"If you would walk it, learn first how words can open wounds.\"");
        dialogue_web_set_options(
            "[{\"key\":97,\"label\":\"Ask who sealed it.\"},"
            "{\"key\":98,\"label\":\"Return to other matters.\"}]");
        break;

    case DIALOGUE_TEST_MEMORY:
        dialogue_web_set_body(
            "\"I remember a house where every answer made another enemy.\"\n\n"
            "\"That is why I test questions before I test blades.\"");
        dialogue_web_set_options(
            "[{\"key\":97,\"label\":\"Return to other matters.\"}]");
        break;

    case DIALOGUE_TEST_SEALED_BY:
        dialogue_web_set_body(
            "\"Not Morgoth. Not wholly. Some doors are barred by those who fear rescue.\"");
        dialogue_web_set_options(
            "[{\"key\":97,\"label\":\"Return to other matters.\"}]");
        break;

    case DIALOGUE_TEST_END:
        dialogue_web_set_body("");
        dialogue_web_set_options("[]");
        break;
    }

    dialogue_web_touch();
}

#ifndef USE_WEB
static void dialogue_put_wrapped(cptr text, int* row, int col, int width)
{
    char line[160];
    cptr s = text;

    while (s && *s && (*row < Term->hgt - 4))
    {
        int len = 0;
        int cut = -1;

        while (s[len] && (len < width) && (len < (int)sizeof(line) - 1))
        {
            if (s[len] == ' ')
                cut = len;
            len++;
        }

        if (s[len] && (cut > 0))
            len = cut;

        memcpy(line, s, len);
        line[len] = '\0';
        c_put_str(TERM_WHITE, line, *row, col);
        (*row)++;

        s += len;
        while (*s == ' ')
            s++;
    }
}

static void dialogue_draw_option(int row, char key, cptr label)
{
    c_put_str(TERM_L_BLUE, format("%c)", key), row, 4);
    c_put_str(TERM_L_WHITE, label, row, 7);
}

static void dialogue_clear(void)
{
    Term_clear();
    c_put_str(TERM_L_BLUE, "Doorward of the Hidden Gate", 1, 2);
    c_put_str(TERM_SLATE, "Choose a reply, or press ESC to leave.", 2, 2);
}

static dialogue_test_node dialogue_test_show_node(dialogue_test_node node)
{
    char ch;
    int row = 4;

    dialogue_clear();

    switch (node)
    {
    case DIALOGUE_TEST_GREETING:
        dialogue_put_wrapped(
            "The doorward studies you in silence, then lowers his spear.", &row, 4, 68);
        dialogue_put_wrapped(
            "\"Few come to this forgotten post. Fewer still come with questions.\"",
            &row, 4, 68);
        row++;
        dialogue_draw_option(row++, 'a', "Ask who he serves.");
        dialogue_draw_option(row++, 'b', "Ask about the sealed path.");
        dialogue_draw_option(row++, 'c', "Leave him to his watch.");
        break;

    case DIALOGUE_TEST_SERVICE:
        dialogue_test_asked_service = TRUE;
        dialogue_put_wrapped(
            "\"I serve memory first, and my hidden lord second. Both are poor masters.\"",
            &row, 4, 68);
        row++;
        dialogue_draw_option(row++, 'a', "Ask what he remembers.");
        dialogue_draw_option(row++, 'b', "Return to other matters.");
        break;

    case DIALOGUE_TEST_SEALED_PATH:
        dialogue_test_heard_sealed_path = TRUE;
        dialogue_put_wrapped(
            "\"Beyond this place lies an errand of knives, oaths, and half-truths.\"",
            &row, 4, 68);
        dialogue_put_wrapped(
            "\"If you would walk it, learn first how words can open wounds.\"",
            &row, 4, 68);
        row++;
        dialogue_draw_option(row++, 'a', "Ask who sealed it.");
        dialogue_draw_option(row++, 'b', "Return to other matters.");
        break;

    case DIALOGUE_TEST_MEMORY:
        dialogue_put_wrapped(
            "\"I remember a house where every answer made another enemy.\"",
            &row, 4, 68);
        dialogue_put_wrapped(
            "\"That is why I test questions before I test blades.\"", &row, 4, 68);
        row++;
        dialogue_draw_option(row++, 'a', "Return to other matters.");
        break;

    case DIALOGUE_TEST_SEALED_BY:
        dialogue_put_wrapped(
            "\"Not Morgoth. Not wholly. Some doors are barred by those who fear rescue.\"",
            &row, 4, 68);
        row++;
        dialogue_draw_option(row++, 'a', "Return to other matters.");
        break;

    case DIALOGUE_TEST_END:
        return DIALOGUE_TEST_END;
    }

    Term_fresh();
    ch = inkey();

    if (ch == ESCAPE)
        return DIALOGUE_TEST_END;

    switch (node)
    {
    case DIALOGUE_TEST_GREETING:
        if ((ch == 'a') || (ch == 'A'))
            return DIALOGUE_TEST_SERVICE;
        if ((ch == 'b') || (ch == 'B'))
            return DIALOGUE_TEST_SEALED_PATH;
        if ((ch == 'c') || (ch == 'C'))
            return DIALOGUE_TEST_END;
        break;

    case DIALOGUE_TEST_SERVICE:
        if (((ch == 'a') || (ch == 'A')) && dialogue_test_asked_service)
            return DIALOGUE_TEST_MEMORY;
        if ((ch == 'b') || (ch == 'B'))
            return DIALOGUE_TEST_GREETING;
        break;

    case DIALOGUE_TEST_SEALED_PATH:
        if (((ch == 'a') || (ch == 'A')) && dialogue_test_heard_sealed_path)
            return DIALOGUE_TEST_SEALED_BY;
        if ((ch == 'b') || (ch == 'B'))
            return DIALOGUE_TEST_GREETING;
        break;

    case DIALOGUE_TEST_MEMORY:
    case DIALOGUE_TEST_SEALED_BY:
        if ((ch == 'a') || (ch == 'A'))
            return DIALOGUE_TEST_GREETING;
        break;

    case DIALOGUE_TEST_END:
        break;
    }

    bell("Invalid reply.");
    return node;
}

static bool dialogue_test_doorward(void)
{
    dialogue_test_node node = DIALOGUE_TEST_GREETING;

    screen_save();

    while (node != DIALOGUE_TEST_END)
        node = dialogue_test_show_node(node);

    screen_load();
    p_ptr->redraw |= (PR_MAP);
    p_ptr->window |= (PW_OVERHEAD);
    return (TRUE);
}
#endif

bool dialogue_monster_has_bump_dialogue(const monster_type* m_ptr)
{
    cptr tag = scenario_monster_tag(m_ptr);

    if (!tag)
        return (FALSE);

    if (!my_stricmp(tag, "doorward"))
        return (TRUE);

    return (FALSE);
}

bool dialogue_monster_can_talk(const monster_type* m_ptr)
{
    if (!m_ptr || !m_ptr->ml || !dialogue_monster_has_bump_dialogue(m_ptr))
        return (FALSE);

    if (!player_has_los_bold(m_ptr->fy, m_ptr->fx))
        return (FALSE);

    return (TRUE);
}

bool dialogue_handle_monster(monster_type* m_ptr)
{
    if (!dialogue_monster_can_talk(m_ptr))
        return (FALSE);

#ifdef USE_WEB
    return (dialogue_web_open_monster(m_ptr));
#else
    return (dialogue_test_doorward());
#endif
}

bool dialogue_web_open_monster(monster_type* m_ptr)
{
    if (!dialogue_monster_can_talk(m_ptr))
        return (FALSE);

    dialogue_web_active = TRUE;
    dialogue_web_node = DIALOGUE_TEST_GREETING;
    dialogue_web_publish_node();
    return (TRUE);
}

bool dialogue_web_is_active(void)
{
    return (dialogue_web_active);
}

unsigned int dialogue_web_get_revision(void)
{
    return (dialogue_web_revision);
}

cptr dialogue_web_get_title(void)
{
    return "Doorward of the Hidden Gate";
}

cptr dialogue_web_get_body(void)
{
    return dialogue_web_body;
}

cptr dialogue_web_get_options_json(void)
{
    return dialogue_web_options;
}

bool dialogue_web_choose(int key)
{
    dialogue_test_node next = dialogue_web_node;

    if (!dialogue_web_active)
        return (FALSE);

    if ((key == ESCAPE) || (key == 'c') || (key == 'C'))
    {
        if (dialogue_web_node == DIALOGUE_TEST_GREETING)
        {
            dialogue_web_close_internal();
            return (TRUE);
        }
        if (key == ESCAPE)
        {
            dialogue_web_close_internal();
            return (TRUE);
        }
    }

    switch (dialogue_web_node)
    {
    case DIALOGUE_TEST_GREETING:
        if ((key == 'a') || (key == 'A'))
            next = DIALOGUE_TEST_SERVICE;
        else if ((key == 'b') || (key == 'B'))
            next = DIALOGUE_TEST_SEALED_PATH;
        else if ((key == 'c') || (key == 'C'))
            next = DIALOGUE_TEST_END;
        else
            return (FALSE);
        break;

    case DIALOGUE_TEST_SERVICE:
        if ((key == 'a') || (key == 'A'))
            next = DIALOGUE_TEST_MEMORY;
        else if ((key == 'b') || (key == 'B'))
            next = DIALOGUE_TEST_GREETING;
        else
            return (FALSE);
        break;

    case DIALOGUE_TEST_SEALED_PATH:
        if ((key == 'a') || (key == 'A'))
            next = DIALOGUE_TEST_SEALED_BY;
        else if ((key == 'b') || (key == 'B'))
            next = DIALOGUE_TEST_GREETING;
        else
            return (FALSE);
        break;

    case DIALOGUE_TEST_MEMORY:
    case DIALOGUE_TEST_SEALED_BY:
        if ((key == 'a') || (key == 'A'))
            next = DIALOGUE_TEST_GREETING;
        else
            return (FALSE);
        break;

    case DIALOGUE_TEST_END:
        dialogue_web_close_internal();
        return (TRUE);
    }

    if (next == DIALOGUE_TEST_END)
        dialogue_web_close_internal();
    else
    {
        dialogue_web_node = next;
        dialogue_web_publish_node();
    }

    return (TRUE);
}

static void dialogue_web_close_internal(void)
{
    dialogue_web_active = FALSE;
    dialogue_web_node = DIALOGUE_TEST_GREETING;
    dialogue_web_set_body("");
    dialogue_web_set_options("[]");
    dialogue_web_touch();
    if (p_ptr)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_OVERHEAD);
    }
}

void dialogue_web_close(void)
{
    dialogue_web_close_internal();
}

bool dialogue_force_attack_active(void)
{
    return (dialogue_force_attack_active_state);
}

bool dialogue_force_attack_adjacent(int dir)
{
    int y;
    int x;
    monster_type* m_ptr;

    if (!p_ptr || !character_dungeon || (dir < 1) || (dir > 9) || (dir == 5))
        return (FALSE);

    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];
    if (!in_bounds(y, x) || (cave_m_idx[y][x] <= 0))
        return (FALSE);

    m_ptr = &mon_list[cave_m_idx[y][x]];
    if (!m_ptr->ml || !dialogue_monster_has_bump_dialogue(m_ptr))
        return (FALSE);

    dialogue_force_attack_active_state = TRUE;
    py_attack_aux(y, x, ATT_MAIN);
    dialogue_force_attack_active_state = FALSE;

    return (TRUE);
}

bool dialogue_handle_bump(monster_type* m_ptr)
{
    return (dialogue_handle_monster(m_ptr));
}
