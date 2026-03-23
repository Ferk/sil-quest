/* File: ui-inventory.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Frontend-neutral unified inventory browser helpers.
 */

#ifndef INCLUDED_UI_INVENTORY_H
#define INCLUDED_UI_INVENTORY_H

#include "angband.h"

enum
{
    UI_INVENTORY_SECTION_EQUIP = 0,
    UI_INVENTORY_SECTION_INVEN = 1
};

bool do_cmd_ui_inventory_menu(
    int initial_section, bool (*can_wear)(const object_type*));

#endif /* INCLUDED_UI_INVENTORY_H */
