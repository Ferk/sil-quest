/* File: ui-document.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral state for long scrollable text documents such as help files,
 * manuals, notes, and temporary report buffers.
 */

#include "angband.h"

#include "ui-document.h"
#include "ui-model.h"

#define UI_DOCUMENT_TITLE_MAX 128
#define UI_DOCUMENT_TEXT_MAX (MESSAGE_BUF * 8)

static bool ui_document_active = FALSE;
static unsigned int ui_document_revision = 1;
static char ui_document_title[UI_DOCUMENT_TITLE_MAX];
static char ui_document_text[UI_DOCUMENT_TEXT_MAX];
static byte ui_document_attrs[UI_DOCUMENT_TEXT_MAX];
static int ui_document_attrs_len = 0;
static int ui_document_top_line = 0;
static int ui_document_line_count = 0;

static void ui_document_touch(void)
{
    ui_document_revision++;
    if (ui_document_revision == 0)
        ui_document_revision = 1;
    ui_front_invalidate();
}

void ui_document_set(cptr title, cptr text, const byte* attrs, int attrs_len,
    int top_line, int line_count)
{
    size_t title_len = 0;
    size_t text_len = 0;
    size_t i;

    if (title)
        title_len = strlen(title);
    if (title_len >= sizeof(ui_document_title))
        title_len = sizeof(ui_document_title) - 1;

    if (title_len > 0)
        memcpy(ui_document_title, title, title_len);
    ui_document_title[title_len] = '\0';

    if (text)
        text_len = strlen(text);
    if (text_len >= sizeof(ui_document_text))
        text_len = sizeof(ui_document_text) - 1;

    if (text_len > 0)
        memcpy(ui_document_text, text, text_len);
    ui_document_text[text_len] = '\0';

    ui_document_attrs_len = (int)text_len;
    for (i = 0; i < text_len; i++)
    {
        if (attrs && ((int)i < attrs_len))
            ui_document_attrs[i] = attrs[i];
        else
            ui_document_attrs[i] = TERM_WHITE;
    }

    ui_document_top_line = MAX(top_line, 0);
    ui_document_line_count = MAX(line_count, 0);
    ui_document_active = TRUE;
    ui_document_touch();
}

void ui_document_clear(void)
{
    ui_document_active = FALSE;
    ui_document_title[0] = '\0';
    ui_document_text[0] = '\0';
    ui_document_attrs_len = 0;
    ui_document_top_line = 0;
    ui_document_line_count = 0;
    ui_document_touch();
}

bool ui_document_is_active(void) { return ui_document_active; }

const char* ui_document_get_title(void) { return ui_document_title; }

int ui_document_get_title_len(void) { return (int)strlen(ui_document_title); }

const char* ui_document_get_text(void) { return ui_document_text; }

int ui_document_get_text_len(void) { return (int)strlen(ui_document_text); }

const byte* ui_document_get_attrs(void) { return ui_document_attrs; }

int ui_document_get_attrs_len(void) { return ui_document_attrs_len; }

int ui_document_get_top_line(void) { return ui_document_top_line; }

int ui_document_get_line_count(void) { return ui_document_line_count; }

unsigned int ui_document_get_revision(void) { return ui_document_revision; }
