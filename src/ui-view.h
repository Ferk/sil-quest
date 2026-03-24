/* File: ui-view.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Commands for the nearby monster and object overlays.
 */

#ifndef INCLUDED_UI_VIEW_H
#define INCLUDED_UI_VIEW_H

#include "angband.h"

/* Shows the nearby-monster overlay and toggle prompt. */
void do_cmd_view_monsters(void);
/* Shows the nearby-object overlay and toggle prompt. */
void do_cmd_view_objects(void);

#endif
