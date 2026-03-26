/* File: ui-input.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral helpers for interpreting keypresses from the shared input
 * stream after frontend and macro normalization has already happened.
 */

#include "angband.h"

#include "ui-input.h"

/*
 * Simple menus intentionally stay on their canonical up/down keys here.
 * Full movement decoding is too broad for generic menu navigation because
 * it can turn roguelike movement keys into accidental menu actions.
 *
 * Frontends that want arrow support should normalize those keys before they
 * reach this layer rather than pulling in the whole movement-key policy.
 */
static int ui_input_simple_menu_direction(int key)
{
    if (key == '8')
        return -1;

    if (key == '2')
        return 1;

    return 0;
}

/* Decodes one keypress into a movement direction using the active keyset. */
static int ui_input_movement_direction(int key)
{
    return target_dir((char)key);
}

/* Packs one semantic target-input result for the caller. */
static ui_input_target ui_input_target_make(
    ui_input_target_action action, int direction)
{
    ui_input_target input;

    input.action = action;
    input.direction = direction;

    return input;
}

/* Packs one semantic browser-input result for the caller. */
static ui_input_browser ui_input_browser_make(
    ui_input_browser_action action, int direction)
{
    ui_input_browser input;

    input.action = action;
    input.direction = direction;

    return input;
}

/* Returns whether one keypress should be treated as Enter/Return. */
bool ui_input_is_accept_key(int key)
{
    return (key == '\n') || (key == '\r');
}

/* Returns whether one keypress should cancel the current interaction. */
bool ui_input_is_cancel_key(int key)
{
    return key == ESCAPE;
}

/* Returns whether one keypress should delete the previous character. */
bool ui_input_is_backspace_key(int key)
{
    return (key == 0x7F) || (key == '\010');
}

/* Returns whether one keypress should confirm a yes/no prompt. */
bool ui_input_is_yes_key(int key)
{
    return (key == 'Y') || (key == 'y');
}

/* Returns whether one keypress should reject a yes/no prompt. */
bool ui_input_is_no_key(int key)
{
    return (key == 'N') || (key == 'n');
}

/* Converts one alphabetic menu key into a zero-based choice index. */
int ui_input_alpha_choice_index(int key)
{
    if (islower((unsigned char)key))
        return A2I(key);

    if (isupper((unsigned char)key))
        return key - 'A' + 26;

    return -1;
}

/* Classifies one keypress for a simple vertical text menu. */
ui_input_simple_menu_action ui_input_parse_simple_menu_key(int key)
{
    int direction = ui_input_simple_menu_direction(key);

    if (ui_input_is_cancel_key(key) || (key == '4'))
        return UI_INPUT_SIMPLE_MENU_ACTION_CANCEL;

    if (ui_input_is_accept_key(key) || (key == ' ') || (key == '6'))
        return UI_INPUT_SIMPLE_MENU_ACTION_CHOOSE;

    if (direction < 0)
        return UI_INPUT_SIMPLE_MENU_ACTION_PREV;

    if (direction > 0)
        return UI_INPUT_SIMPLE_MENU_ACTION_NEXT;

    return UI_INPUT_SIMPLE_MENU_ACTION_NONE;
}

/* Classifies one keypress for a nested menu with directional navigation. */
ui_input_nested_menu_action ui_input_parse_nested_menu_key(int key)
{
    if (ui_input_is_cancel_key(key))
        return UI_INPUT_NESTED_MENU_ACTION_CANCEL;

    if (ui_input_is_accept_key(key) || (key == ' ') || (key == '5'))
        return UI_INPUT_NESTED_MENU_ACTION_CHOOSE;

    switch (key)
    {
    case '8':
        return UI_INPUT_NESTED_MENU_ACTION_UP;

    case '2':
        return UI_INPUT_NESTED_MENU_ACTION_DOWN;

    case '4':
        return UI_INPUT_NESTED_MENU_ACTION_LEFT;

    case '6':
        return UI_INPUT_NESTED_MENU_ACTION_RIGHT;

    default:
        return UI_INPUT_NESTED_MENU_ACTION_NONE;
    }
}

/* Classifies one keypress for four-way adjust screens such as birth choices. */
ui_input_adjust_action ui_input_parse_adjust_key(int key)
{
    int direction = ui_input_movement_direction(key);

    if (ui_input_is_cancel_key(key))
        return UI_INPUT_ADJUST_ACTION_CANCEL;

    if (ui_input_is_accept_key(key))
        return UI_INPUT_ADJUST_ACTION_CONFIRM;

    switch (direction)
    {
    case 8:
    case DIRECTION_UP:
        return UI_INPUT_ADJUST_ACTION_PREV;

    case 2:
    case DIRECTION_DOWN:
        return UI_INPUT_ADJUST_ACTION_NEXT;

    case 4:
        return UI_INPUT_ADJUST_ACTION_DECREASE;

    case 6:
        return UI_INPUT_ADJUST_ACTION_INCREASE;

    default:
        return UI_INPUT_ADJUST_ACTION_NONE;
    }
}

/* Classifies one keypress for the normal options screen. */
ui_input_option_action ui_input_parse_option_key(int key)
{
    int direction = ui_input_movement_direction(key);

    if (ui_input_is_cancel_key(key) || ui_input_is_accept_key(key))
        return UI_INPUT_OPTION_ACTION_CLOSE;

    if (key == '-')
        return UI_INPUT_OPTION_ACTION_PREV;

    switch (direction)
    {
    case 8:
    case DIRECTION_UP:
        return UI_INPUT_OPTION_ACTION_PREV;

    case 2:
    case DIRECTION_DOWN:
        return UI_INPUT_OPTION_ACTION_NEXT;

    case 4:
        return UI_INPUT_OPTION_ACTION_DISABLE;

    case 6:
        return UI_INPUT_OPTION_ACTION_ENABLE;

    default:
        break;
    }

    if ((key == 't') || (key == '5') || (key == ' '))
        return UI_INPUT_OPTION_ACTION_TOGGLE;

    if (ui_input_is_yes_key(key))
        return UI_INPUT_OPTION_ACTION_ENABLE;

    if (ui_input_is_no_key(key))
        return UI_INPUT_OPTION_ACTION_DISABLE;

    return UI_INPUT_OPTION_ACTION_ERROR;
}

/* Classifies one keypress for knowledge-browser navigation. */
ui_input_browser ui_input_parse_browser_key(int key)
{
    int direction = ui_input_movement_direction(key);

    if (ui_input_is_cancel_key(key))
        return ui_input_browser_make(UI_INPUT_BROWSER_ACTION_CANCEL, 0);

    if ((key == 'R') || (key == 'r'))
        return ui_input_browser_make(UI_INPUT_BROWSER_ACTION_RECALL, 0);

    if (direction)
        return ui_input_browser_make(UI_INPUT_BROWSER_ACTION_MOVE, direction);

    return ui_input_browser_make(UI_INPUT_BROWSER_ACTION_NONE, 0);
}

/* Classifies one keypress for the message recall viewer. */
ui_input_message_recall_action ui_input_parse_message_recall_key(int key)
{
    if (ui_input_is_cancel_key(key))
        return UI_INPUT_MESSAGE_RECALL_ACTION_CANCEL;

    switch (key)
    {
    case '4':
        return UI_INPUT_MESSAGE_RECALL_ACTION_SCROLL_LEFT;

    case '6':
        return UI_INPUT_MESSAGE_RECALL_ACTION_SCROLL_RIGHT;

    case '=':
        return UI_INPUT_MESSAGE_RECALL_ACTION_SHOW;

    case '/':
        return UI_INPUT_MESSAGE_RECALL_ACTION_FIND;

    case 'p':
    case KTRL('P'):
    case ' ':
        return UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_PAGE;

    case '+':
        return UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_HALF_PAGE;

    case '8':
    case '\n':
    case '\r':
        return UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_LINE;

    case 'n':
    case KTRL('N'):
        return UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_PAGE;

    case '-':
        return UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_HALF_PAGE;

    case '2':
        return UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_LINE;

    default:
        return UI_INPUT_MESSAGE_RECALL_ACTION_NONE;
    }
}

/* Classifies one keypress for the generic file/help pager. */
ui_input_pager_action ui_input_parse_pager_key(int key)
{
    if (ui_input_is_cancel_key(key))
        return UI_INPUT_PAGER_ACTION_CANCEL;

    if (key == '?')
        return UI_INPUT_PAGER_ACTION_EXIT_HELP;

    if (key == '!')
        return UI_INPUT_PAGER_ACTION_TOGGLE_CASE;

    if (key == '&')
        return UI_INPUT_PAGER_ACTION_SHOW;

    if (key == '/')
        return UI_INPUT_PAGER_ACTION_FIND;

    if (key == '#')
        return UI_INPUT_PAGER_ACTION_GOTO_LINE;

    if ((key == '8') || (key == '='))
        return UI_INPUT_PAGER_ACTION_LINE_UP;

    if (key == '_')
        return UI_INPUT_PAGER_ACTION_HALF_PAGE_UP;

    if ((key == '9') || (key == '-'))
        return UI_INPUT_PAGER_ACTION_PAGE_UP;

    if (key == '7')
        return UI_INPUT_PAGER_ACTION_TOP;

    if ((key == '2') || ui_input_is_accept_key(key))
        return UI_INPUT_PAGER_ACTION_LINE_DOWN;

    if (key == '+')
        return UI_INPUT_PAGER_ACTION_HALF_PAGE_DOWN;

    if ((key == '3') || (key == ' '))
        return UI_INPUT_PAGER_ACTION_PAGE_DOWN;

    if (key == '1')
        return UI_INPUT_PAGER_ACTION_BOTTOM;

    return UI_INPUT_PAGER_ACTION_NONE;
}

/* Classifies one keypress for the semantic character-sheet screen. */
ui_input_character_sheet_action ui_input_parse_character_sheet_key(int key)
{
    if (ui_input_is_cancel_key(key) || ui_input_is_accept_key(key)
        || (key == 'q') || (key == 'Q'))
    {
        return UI_INPUT_CHARACTER_SHEET_ACTION_CLOSE;
    }

    switch (key)
    {
    case 'n':
    case 'N':
    case ' ':
        return UI_INPUT_CHARACTER_SHEET_ACTION_NOTES;

    case 'c':
    case 'C':
        return UI_INPUT_CHARACTER_SHEET_ACTION_CHANGE_NAME;

    case 's':
    case 'S':
        return UI_INPUT_CHARACTER_SHEET_ACTION_SAVE;

    case 'a':
    case 'A':
    case '\t':
        return UI_INPUT_CHARACTER_SHEET_ACTION_ABILITIES;

    case 'i':
    case 'I':
        return UI_INPUT_CHARACTER_SHEET_ACTION_SKILLS;

    default:
        return UI_INPUT_CHARACTER_SHEET_ACTION_NONE;
    }
}

/* Classifies one keypress for the interesting-grid targeting prompt. */
ui_input_target ui_input_parse_target_interesting_key(int key, int mode)
{
    int direction = ui_input_movement_direction(key);

    if (ui_input_is_cancel_key(key) || (key == 'q'))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CANCEL, 0);

    if ((key == ' ') || (key == '*') || (key == '+'))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_NEXT, 0);

    if (key == '-')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_PREV, 0);

    if (key == 'p')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_RECENTER, 0);

    if (key == 'm')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MANUAL, 0);

    if ((key == 't') || (key == '5') || (key == 'z'))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CONFIRM, 0);

    if ((mode & TARGET_KILL) && ui_input_is_accept_key(key))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CONFIRM, 0);

    if (direction)
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MOVE, direction);

    if (ui_input_is_accept_key(key))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_NONE, 0);

    return ui_input_target_make(UI_INPUT_TARGET_ACTION_ERROR, 0);
}

/* Classifies one keypress for the arbitrary-grid targeting prompt. */
ui_input_target ui_input_parse_target_arbitrary_key(int key, int mode)
{
    int direction = ui_input_movement_direction(key);

    if (ui_input_is_cancel_key(key) || (key == 'q'))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CANCEL, 0);

    if (key == 'p')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_RECENTER, 0);

    if (key == 'a')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_AUTO, 0);

    if ((key == 't') || (key == '5') || (key == 'z'))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CONFIRM, 0);

    if ((mode & TARGET_KILL) && ui_input_is_accept_key(key))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_CONFIRM, 0);

    if (direction)
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MOVE, direction);

    if (ui_input_is_accept_key(key))
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_NONE, 0);

    return ui_input_target_make(UI_INPUT_TARGET_ACTION_ERROR, 0);
}

/* Classifies one keypress for the aim-direction prompt. */
ui_input_target ui_input_parse_target_aim_key(int key)
{
    int direction = ui_input_movement_direction(key);

    if (key == '*')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_RETARGET, 0);

    if ((key == 'f') || (key == 'F') || (key == 't') || (key == '5')
        || (key == 'z'))
    {
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_USE_TARGET, 0);
    }

    if (key == '>')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MOVE, DIRECTION_DOWN);

    if (key == '<')
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MOVE, DIRECTION_UP);

    if (direction)
        return ui_input_target_make(UI_INPUT_TARGET_ACTION_MOVE, direction);

    return ui_input_target_make(UI_INPUT_TARGET_ACTION_ERROR, 0);
}
