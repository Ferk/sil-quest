/* File: ui-model.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL 
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral helpers for building structured UI text with per-character
 * terminal attributes.
 */

#ifndef INCLUDED_UI_MODEL_H
#define INCLUDED_UI_MODEL_H

#include "angband.h"

typedef struct ui_text_builder ui_text_builder;
typedef struct ui_menu_item ui_menu_item;
typedef struct ui_simple_menu_entry ui_simple_menu_entry;
typedef struct ui_text_output_state ui_text_output_state;
typedef enum ui_modal_kind ui_modal_kind;
typedef enum ui_prompt_kind ui_prompt_kind;
typedef bool (*ui_prompt_render_hook)(int row, int col);
typedef void (*ui_prompt_clear_hook)(void);
typedef void (*ui_menu_render_hook)(void);

enum
{
    UI_MENU_VISUAL_NONE = 0,
    UI_MENU_VISUAL_TEXT = 1,
    UI_MENU_VISUAL_TILE = 2
};

struct ui_text_builder
{
    char* text;
    byte* attrs;
    size_t size;
    size_t off;
};

struct ui_text_output_state
{
    void (*hook)(byte a, cptr str);
    int wrap;
    int indent;
};

#define UI_MENU_LABEL_MAX 64
#define UI_MENU_NAV_MAX 32

struct ui_menu_item
{
    int x;
    int y;
    int w;
    int h;
    int key;
    int selected;
    int attr;
    int visual_kind;
    int visual_attr;
    int visual_char;
    char label[UI_MENU_LABEL_MAX];
    int nav_len;
    char nav[UI_MENU_NAV_MAX];
};

struct ui_simple_menu_entry
{
    int key;
    int row;
    cptr label;
    cptr details;
};

enum ui_prompt_kind
{
    UI_PROMPT_KIND_NONE = 0,
    UI_PROMPT_KIND_MESSAGE = 1,
    UI_PROMPT_KIND_GENERIC = 2,
    UI_PROMPT_KIND_YES_NO = 3,
    UI_PROMPT_KIND_TARGET = 4,
    UI_PROMPT_KIND_MORE = 5
};

enum ui_modal_kind
{
    UI_MODAL_KIND_GENERIC = 0,
    UI_MODAL_KIND_MESSAGE_HISTORY = 1
};

/* Starts writing into one text-plus-attrs builder. */
void ui_text_builder_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size);
/* Appends one text run with a uniform terminal attr. */
void ui_text_builder_append(
    ui_text_builder* builder, cptr text, byte attr);
/* Appends a newline to one text builder. */
void ui_text_builder_newline(ui_text_builder* builder, byte attr);
/* Appends one line of text followed by a newline. */
void ui_text_builder_append_line(
    ui_text_builder* builder, cptr text, byte attr);
/* Returns the current text length stored in one builder. */
int ui_text_builder_length(const ui_text_builder* builder);
/* Saves the current text_out() globals and applies a temporary override. */
void ui_text_output_begin(ui_text_output_state* state,
    void (*hook)(byte a, cptr str), int indent, int wrap);
/* Restores the previous text_out() globals after a temporary override. */
void ui_text_output_end(const ui_text_output_state* state);

/* Publishes the semantic state for one simple vertical text menu. */
void ui_model_publish_simple_menu(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details, int details_width, int details_visual_kind,
    int details_visual_attr, int details_visual_char);
/* Renders one simple vertical text menu plus its details pane. */
void ui_simple_menu_render(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details);
/* Renders one simple vertical text menu with custom details-pane settings. */
void ui_simple_menu_render_custom(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details, int details_width, int details_visual_kind,
    int details_visual_attr, int details_visual_char);
/* Reads one action from a simple vertical text menu. */
int ui_simple_menu_read_action(
    int* highlight, const ui_simple_menu_entry* entries, int entry_count);
/* Runs one command-style simple menu and returns the chosen 1-based index. */
int ui_command_menu_choose(cptr title, int col,
    const ui_simple_menu_entry* entries, int entry_count, int* highlight,
    cptr extra_details, int escape_choice, int escape_result);
/* Registers frontend-specific rendering hooks for prompts and menus. */
void ui_front_set_hooks(ui_prompt_render_hook prompt_render,
    ui_prompt_clear_hook prompt_clear, ui_menu_render_hook menu_render);

/* Keeps one paged menu selection visible inside its current viewport. */
void ui_menu_scroll_selection_into_view(
    int current, int* top, int count, int page_rows);
/* Moves one two-column menu selection using viewport-aware semantics. */
void ui_menu_move_two_column_selection(int direction, int page_rows,
    int* column, int* left_cur, int left_top, int left_count, int* right_cur,
    int right_top, int right_count);

/* Begins publishing a fresh semantic menu frame. */
void ui_menu_begin(void);
/* Adds one menu item with an explicit visual payload. */
void ui_menu_add_visual(int x, int y, int w, int h, int key, int selected,
    int attr, int visual_kind, int visual_attr, int visual_char, cptr label);
/* Adds a plain-text menu item with no visual or custom navigation. */
void ui_menu_add(
    int x, int y, int w, int h, int key, int selected, int attr, cptr label);
/* Adds one fully-specified semantic menu item to the current frame. */
void ui_menu_add_visual_with_nav(int x, int y, int w, int h, int key,
    int selected, int attr, int visual_kind, int visual_attr, int visual_char,
    cptr label, cptr nav);
/* Adds one plain-text menu item with synthetic navigation hints. */
void ui_menu_add_with_nav(int x, int y, int w, int h, int key, int selected,
    int attr, cptr label, cptr nav);
/* Restricts menu selection to one column, or all columns with `-1`. */
void ui_menu_set_active_column(int x);
/* Publishes the menu's main text block. */
void ui_menu_set_text(cptr text, const byte* attrs, int attrs_len);
/* Publishes the menu's details pane text block. */
void ui_menu_set_details(cptr text, const byte* attrs, int attrs_len);
/* Publishes the menu's bottom summary text block. */
void ui_menu_set_summary(cptr text, const byte* attrs, int attrs_len);
/* Sets the preferred details-pane width in character cells. */
void ui_menu_set_details_width(int chars);
/* Sets the preferred details-pane height in text rows. */
void ui_menu_set_details_rows(int rows);
/* Sets the preferred summary-block height in text rows. */
void ui_menu_set_summary_rows(int rows);
/* Publishes the visual preview shown beside the details text. */
void ui_menu_set_details_visual(int kind, int attr, int chr);
/* Marks the current menu frame as complete. */
void ui_menu_end(void);
/* Clears the exported semantic menu state. */
void ui_menu_clear(void);
/* Controls whether the last menu may remain visible during follow-up prompts. */
void ui_menu_set_snapshot_retained(bool retained);
/* Returns whether follow-up prompts may reuse the last rendered menu. */
bool ui_menu_snapshot_retained(void);
/* Returns the current semantic menu item array. */
const ui_menu_item* ui_menu_get_items(void);
/* Returns how many semantic menu items are currently exported. */
int ui_menu_get_item_count(void);
/* Returns the currently selected menu item index, if any. */
int ui_menu_get_selected_index(void);
/* Selects one menu item and updates the active column state. */
int ui_menu_select_index(int index);
/* Returns the active menu column, or `-1` when all are active. */
int ui_menu_get_active_column(void);
/* Returns the menu revision used by the frontend cache. */
unsigned int ui_menu_get_revision(void);
/* Returns the current menu text buffer. */
const char* ui_menu_get_text(void);
/* Returns the current menu text length. */
int ui_menu_get_text_len(void);
/* Returns the attrs paired with the menu text buffer. */
const byte* ui_menu_get_attrs(void);
/* Returns how many attrs are valid in the menu text buffer. */
int ui_menu_get_attrs_len(void);
/* Returns the current menu details text buffer. */
const char* ui_menu_get_details(void);
/* Returns the current menu details text length. */
int ui_menu_get_details_len(void);
/* Returns the attrs paired with the menu details buffer. */
const byte* ui_menu_get_details_attrs(void);
/* Returns how many attrs are valid in the menu details buffer. */
int ui_menu_get_details_attrs_len(void);
/* Returns the current menu summary text buffer. */
const char* ui_menu_get_summary(void);
/* Returns the current menu summary text length. */
int ui_menu_get_summary_len(void);
/* Returns the attrs paired with the menu summary buffer. */
const byte* ui_menu_get_summary_attrs(void);
/* Returns how many attrs are valid in the menu summary buffer. */
int ui_menu_get_summary_attrs_len(void);
/* Returns the requested details-pane width in characters. */
int ui_menu_get_details_width(void);
/* Returns the requested details-pane height in rows. */
int ui_menu_get_details_rows(void);
/* Returns the requested summary-block height in rows. */
int ui_menu_get_summary_rows(void);
/* Returns the visual kind exported for the details pane. */
int ui_menu_get_details_visual_kind(void);
/* Returns the visual attr exported for the details pane. */
int ui_menu_get_details_visual_attr(void);
/* Returns the visual character exported for the details pane. */
int ui_menu_get_details_visual_char(void);
/* Asks the active frontend to render the current semantic menu state. */
void ui_menu_render_current(void);

/* Enables one frontend-neutral semantic message-history viewer path. */
void ui_message_recall_set_semantic_enabled(bool enabled);
/* Returns whether message recall should use the semantic viewer path. */
bool ui_message_recall_semantic_enabled(void);

/* Publishes one modal text block plus its dismiss key. */
void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key);
/* Publishes one modal text block plus its dismiss key and semantic kind. */
void ui_modal_set_kind(cptr text, const byte* attrs, int attrs_len,
    int dismiss_key, ui_modal_kind kind);
/* Clears the exported modal state. */
void ui_modal_clear(void);
/* Returns the current modal text buffer. */
const char* ui_modal_get_text(void);
/* Returns the current modal text length. */
int ui_modal_get_text_len(void);
/* Returns the attrs paired with the modal text buffer. */
const byte* ui_modal_get_attrs(void);
/* Returns how many attrs are valid in the modal text buffer. */
int ui_modal_get_attrs_len(void);
/* Returns the modal's configured dismiss key. */
int ui_modal_get_dismiss_key(void);
/* Returns the semantic kind exported for the current modal. */
ui_modal_kind ui_modal_get_kind(void);
/* Returns the modal revision used by the frontend cache. */
unsigned int ui_modal_get_revision(void);

/* Plans the semantic kind and inline-more hint for the next top-line prompt. */
void ui_prompt_plan(ui_prompt_kind kind, bool has_more_hint);
/* Publishes one prompt text buffer with a uniform terminal attribute. */
void ui_prompt_set_text(
    cptr text, byte attr, ui_prompt_kind kind, bool has_more_hint);
/* Writes one colored text run into the semantic top-line message buffer. */
void ui_prompt_putstr(int col, byte attr, cptr text);
/* Clears the current semantic prompt state. */
void ui_prompt_clear(void);
/* Publishes and optionally renders one semantic top-line prompt. */
bool ui_prompt_render(int row, int col, byte attr, cptr text);
/* Publishes and optionally renders one semantic "-more-" prompt. */
void ui_prompt_show_more(int col, byte attr);
/* Clears the semantic prompt state and any frontend-specific prompt line. */
void ui_prompt_clear_line(void);
/* Returns the current semantic prompt kind. */
ui_prompt_kind ui_prompt_get_kind(void);
/* Returns whether the current prompt has extra inline detail to cycle. */
bool ui_prompt_get_more_hint(void);
/* Returns the current semantic prompt text buffer. */
const char* ui_prompt_get_text(void);
/* Returns the current semantic prompt text length. */
int ui_prompt_get_text_len(void);
/* Returns the attrs paired with the semantic prompt text. */
const byte* ui_prompt_get_attrs(void);
/* Returns how many attrs are valid in the semantic prompt text buffer. */
int ui_prompt_get_attrs_len(void);
/* Returns the prompt revision used by the frontend cache. */
unsigned int ui_prompt_get_revision(void);

/* Marks the start of one saved-screen scope such as screen_save(). */
void ui_saved_screen_begin(void);
/* Marks the end of one saved-screen scope such as screen_load(). */
void ui_saved_screen_end(void);
/* Returns the current nested saved-screen depth. */
int ui_saved_screen_get_depth(void);
/* Returns whether the last saved screen was restored and not reopened yet. */
bool ui_saved_screen_was_restored(void);
/* Returns the revision used by frontends to detect saved-screen transitions. */
unsigned int ui_saved_screen_get_revision(void);

#endif /* INCLUDED_UI_MODEL_H */
