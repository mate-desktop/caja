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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "caja-desktop-window.h"
#include "caja-window-private.h"
#include "caja-actions.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <eel/eel-vfs-extensions.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-icon-names.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

struct CajaDesktopWindowDetails
{
    gulong size_changed_id;

    gboolean loaded;
};

G_DEFINE_TYPE (CajaDesktopWindow, caja_desktop_window,
               CAJA_TYPE_SPATIAL_WINDOW);

static void
caja_desktop_window_init (CajaDesktopWindow *window)
{
    GtkAction *action;
    AtkObject *accessible;

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, CAJA_TYPE_DESKTOP_WINDOW,
						       CajaDesktopWindowDetails);

    gtk_window_move (GTK_WINDOW (window), 0, 0);

    /* shouldn't really be needed given our semantic type
     * of _NET_WM_TYPE_DESKTOP, but why not
     */
    gtk_window_set_resizable (GTK_WINDOW (window),
                              FALSE);

    g_object_set_data (G_OBJECT (window), "is_desktop_window",
                       GINT_TO_POINTER (1));

    gtk_widget_hide (CAJA_WINDOW (window)->details->statusbar);
    gtk_widget_hide (CAJA_WINDOW (window)->details->menubar);

    /* Don't allow close action on desktop */
    action = gtk_action_group_get_action (CAJA_WINDOW (window)->details->main_action_group,
                                          CAJA_ACTION_CLOSE);
    gtk_action_set_sensitive (action, FALSE);

    /* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
    accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
        atk_object_set_name (accessible, _("Desktop"));
	}
}

static gint
caja_desktop_window_delete_event (CajaDesktopWindow *window)
{
    /* Returning true tells GTK+ not to delete the window. */
    return TRUE;
}

void
caja_desktop_window_update_directory (CajaDesktopWindow *window)
{
    GFile *location;

    g_assert (CAJA_IS_DESKTOP_WINDOW (window));

    location = g_file_new_for_uri (EEL_DESKTOP_URI);
    caja_window_go_to (CAJA_WINDOW (window), location);
    window->details->loaded = TRUE;

    g_object_unref (location);
}

static void
caja_desktop_window_screen_size_changed (GdkScreen             *screen,
        CajaDesktopWindow *window)
{
    int width_request, height_request;

    width_request = gdk_screen_get_width (screen);
    height_request = gdk_screen_get_height (screen);

    g_object_set (window,
                  "width_request", width_request,
                  "height_request", height_request,
                  NULL);
}

CajaDesktopWindow *
caja_desktop_window_new (CajaApplication *application,
                         GdkScreen           *screen)
{
    CajaDesktopWindow *window;
    int width_request, height_request;

    width_request = gdk_screen_get_width (screen);
    height_request = gdk_screen_get_height (screen);

    window = CAJA_DESKTOP_WINDOW
             (gtk_widget_new (caja_desktop_window_get_type(),
                              "app", application,
                              "width_request", width_request,
                              "height_request", height_request,
                              "screen", screen,
                              NULL));

    /* Special sawmill setting*/
    gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Caja");

    g_signal_connect (window, "delete_event", G_CALLBACK (caja_desktop_window_delete_event), NULL);

    /* Point window at the desktop folder.
     * Note that caja_desktop_window_init is too early to do this.
     */
    caja_desktop_window_update_directory (window);

    return window;
}

static void
map (GtkWidget *widget)
{
    /* Chain up to realize our children */
    GTK_WIDGET_CLASS (caja_desktop_window_parent_class)->map (widget);
    gdk_window_lower (gtk_widget_get_window (widget));
}

static void
unrealize (GtkWidget *widget)
{
    CajaDesktopWindow *window;
	CajaDesktopWindowDetails *details;
    GdkWindow *root_window;

    window = CAJA_DESKTOP_WINDOW (widget);
	details = window->details;

    root_window = gdk_screen_get_root_window (
                      gtk_window_get_screen (GTK_WINDOW (window)));

    gdk_property_delete (root_window,
                         gdk_atom_intern ("CAJA_DESKTOP_WINDOW_ID", TRUE));

	if (details->size_changed_id != 0) {
		g_signal_handler_disconnect (gtk_window_get_screen (GTK_WINDOW (window)),
					     details->size_changed_id);
		details->size_changed_id = 0;
	}

    GTK_WIDGET_CLASS (caja_desktop_window_parent_class)->unrealize (widget);
}

static void
set_wmspec_desktop_hint (GdkWindow *window)
{
    GdkAtom atom;

    atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);

    gdk_property_change (window,
                         gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
                         gdk_x11_xatom_to_atom (XA_ATOM), 32,
                         GDK_PROP_MODE_REPLACE, (guchar *) &atom, 1);
}

static void
set_desktop_window_id (CajaDesktopWindow *window,
                       GdkWindow             *gdkwindow)
{
    /* Tuck the desktop windows xid in the root to indicate we own the desktop.
     */
    Window window_xid;
    GdkWindow *root_window;

    root_window = gdk_screen_get_root_window (
                      gtk_window_get_screen (GTK_WINDOW (window)));

    window_xid = GDK_WINDOW_XID (gdkwindow);

    gdk_property_change (root_window,
                         gdk_atom_intern ("CAJA_DESKTOP_WINDOW_ID", FALSE),
                         gdk_x11_xatom_to_atom (XA_WINDOW), 32,
                         GDK_PROP_MODE_REPLACE, (guchar *) &window_xid, 1);
}

static void
realize (GtkWidget *widget)
{
    CajaDesktopWindow *window;
	CajaDesktopWindowDetails *details;

    window = CAJA_DESKTOP_WINDOW (widget);
	details = window->details;

    /* Make sure we get keyboard events */
    gtk_widget_set_events (widget, gtk_widget_get_events (widget)
                           | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    /* Do the work of realizing. */
    GTK_WIDGET_CLASS (caja_desktop_window_parent_class)->realize (widget);

    /* This is the new way to set up the desktop window */
    set_wmspec_desktop_hint (gtk_widget_get_window (widget));

    set_desktop_window_id (window, gtk_widget_get_window (widget));

	details->size_changed_id =
		g_signal_connect (gtk_window_get_screen (GTK_WINDOW (window)), "size_changed",
				  G_CALLBACK (caja_desktop_window_screen_size_changed), window);
}

static char *
real_get_title (CajaWindow *window)
{
    return g_strdup (_("Desktop"));
}

static CajaIconInfo *
real_get_icon (CajaWindow *window,
               CajaWindowSlot *slot)
{
    return caja_icon_info_lookup_from_name (CAJA_ICON_DESKTOP, 48);
}

static void
caja_desktop_window_class_init (CajaDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	CajaWindowClass *nclass = CAJA_WINDOW_CLASS (klass);

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;

	nclass->window_type = CAJA_WINDOW_DESKTOP;
	nclass->get_title = real_get_title;
	nclass->get_icon = real_get_icon;

	g_type_class_add_private (klass, sizeof (CajaDesktopWindowDetails));
}

gboolean
caja_desktop_window_loaded (CajaDesktopWindow *window)
{
    return window->details->loaded;
}

