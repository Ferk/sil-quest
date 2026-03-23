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

/* Renders one simple vertical text menu plus its details pane. */
void ui_simple_menu_render(cptr title, int title_row, int col,
    const ui_simple_menu_entry* entries, int entry_count, int highlight,
    cptr extra_details);
/* Reads one action from a simple vertical text menu. */
int ui_simple_menu_read_action(
    int* highlight, const ui_simple_menu_entry* entries, int entry_count);

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
/* Sets the preferred details-pane width in character cells. */
void ui_menu_set_details_width(int chars);
/* Publishes the visual preview shown beside the details text. */
void ui_menu_set_details_visual(int kind, int attr, int chr);
/* Marks the current menu frame as complete. */
void ui_menu_end(void);
/* Clears the exported semantic menu state. */
void ui_menu_clear(void);
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
/* Returns the requested details-pane width in characters. */
int ui_menu_get_details_width(void);
/* Returns the visual kind exported for the details pane. */
int ui_menu_get_details_visual_kind(void);
/* Returns the visual attr exported for the details pane. */
int ui_menu_get_details_visual_attr(void);
/* Returns the visual character exported for the details pane. */
int ui_menu_get_details_visual_char(void);

/* Publishes one modal text block plus its dismiss key. */
void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key);
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
/* Returns the modal revision used by the frontend cache. */
unsigned int ui_modal_get_revision(void);

#endif /* INCLUDED_UI_MODEL_H */
