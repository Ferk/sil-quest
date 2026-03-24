/* File: ui-input.h
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

#ifndef INCLUDED_UI_INPUT_H
#define INCLUDED_UI_INPUT_H

#include "angband.h"

typedef enum ui_input_target_action ui_input_target_action;
typedef enum ui_input_adjust_action ui_input_adjust_action;
typedef enum ui_input_option_action ui_input_option_action;
typedef enum ui_input_simple_menu_action ui_input_simple_menu_action;
typedef enum ui_input_nested_menu_action ui_input_nested_menu_action;
typedef enum ui_input_browser_action ui_input_browser_action;
typedef enum ui_input_message_recall_action ui_input_message_recall_action;
typedef enum ui_input_pager_action ui_input_pager_action;
typedef struct ui_input_target ui_input_target;
typedef struct ui_input_browser ui_input_browser;

enum ui_input_target_action
{
    UI_INPUT_TARGET_ACTION_NONE = 0,
    UI_INPUT_TARGET_ACTION_CANCEL,
    UI_INPUT_TARGET_ACTION_NEXT,
    UI_INPUT_TARGET_ACTION_PREV,
    UI_INPUT_TARGET_ACTION_RECENTER,
    UI_INPUT_TARGET_ACTION_MANUAL,
    UI_INPUT_TARGET_ACTION_AUTO,
    UI_INPUT_TARGET_ACTION_CONFIRM,
    UI_INPUT_TARGET_ACTION_RETARGET,
    UI_INPUT_TARGET_ACTION_USE_TARGET,
    UI_INPUT_TARGET_ACTION_MOVE,
    UI_INPUT_TARGET_ACTION_ERROR
};

struct ui_input_target
{
    ui_input_target_action action;
    int direction;
};

enum ui_input_adjust_action
{
    UI_INPUT_ADJUST_ACTION_NONE = 0,
    UI_INPUT_ADJUST_ACTION_CANCEL,
    UI_INPUT_ADJUST_ACTION_CONFIRM,
    UI_INPUT_ADJUST_ACTION_PREV,
    UI_INPUT_ADJUST_ACTION_NEXT,
    UI_INPUT_ADJUST_ACTION_DECREASE,
    UI_INPUT_ADJUST_ACTION_INCREASE
};

enum ui_input_option_action
{
    UI_INPUT_OPTION_ACTION_NONE = 0,
    UI_INPUT_OPTION_ACTION_CLOSE,
    UI_INPUT_OPTION_ACTION_PREV,
    UI_INPUT_OPTION_ACTION_NEXT,
    UI_INPUT_OPTION_ACTION_TOGGLE,
    UI_INPUT_OPTION_ACTION_ENABLE,
    UI_INPUT_OPTION_ACTION_DISABLE,
    UI_INPUT_OPTION_ACTION_ERROR
};

enum ui_input_simple_menu_action
{
    UI_INPUT_SIMPLE_MENU_ACTION_NONE = 0,
    UI_INPUT_SIMPLE_MENU_ACTION_CANCEL,
    UI_INPUT_SIMPLE_MENU_ACTION_CHOOSE,
    UI_INPUT_SIMPLE_MENU_ACTION_PREV,
    UI_INPUT_SIMPLE_MENU_ACTION_NEXT
};

enum ui_input_nested_menu_action
{
    UI_INPUT_NESTED_MENU_ACTION_NONE = 0,
    UI_INPUT_NESTED_MENU_ACTION_CANCEL,
    UI_INPUT_NESTED_MENU_ACTION_CHOOSE,
    UI_INPUT_NESTED_MENU_ACTION_UP,
    UI_INPUT_NESTED_MENU_ACTION_DOWN,
    UI_INPUT_NESTED_MENU_ACTION_LEFT,
    UI_INPUT_NESTED_MENU_ACTION_RIGHT
};

enum ui_input_browser_action
{
    UI_INPUT_BROWSER_ACTION_NONE = 0,
    UI_INPUT_BROWSER_ACTION_CANCEL,
    UI_INPUT_BROWSER_ACTION_RECALL,
    UI_INPUT_BROWSER_ACTION_MOVE
};

enum ui_input_message_recall_action
{
    UI_INPUT_MESSAGE_RECALL_ACTION_NONE = 0,
    UI_INPUT_MESSAGE_RECALL_ACTION_CANCEL,
    UI_INPUT_MESSAGE_RECALL_ACTION_SCROLL_LEFT,
    UI_INPUT_MESSAGE_RECALL_ACTION_SCROLL_RIGHT,
    UI_INPUT_MESSAGE_RECALL_ACTION_SHOW,
    UI_INPUT_MESSAGE_RECALL_ACTION_FIND,
    UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_PAGE,
    UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_HALF_PAGE,
    UI_INPUT_MESSAGE_RECALL_ACTION_OLDER_LINE,
    UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_PAGE,
    UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_HALF_PAGE,
    UI_INPUT_MESSAGE_RECALL_ACTION_NEWER_LINE
};

enum ui_input_pager_action
{
    UI_INPUT_PAGER_ACTION_NONE = 0,
    UI_INPUT_PAGER_ACTION_CANCEL,
    UI_INPUT_PAGER_ACTION_EXIT_HELP,
    UI_INPUT_PAGER_ACTION_TOGGLE_CASE,
    UI_INPUT_PAGER_ACTION_SHOW,
    UI_INPUT_PAGER_ACTION_FIND,
    UI_INPUT_PAGER_ACTION_GOTO_LINE,
    UI_INPUT_PAGER_ACTION_LINE_UP,
    UI_INPUT_PAGER_ACTION_HALF_PAGE_UP,
    UI_INPUT_PAGER_ACTION_PAGE_UP,
    UI_INPUT_PAGER_ACTION_TOP,
    UI_INPUT_PAGER_ACTION_LINE_DOWN,
    UI_INPUT_PAGER_ACTION_HALF_PAGE_DOWN,
    UI_INPUT_PAGER_ACTION_PAGE_DOWN,
    UI_INPUT_PAGER_ACTION_BOTTOM
};

struct ui_input_browser
{
    ui_input_browser_action action;
    int direction;
};

/* Returns whether one keypress should be treated as Enter/Return. */
bool ui_input_is_accept_key(int key);
/* Returns whether one keypress should cancel the current interaction. */
bool ui_input_is_cancel_key(int key);
/* Returns whether one keypress should delete the previous character. */
bool ui_input_is_backspace_key(int key);
/* Returns whether one keypress should confirm a yes/no prompt. */
bool ui_input_is_yes_key(int key);
/* Returns whether one keypress should reject a yes/no prompt. */
bool ui_input_is_no_key(int key);
/* Converts one alphabetic menu key into a zero-based choice index. */
int ui_input_alpha_choice_index(int key);
/* Classifies one keypress for a simple vertical text menu. */
ui_input_simple_menu_action ui_input_parse_simple_menu_key(int key);
/* Classifies one keypress for a nested menu with directional navigation. */
ui_input_nested_menu_action ui_input_parse_nested_menu_key(int key);
/* Classifies one keypress for four-way adjust screens such as birth choices. */
ui_input_adjust_action ui_input_parse_adjust_key(int key);
/* Classifies one keypress for the normal options screen. */
ui_input_option_action ui_input_parse_option_key(int key);
/* Classifies one keypress for knowledge-browser navigation. */
ui_input_browser ui_input_parse_browser_key(int key);
/* Classifies one keypress for the message recall viewer. */
ui_input_message_recall_action ui_input_parse_message_recall_key(int key);
/* Classifies one keypress for the generic file/help pager. */
ui_input_pager_action ui_input_parse_pager_key(int key);
/* Classifies one keypress for the interesting-grid targeting prompt. */
ui_input_target ui_input_parse_target_interesting_key(int key, int mode);
/* Classifies one keypress for the arbitrary-grid targeting prompt. */
ui_input_target ui_input_parse_target_arbitrary_key(int key, int mode);
/* Classifies one keypress for the aim-direction prompt. */
ui_input_target ui_input_parse_target_aim_key(int key);

#endif /* INCLUDED_UI_INPUT_H */
