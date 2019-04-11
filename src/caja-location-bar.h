/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

/* caja-location-bar.h - Location bar for Caja
 */

#ifndef CAJA_LOCATION_BAR_H
#define CAJA_LOCATION_BAR_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-entry.h>

#include "caja-navigation-window.h"
#include "caja-navigation-window-pane.h"

#define CAJA_TYPE_LOCATION_BAR caja_location_bar_get_type()
#define CAJA_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_LOCATION_BAR, CajaLocationBar))
#define CAJA_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_LOCATION_BAR, CajaLocationBarClass))
#define CAJA_IS_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_LOCATION_BAR))
#define CAJA_IS_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_LOCATION_BAR))
#define CAJA_LOCATION_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_LOCATION_BAR, CajaLocationBarClass))

typedef struct _CajaLocationBarPrivate CajaLocationBarPrivate;

typedef struct CajaLocationBar
{
    GtkHBox parent;
    CajaLocationBarPrivate *details;
} CajaLocationBar;

typedef struct
{
    GtkHBoxClass parent_class;

    /* for GtkBindingSet */
    void         (* cancel)           (CajaLocationBar *bar);
} CajaLocationBarClass;

GType      caja_location_bar_get_type     	(void);
GtkWidget* caja_location_bar_new          	(CajaNavigationWindowPane *pane);
void       caja_location_bar_set_active     (CajaLocationBar *location_bar,
        gboolean is_active);
CajaEntry * caja_location_bar_get_entry (CajaLocationBar *location_bar);

void    caja_location_bar_activate         (CajaLocationBar *bar);
void    caja_location_bar_set_location     (CajaLocationBar *bar,
                                            const char      *location);

#endif /* CAJA_LOCATION_BAR_H */
