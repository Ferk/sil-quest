/* File: ui-preview.h
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

#ifndef INCLUDED_UI_PREVIEW_H
#define INCLUDED_UI_PREVIEW_H

#include "angband.h"

/* Adds preview-only defaults so object descriptions remain readable. */
void ui_preview_apply_object_defaults(object_type* o_ptr);
/* Builds one fake known object-kind preview for details or recall screens. */
bool ui_preview_prepare_kind_object(
    object_type* o_ptr, int k_idx, bool fully_known);
/* Builds one fake artefact preview object for details or recall screens. */
bool ui_preview_prepare_artefact_object(object_type* o_ptr, int a_idx);
/* Builds one smithing base object preview from a tval/sval choice. */
bool ui_preview_prepare_smithing_base_object(
    object_type* o_ptr, int tval, int sval);
/* Opens the full recall screen for one fake object kind preview. */
void ui_preview_show_kind_recall(int k_idx);
/* Opens the full recall screen for one fake artefact preview. */
void ui_preview_show_artefact_recall(int a_idx);

#endif
