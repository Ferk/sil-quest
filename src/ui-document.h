/* File: ui-document.h
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

#ifndef INCLUDED_UI_DOCUMENT_H
#define INCLUDED_UI_DOCUMENT_H

#include "angband.h"

void ui_document_set(cptr title, cptr text, const byte* attrs, int attrs_len,
    int top_line, int line_count);
void ui_document_clear(void);
bool ui_document_is_active(void);
const char* ui_document_get_title(void);
int ui_document_get_title_len(void);
const char* ui_document_get_text(void);
int ui_document_get_text_len(void);
const byte* ui_document_get_attrs(void);
int ui_document_get_attrs_len(void);
int ui_document_get_top_line(void);
int ui_document_get_line_count(void);
unsigned int ui_document_get_revision(void);

#endif /* INCLUDED_UI_DOCUMENT_H */
