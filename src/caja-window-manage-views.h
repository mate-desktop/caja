/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef CAJA_WINDOW_MANAGE_VIEWS_H
#define CAJA_WINDOW_MANAGE_VIEWS_H

#include "caja-window.h"
#include "caja-window-pane.h"
#include "caja-navigation-window.h"

void caja_window_manage_views_close_slot (CajaWindowPane *pane,
        CajaWindowSlot *slot);


/* CajaWindowInfo implementation: */
void caja_window_report_load_underway     (CajaWindow     *window,
        CajaView       *view);
void caja_window_report_selection_changed (CajaWindowInfo *window);
void caja_window_report_view_failed       (CajaWindow     *window,
        CajaView       *view);
void caja_window_report_load_complete     (CajaWindow     *window,
        CajaView       *view);
void caja_window_report_location_change   (CajaWindow     *window);
void caja_window_update_up_button         (CajaWindow     *window);

#endif /* CAJA_WINDOW_MANAGE_VIEWS_H */
