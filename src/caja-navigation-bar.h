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
 */

/* caja-navigation-bar.h - Abstract navigation bar class
 */

#ifndef CAJA_NAVIGATION_BAR_H
#define CAJA_NAVIGATION_BAR_H

#include <gtk/gtk.h>

#define CAJA_TYPE_NAVIGATION_BAR caja_navigation_bar_get_type()
#define CAJA_NAVIGATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_NAVIGATION_BAR, CajaNavigationBar))
#define CAJA_NAVIGATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_NAVIGATION_BAR, CajaNavigationBarClass))
#define CAJA_IS_NAVIGATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_NAVIGATION_BAR))
#define CAJA_IS_NAVIGATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_NAVIGATION_BAR))
#define CAJA_NAVIGATION_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_NAVIGATION_BAR, CajaNavigationBarClass))

typedef struct
{
    GtkHBox parent;
} CajaNavigationBar;

typedef struct
{
    GtkHBoxClass parent_class;

    /* signals */
    void         (* location_changed) (CajaNavigationBar *bar,
                                       const char            *location);
    void         (* cancel)           (CajaNavigationBar *bar);

    /* virtual methods */
    void	     (* activate)	  (CajaNavigationBar *bar);
    char *       (* get_location)     (CajaNavigationBar *bar);
    void         (* set_location)     (CajaNavigationBar *bar,
                                       const char            *location);

} CajaNavigationBarClass;

GType   caja_navigation_bar_get_type         (void);
void	caja_navigation_bar_activate	 (CajaNavigationBar *bar);
char *  caja_navigation_bar_get_location     (CajaNavigationBar *bar);
void    caja_navigation_bar_set_location     (CajaNavigationBar *bar,
        const char            *location);

/* `protected' function meant to be used by subclasses to emit the `location_changed' signal */
void    caja_navigation_bar_location_changed (CajaNavigationBar *bar);

#endif /* CAJA_NAVIGATION_BAR_H */
