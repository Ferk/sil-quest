/* File: ui-knowledge.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Helpers for the knowledge screens and their two-column browser UIs.
 */

#ifndef INCLUDED_UI_KNOWLEDGE_H
#define INCLUDED_UI_KNOWLEDGE_H

#include "angband.h"

typedef struct monster_list_entry monster_list_entry;
typedef struct object_list_entry object_list_entry;

/* Shared page size for the two-column knowledge browsers. */
#define UI_KNOWLEDGE_BROWSER_ROWS 16

struct monster_list_entry
{
    s16b r_idx;
    byte amount;
};

struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval;
    int sval;
};

/* Runs one interaction step for the top-level knowledge menu. */
int ui_knowledge_menu_choose(int* highlight);
/* Formats the display name for one artefact browser entry. */
void ui_knowledge_artefact_name(int a_idx, char* name, size_t name_size);
/* Publishes the semantic artefact browser UI for the current selection. */
void ui_knowledge_publish_artefacts(cptr group_text[], int grp_idx[],
    int grp_cnt, int grp_cur, int grp_top, int artefact_idx[],
    int artefact_cnt, int artefact_cur, int artefact_top, int column);
/* Shows the full recall modal for one artefact entry. */
void ui_knowledge_recall_artefact(int a_idx);
/* Publishes the semantic monster browser UI for the current selection. */
void ui_knowledge_publish_monsters(cptr group_text[], cptr group_char[],
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    monster_list_entry* mon_idx, int monster_count, int mon_cur, int mon_top,
    int column);
/* Shows the full recall modal for one monster entry. */
void ui_knowledge_recall_monster(int r_idx);
/* Formats the display label for one object browser entry. */
void ui_knowledge_object_entry_label(
    char* buf, size_t buf_size, const object_list_entry* obj);
/* Returns the preferred terminal attr for one object browser entry. */
byte ui_knowledge_object_entry_attr(
    const object_list_entry* obj, bool selected);
/* Returns a stable tracking id for one object browser entry. */
int ui_knowledge_object_entry_id(const object_list_entry* obj);
/* Publishes the semantic object browser UI for the current selection. */
void ui_knowledge_publish_objects(cptr group_text[], int grp_idx[], int grp_cnt,
    int grp_cur, int grp_top, object_list_entry object_idx[], int object_cnt,
    int object_cur, int object_top, int column);
/* Shows the full recall modal for one object browser entry. */
void ui_knowledge_recall_object(const object_list_entry* obj);

#endif
