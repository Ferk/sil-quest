/* File: ui-term.h
 *
 * Copyright (c) 2026 Fernando Carmona Varo
 * This file is part of Sil-Quest.
 * Licensed under the EUPL, Version 1.2 or subsequent versions of the EUPL
 * You may not use this work except in compliance with the Licence.
 * You may obtain copy of it at: https://joinup.ec.europa.eu/software/page/eupl
 */

/*
 * Terminal-frontend hook registration for rendering frontend-neutral UI state
 * on the classic terminal presentation.
 */

#ifndef INCLUDED_UI_TERM_H
#define INCLUDED_UI_TERM_H

#include "angband.h"

/* Registers the terminal frontend's prompt and semantic-menu render hooks. */
void ui_term_init(void);

#endif /* INCLUDED_UI_TERM_H */
