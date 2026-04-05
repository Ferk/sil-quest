/* File: ui-preview.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Helpers for building and displaying fake item previews used by UI screens.
 */

#include "angband.h"

#include "ui-preview.h"
#include "ui-model.h"

#define UI_PREVIEW_MODAL_TEXT_MAX 8192

static bool ui_preview_object_recall_modal_enabled = FALSE;

/* Opens the standard full recall screen for one prepared preview object. */
static void ui_preview_show_object_recall(const object_type* o_ptr)
{
    if (!o_ptr)
        return;

    handle_stuff();
    Term_gotoxy(0, 0);
    object_info_screen(o_ptr);
}

/* Enables or disables semantic object-recall modals for one frontend. */
void ui_preview_set_object_recall_modal_enabled(bool enabled)
{
    ui_preview_object_recall_modal_enabled = enabled;
}

/* Shows one semantic object-recall modal when the active frontend supports it. */
bool ui_preview_show_object_recall_modal(const object_type* o_ptr)
{
    char modal_text[UI_PREVIEW_MODAL_TEXT_MAX];
    byte modal_attrs[UI_PREVIEW_MODAL_TEXT_MAX];
    ui_text_builder modal_builder;

    if (!ui_preview_object_recall_modal_enabled || !o_ptr)
        return FALSE;

    ui_text_builder_init(
        &modal_builder, modal_text, modal_attrs, sizeof(modal_text));
    object_info_append_ui_text(&modal_builder, o_ptr, TRUE);
    if ((modal_builder.off > 0)
        && (modal_builder.text[modal_builder.off - 1] != '\n'))
    {
        ui_text_builder_newline(&modal_builder, TERM_WHITE);
    }
    ui_text_builder_newline(&modal_builder, TERM_WHITE);
    ui_text_builder_append_line(&modal_builder, "(press any key)", TERM_L_BLUE);

    ui_modal_set_kind(modal_text, modal_attrs,
        ui_text_builder_length(&modal_builder), '\r', UI_MODAL_KIND_GENERIC);
    ui_modal_set_visual(graphics_are_ascii() ? UI_MENU_VISUAL_TEXT
                                             : UI_MENU_VISUAL_TILE,
        (byte)object_attr((object_type*)o_ptr),
        (byte)object_char((object_type*)o_ptr));

    (void)inkey();
    ui_modal_clear();

    return TRUE;
}

/* Adds preview-only defaults so object descriptions remain readable. */
void ui_preview_apply_object_defaults(object_type* o_ptr)
{
    if (!o_ptr)
        return;

    switch (o_ptr->tval)
    {
    case TV_DIGGING:
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;

    case TV_RING:
        switch (o_ptr->sval)
        {
        case SV_RING_STR:
        case SV_RING_DEX:
        case SV_RING_SECRETS:
        case SV_RING_ERED_LUIN:
        case SV_RING_LAIQUENDI:
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;

        case SV_RING_ACCURACY:
            if (o_ptr->att < 1)
                o_ptr->att = 1;
            break;

        case SV_RING_PROTECTION:
            o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;

        case SV_RING_EVASION:
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;
            break;

        default:
            break;
        }
        break;

    case TV_AMULET:
        switch (o_ptr->sval)
        {
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        case SV_AMULET_BLESSED_REALM:
        case SV_AMULET_VIGILANT_EYE:
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;

        default:
            break;
        }
        break;

    case TV_LIGHT:
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
            o_ptr->timeout = 0;
            break;

        default:
            break;
        }
        break;

    case TV_STAFF:
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;

    default:
        break;
    }
}

/* Builds one fake known object-kind preview for details or recall screens. */
bool ui_preview_prepare_kind_object(object_type* o_ptr, int k_idx, bool fully_known)
{
    if (!o_ptr || (k_idx <= 0) || (k_idx >= z_info->k_max))
        return FALSE;

    object_wipe(o_ptr);
    object_prep(o_ptr, k_idx);
    ui_preview_apply_object_defaults(o_ptr);
    object_aware(o_ptr);

    if (fully_known)
        object_known(o_ptr);

    return TRUE;
}

/* Builds one fake artefact preview object for details or recall screens. */
bool ui_preview_prepare_artefact_object(object_type* o_ptr, int a_idx)
{
    s16b kind_idx;
    s16b i;
    artefact_type* a_ptr;

    if (!o_ptr || (a_idx <= 0) || (a_idx >= z_info->art_max))
        return FALSE;

    a_ptr = &a_info[a_idx];
    if (a_ptr->tval + a_ptr->sval == 0)
        return FALSE;

    kind_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (!kind_idx)
        return FALSE;

    object_wipe(o_ptr);
    object_prep(o_ptr, kind_idx);
    o_ptr->name1 = a_idx;
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    object_known(o_ptr);
    o_ptr->ident |= IDENT_SPOIL;

    if (a_ptr->flags3 & TR3_LIGHT_CURSE)
        o_ptr->ident |= IDENT_CURSED;

    return TRUE;
}

/* Builds one smithing base object preview from a tval/sval choice. */
bool ui_preview_prepare_smithing_base_object(object_type* o_ptr, int tval, int sval)
{
    int kind_idx;

    if (!o_ptr)
        return FALSE;

    kind_idx = lookup_kind(tval, sval);
    if (!kind_idx)
        return FALSE;

    object_wipe(o_ptr);
    object_prep(o_ptr, kind_idx);
    ui_preview_apply_object_defaults(o_ptr);
    o_ptr->weight = k_info[o_ptr->k_idx].weight;
    o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);

    if (tval == TV_ARROW)
        o_ptr->number = 12;

    return TRUE;
}

/* Opens the full recall screen for one fake object kind preview. */
void ui_preview_show_kind_recall(int k_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    if (!ui_preview_prepare_kind_object(i_ptr, k_idx, TRUE))
        return;

    ui_preview_show_object_recall(i_ptr);
}

/* Opens the full recall screen for one fake artefact preview. */
void ui_preview_show_artefact_recall(int a_idx)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    if (!ui_preview_prepare_artefact_object(i_ptr, a_idx))
        return;

    ui_preview_show_object_recall(i_ptr);
}
