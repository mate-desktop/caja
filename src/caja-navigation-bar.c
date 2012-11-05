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

/* caja-navigation-bar.c - Abstract navigation bar class
 */

#include <config.h>
#include "caja-navigation-bar.h"

#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

enum
{
    ACTIVATE,
    CANCEL,
    LOCATION_CHANGED,
    LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void caja_navigation_bar_class_init (CajaNavigationBarClass *class);
static void caja_navigation_bar_init       (CajaNavigationBar      *bar);

EEL_CLASS_BOILERPLATE (CajaNavigationBar, caja_navigation_bar, GTK_TYPE_HBOX)

EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (caja_navigation_bar, get_location)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (caja_navigation_bar, set_location)

static void
caja_navigation_bar_class_init (CajaNavigationBarClass *klass)
{
    GtkObjectClass *object_class;
    GtkBindingSet *binding_set;

    object_class = GTK_OBJECT_CLASS (klass);

    signals[ACTIVATE] = g_signal_new
                        ("activate",
                         G_TYPE_FROM_CLASS (object_class),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET (CajaNavigationBarClass,
                                          activate),
                         NULL, NULL,
                         g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0);

    signals[CANCEL] = g_signal_new
                      ("cancel",
                       G_TYPE_FROM_CLASS (object_class),
                       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET (CajaNavigationBarClass,
                                        cancel),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    signals[LOCATION_CHANGED] = g_signal_new
                                ("location_changed",
                                 G_TYPE_FROM_CLASS (object_class),
                                 G_SIGNAL_RUN_LAST,
                                 G_STRUCT_OFFSET (CajaNavigationBarClass,
                                         location_changed),
                                 NULL, NULL,
                                 g_cclosure_marshal_VOID__STRING,
                                 G_TYPE_NONE, 1, G_TYPE_STRING);

    klass->activate = NULL;
    klass->cancel = NULL;

    binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

    EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, caja_navigation_bar, get_location);
    EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, caja_navigation_bar, set_location);
}

static void
caja_navigation_bar_init (CajaNavigationBar *bar)
{
}

/**
 * caja_navigation_bar_activate
 *
 * Change the navigation bar to an active state.
 *
 * @bar: A CajaNavigationBar.
 */
void
caja_navigation_bar_activate (CajaNavigationBar *bar)
{
    g_return_if_fail (CAJA_IS_NAVIGATION_BAR (bar));

    g_signal_emit (bar, signals[ACTIVATE], 0);
}

/**
 * caja_navigation_bar_get_location
 *
 * Return the location displayed in the navigation bar.
 *
 * @bar: A CajaNavigationBar.
 * @location: The uri that should be displayed.
 */
char *
caja_navigation_bar_get_location (CajaNavigationBar *bar)
{
    g_return_val_if_fail (CAJA_IS_NAVIGATION_BAR (bar), NULL);

    return EEL_CALL_METHOD_WITH_RETURN_VALUE
           (CAJA_NAVIGATION_BAR_CLASS, bar,
            get_location, (bar));
}

/**
 * caja_navigation_bar_set_location
 *
 * Change the location displayed in the navigation bar.
 *
 * @bar: A CajaNavigationBar.
 * @location: The uri that should be displayed.
 */
void
caja_navigation_bar_set_location (CajaNavigationBar *bar,
                                  const char *location)
{
    g_return_if_fail (CAJA_IS_NAVIGATION_BAR (bar));

    EEL_CALL_METHOD (CAJA_NAVIGATION_BAR_CLASS, bar,
                     set_location, (bar, location));
}

void
caja_navigation_bar_location_changed (CajaNavigationBar *bar)
{
    char *location;

    g_return_if_fail (CAJA_IS_NAVIGATION_BAR (bar));

    location = caja_navigation_bar_get_location (bar);
    g_signal_emit (bar,
                   signals[LOCATION_CHANGED], 0,
                   location);
    g_free (location);
}
