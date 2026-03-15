/* File: ui-model.c */

/*
 * Copyright (c) 2026
 *
 * Frontend-neutral methods for building structured UI text
 * with a parallel attribute buffer.
 */

#include "angband.h"

#include "ui-model.h"

#define UI_MENU_ITEMS_MAX 32
#define UI_MENU_TEXT_MAX 16384

static ui_menu_item ui_menu_items[UI_MENU_ITEMS_MAX];
static int ui_menu_item_count = 0;
static int ui_menu_selected_hint = -1;
static unsigned int ui_menu_revision = 1;
static char ui_menu_text[UI_MENU_TEXT_MAX];
static byte ui_menu_attrs[UI_MENU_TEXT_MAX];
static int ui_menu_attrs_len = 0;
static char ui_menu_details[UI_MENU_TEXT_MAX];
static byte ui_menu_details_attrs[UI_MENU_TEXT_MAX];
static int ui_menu_details_attrs_len = 0;
static unsigned int ui_modal_revision = 1;
static char ui_modal_text[UI_MENU_TEXT_MAX];
static byte ui_modal_attrs[UI_MENU_TEXT_MAX];
static int ui_modal_attrs_len = 0;
static int ui_modal_dismiss_key = 0;

static void ui_menu_touch(void)
{
    ui_menu_revision++;
    if (ui_menu_revision == 0)
        ui_menu_revision = 1;
}

static void ui_modal_touch(void)
{
    ui_modal_revision++;
    if (ui_modal_revision == 0)
        ui_modal_revision = 1;
}

static void ui_model_set_buffer_text(char* text_buf, byte* attr_buf, int* attr_len,
    size_t buf_size, cptr text, const byte* attrs, int attrs_len)
{
    size_t text_len = 0;
    size_t i;

    if (!text_buf || !attr_buf || !attr_len || (buf_size == 0))
        return;

    if (text)
        text_len = strlen(text);
    if (text_len >= buf_size)
        text_len = buf_size - 1;

    if (text_len > 0)
        memcpy(text_buf, text, text_len);
    text_buf[text_len] = '\0';

    *attr_len = (int)text_len;
    for (i = 0; i < text_len; i++)
    {
        if (attrs && ((int)i < attrs_len))
            attr_buf[i] = attrs[i];
        else
            attr_buf[i] = TERM_WHITE;
    }
}

void ui_text_builder_init(
    ui_text_builder* builder, char* text, byte* attrs, size_t size)
{
    if (!builder)
        return;

    builder->text = text;
    builder->attrs = attrs;
    builder->size = size;
    builder->off = 0;

    if (text && (size > 0))
        text[0] = '\0';
}

void ui_text_builder_append(
    ui_text_builder* builder, cptr text, byte attr)
{
    const char* s = text ? text : "";

    if (!builder || !builder->text || !builder->attrs || (builder->size == 0))
        return;

    while (*s && (builder->off + 1 < builder->size))
    {
        builder->text[builder->off] = *s++;
        builder->attrs[builder->off] = attr;
        builder->off++;
    }

    builder->text[builder->off] = '\0';
}

void ui_text_builder_newline(ui_text_builder* builder, byte attr)
{
    if (!builder || !builder->text || !builder->attrs || (builder->size == 0)
        || (builder->off + 1 >= builder->size))
    {
        return;
    }

    builder->text[builder->off] = '\n';
    builder->attrs[builder->off] = attr;
    builder->off++;
    builder->text[builder->off] = '\0';
}

void ui_text_builder_append_line(
    ui_text_builder* builder, cptr text, byte attr)
{
    ui_text_builder_append(builder, text, attr);
    ui_text_builder_newline(builder, attr);
}

int ui_text_builder_length(const ui_text_builder* builder)
{
    if (!builder)
        return 0;

    return (int)builder->off;
}

void ui_menu_begin(void)
{
    ui_menu_item_count = 0;
    ui_menu_selected_hint = -1;
    ui_menu_text[0] = '\0';
    ui_menu_attrs_len = 0;
    ui_menu_details[0] = '\0';
    ui_menu_details_attrs_len = 0;
    ui_menu_touch();
}

void ui_menu_add(
    int x, int y, int w, int h, int key, int selected, int attr, cptr label)
{
    ui_menu_item* item;

    if ((w <= 0) || (h <= 0) || (key <= 0))
        return;

    if (ui_menu_item_count >= UI_MENU_ITEMS_MAX)
        return;

    item = &ui_menu_items[ui_menu_item_count];
    item->x = x;
    item->y = y;
    item->w = w;
    item->h = h;
    item->key = key;
    item->selected = selected ? 1 : 0;
    item->attr = attr;
    my_strcpy(item->label, label ? label : "", sizeof(item->label));
    if (selected)
        ui_menu_selected_hint = ui_menu_item_count;
    ui_menu_item_count++;
    ui_menu_touch();
}

void ui_menu_set_text(cptr text, const byte* attrs, int attrs_len)
{
    ui_model_set_buffer_text(ui_menu_text, ui_menu_attrs, &ui_menu_attrs_len,
        sizeof(ui_menu_text), text, attrs, attrs_len);
    ui_menu_touch();
}

void ui_menu_set_details(cptr text, const byte* attrs, int attrs_len)
{
    ui_model_set_buffer_text(ui_menu_details, ui_menu_details_attrs,
        &ui_menu_details_attrs_len, sizeof(ui_menu_details), text, attrs,
        attrs_len);
    ui_menu_touch();
}

void ui_menu_end(void)
{
    ui_menu_touch();
}

void ui_menu_clear(void)
{
    ui_menu_item_count = 0;
    ui_menu_selected_hint = -1;
    ui_menu_text[0] = '\0';
    ui_menu_attrs_len = 0;
    ui_menu_details[0] = '\0';
    ui_menu_details_attrs_len = 0;
    ui_menu_touch();
}

const ui_menu_item* ui_menu_get_items(void)
{
    return ui_menu_items;
}

int ui_menu_get_item_count(void)
{
    return ui_menu_item_count;
}

int ui_menu_get_selected_index(void)
{
    int i;

    if ((ui_menu_selected_hint >= 0) && (ui_menu_selected_hint < ui_menu_item_count))
        return ui_menu_selected_hint;

    for (i = 0; i < ui_menu_item_count; i++)
    {
        if (ui_menu_items[i].selected)
            return i;
    }

    return -1;
}

int ui_menu_select_index(int index)
{
    int i;
    int column_x;

    if ((index < 0) || (index >= ui_menu_item_count))
        return 0;

    column_x = ui_menu_items[index].x;
    for (i = 0; i < ui_menu_item_count; i++)
    {
        if (ui_menu_items[i].x == column_x)
            ui_menu_items[i].selected = (i == index) ? 1 : 0;
    }
    ui_menu_selected_hint = index;
    ui_menu_touch();

    return 1;
}

unsigned int ui_menu_get_revision(void)
{
    return ui_menu_revision;
}

const char* ui_menu_get_text(void)
{
    return ui_menu_text;
}

int ui_menu_get_text_len(void)
{
    return (int)strlen(ui_menu_text);
}

const byte* ui_menu_get_attrs(void)
{
    return ui_menu_attrs;
}

int ui_menu_get_attrs_len(void)
{
    return ui_menu_attrs_len;
}

const char* ui_menu_get_details(void)
{
    return ui_menu_details;
}

int ui_menu_get_details_len(void)
{
    return (int)strlen(ui_menu_details);
}

const byte* ui_menu_get_details_attrs(void)
{
    return ui_menu_details_attrs;
}

int ui_menu_get_details_attrs_len(void)
{
    return ui_menu_details_attrs_len;
}

void ui_modal_set(cptr text, const byte* attrs, int attrs_len, int dismiss_key)
{
    ui_model_set_buffer_text(ui_modal_text, ui_modal_attrs, &ui_modal_attrs_len,
        sizeof(ui_modal_text), text, attrs, attrs_len);
    ui_modal_dismiss_key = dismiss_key;
    ui_modal_touch();
}

void ui_modal_clear(void)
{
    ui_modal_text[0] = '\0';
    ui_modal_attrs_len = 0;
    ui_modal_dismiss_key = 0;
    ui_modal_touch();
}

const char* ui_modal_get_text(void)
{
    return ui_modal_text;
}

int ui_modal_get_text_len(void)
{
    return (int)strlen(ui_modal_text);
}

const byte* ui_modal_get_attrs(void)
{
    return ui_modal_attrs;
}

int ui_modal_get_attrs_len(void)
{
    return ui_modal_attrs_len;
}

int ui_modal_get_dismiss_key(void)
{
    return ui_modal_dismiss_key;
}

unsigned int ui_modal_get_revision(void)
{
    return ui_modal_revision;
}
