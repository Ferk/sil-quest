/* File: ui-model.h */

/*
 * Copyright (c) 2026
 *
 * Frontend-neutral helpers for building structured UI text with per-character
 * terminal attributes.
 */

#ifndef INCLUDED_UI_MODEL_H
#define INCLUDED_UI_MODEL_H

#include "angband.h"

typedef struct ui_text_builder ui_text_builder;
typedef struct ui_menu_item ui_menu_item;

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
    char label[UI_MENU_LABEL_MAX];
    int nav_len;
    char nav[UI_MENU_NAV_MAX];
};

void ui_text_builder_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size);
void ui_text_builder_append(
    ui_text_builder* builder, cptr text, byte attr);
void ui_text_builder_newline(ui_text_builder* builder, byte attr);
void ui_text_builder_append_line(
    ui_text_builder* builder, cptr text, byte attr);
int ui_text_builder_length(const ui_text_builder* builder);

void ui_menu_begin(void);
void ui_menu_add(
    int x, int y, int w, int h, int key, int selected, int attr, cptr label);
void ui_menu_add_with_nav(int x, int y, int w, int h, int key, int selected,
    int attr, cptr label, cptr nav);
void ui_menu_set_active_column(int x);
void ui_menu_set_text(cptr text, const byte* attrs, int attrs_len);
void ui_menu_set_details(cptr text, const byte* attrs, int attrs_len);
void ui_menu_set_details_width(int chars);
void ui_menu_set_details_visual(int kind, int attr, int chr);
void ui_menu_end(void);
void ui_menu_clear(void);
const ui_menu_item* ui_menu_get_items(void);
int ui_menu_get_item_count(void);
int ui_menu_get_selected_index(void);
int ui_menu_select_index(int index);
int ui_menu_get_active_column(void);
unsigned int ui_menu_get_revision(void);
const char* ui_menu_get_text(void);
int ui_menu_get_text_len(void);
const byte* ui_menu_get_attrs(void);
int ui_menu_get_attrs_len(void);
const char* ui_menu_get_details(void);
int ui_menu_get_details_len(void);
const byte* ui_menu_get_details_attrs(void);
int ui_menu_get_details_attrs_len(void);
int ui_menu_get_details_width(void);
int ui_menu_get_details_visual_kind(void);
int ui_menu_get_details_visual_attr(void);
int ui_menu_get_details_visual_char(void);

void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key);
void ui_modal_clear(void);
const char* ui_modal_get_text(void);
int ui_modal_get_text_len(void);
const byte* ui_modal_get_attrs(void);
int ui_modal_get_attrs_len(void);
int ui_modal_get_dismiss_key(void);
unsigned int ui_modal_get_revision(void);

#endif /* INCLUDED_UI_MODEL_H */
