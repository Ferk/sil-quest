/* File: ui-item-select.c
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL 
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Small helper for frontends that want to choose an item first and let the
 * normal command flow consume that choice on the next get_item() call.
 */

#include "angband.h"

static bool ui_preselected_item_valid = FALSE;
static int ui_preselected_item = 0;

/* Clears any pending frontend-supplied item selection. */
static void ui_preselected_item_reset(void)
{
    ui_preselected_item_valid = FALSE;
}

/* Stashes one item index for the next get_item() call to consume. */
void ui_preselect_item(int item)
{
    ui_preselected_item = item;
    ui_preselected_item_valid = TRUE;
}

/* Returns and consumes one pending preselected item, if any. */
bool ui_pull_preselected_item(int* item)
{
    if (!ui_preselected_item_valid || !item)
        return FALSE;

    *item = ui_preselected_item;
    ui_preselected_item_reset();
    return TRUE;
}

/* Discards any pending preselected item without consuming it. */
void ui_clear_preselected_item(void)
{
    ui_preselected_item_reset();
}
