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

struct ui_text_builder
{
    char* text;
    byte* attrs;
    size_t size;
    size_t off;
};

#define UI_MENU_LABEL_MAX 64

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
void ui_menu_set_text(cptr text, const byte* attrs, int attrs_len);
void ui_menu_set_details(cptr text, const byte* attrs, int attrs_len);
void ui_menu_end(void);
void ui_menu_clear(void);
const ui_menu_item* ui_menu_get_items(void);
int ui_menu_get_item_count(void);
int ui_menu_get_selected_index(void);
int ui_menu_select_index(int index);
unsigned int ui_menu_get_revision(void);
const char* ui_menu_get_text(void);
int ui_menu_get_text_len(void);
const byte* ui_menu_get_attrs(void);
int ui_menu_get_attrs_len(void);
const char* ui_menu_get_details(void);
int ui_menu_get_details_len(void);
const byte* ui_menu_get_details_attrs(void);
int ui_menu_get_details_attrs_len(void);

void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key);
void ui_modal_clear(void);
const char* ui_modal_get_text(void);
int ui_modal_get_text_len(void);
const byte* ui_modal_get_attrs(void);
int ui_modal_get_attrs_len(void);
int ui_modal_get_dismiss_key(void);
unsigned int ui_modal_get_revision(void);

#endif /* INCLUDED_UI_MODEL_H */
