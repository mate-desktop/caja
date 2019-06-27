/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* caja-window-toolbars.c - implementation of caja window toolbar operations,
 * split into separate file just for convenience.
 */

#include <config.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <eel/eel-mate-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>

#include <libcaja-extension/caja-menu-provider.h>
#include <libcaja-private/caja-bookmark.h>
#include <libcaja-private/caja-extensions.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-ui-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-module.h>

#include "caja-application.h"
#include "caja-window-manage-views.h"
#include "caja-window-private.h"
#include "caja-window.h"

/* FIXME bugzilla.gnome.org 41243:
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "caja-desktop-window.h"

#define TOOLBAR_PATH_EXTENSION_ACTIONS "/Toolbar/Extra Buttons Placeholder/Extension Actions"

void
caja_navigation_window_set_spinner_active (CajaNavigationWindow *window,
        gboolean allow)
{
    if (( window->details->spinner_active &&  allow) ||
            (!window->details->spinner_active && !allow)) {
        return;
    }

    window->details->spinner_active = allow;
    if (allow) {
        gtk_widget_show (window->details->spinner);
        gtk_spinner_start (GTK_SPINNER (window->details->spinner));
    } else {
        gtk_widget_hide (window->details->spinner);
    }
}

void
caja_navigation_window_activate_spinner (CajaNavigationWindow *window)
{
    GtkToolItem *item;
    GtkWidget *spinner;

    if (window->details->spinner != NULL)
    {
        return;
    }

    item = gtk_tool_item_new ();
    gtk_widget_show (GTK_WIDGET (item));
    gtk_tool_item_set_expand (item, TRUE);
    gtk_toolbar_insert (GTK_TOOLBAR (window->details->toolbar),
                        item, -1);

    spinner = gtk_spinner_new ();
    gtk_widget_show (GTK_WIDGET (spinner));

    item = gtk_tool_item_new ();
    gtk_container_add (GTK_CONTAINER (item), spinner);
    gtk_widget_show (GTK_WIDGET (item));

    gtk_toolbar_insert (GTK_TOOLBAR (window->details->toolbar),
                        item, -1);

    window->details->spinner = spinner;
}

void
caja_navigation_window_initialize_toolbars (CajaNavigationWindow *window)
{
    caja_navigation_window_activate_spinner (window);
}


static GList *
get_extension_toolbar_items (CajaNavigationWindow *window)
{
    CajaWindowSlot *slot;
    GList *items;
    GList *providers;
    GList *l;

    providers = caja_extensions_get_for_type (CAJA_TYPE_MENU_PROVIDER);
    items = NULL;

    slot = CAJA_WINDOW (window)->details->active_pane->active_slot;

    for (l = providers; l != NULL; l = l->next)
    {
        CajaMenuProvider *provider;
        GList *file_items;

        provider = CAJA_MENU_PROVIDER (l->data);
        file_items = caja_menu_provider_get_toolbar_items
                     (provider,
                      GTK_WIDGET (window),
                      slot->viewed_file);
        items = g_list_concat (items, file_items);
    }

    caja_module_extension_list_free (providers);

    return items;
}

void
caja_navigation_window_load_extension_toolbar_items (CajaNavigationWindow *window)
{
    GtkUIManager *ui_manager;
    GList *items;
    GList *l;
    guint merge_id;
    GtkActionGroup *action_group;
    GtkAction *action = NULL;
    CajaMenuItem *item = NULL;
    const gchar *action_name = NULL;

    ui_manager = caja_window_get_ui_manager (CAJA_WINDOW (window));
    if (window->details->extensions_toolbar_merge_id != 0)
    {
        gtk_ui_manager_remove_ui (ui_manager,
                                  window->details->extensions_toolbar_merge_id);
        window->details->extensions_toolbar_merge_id = 0;
    }

    if (window->details->extensions_toolbar_action_group != NULL)
    {
        gtk_ui_manager_remove_action_group (ui_manager,
                                            window->details->extensions_toolbar_action_group);
        window->details->extensions_toolbar_action_group = NULL;
    }

    merge_id = gtk_ui_manager_new_merge_id (ui_manager);
    window->details->extensions_toolbar_merge_id = merge_id;
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action_group = gtk_action_group_new ("ExtensionsToolbarGroup");
    window->details->extensions_toolbar_action_group = action_group;
    gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    gtk_ui_manager_insert_action_group (ui_manager, action_group, -1);
    g_object_unref (action_group); /* owned by ui manager */

    items = get_extension_toolbar_items (window);

    for (l = items; l != NULL; l = l->next)
    {
        item = CAJA_MENU_ITEM (l->data);

        action = caja_toolbar_action_from_menu_item (item, GTK_WIDGET (window));

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gtk_action_group_add_action (action_group,
                                     GTK_ACTION (action));
        g_object_unref (action);

        action_name = gtk_action_get_name (action);
        G_GNUC_END_IGNORE_DEPRECATIONS;

        gtk_ui_manager_add_ui (ui_manager,
                               merge_id,
                               TOOLBAR_PATH_EXTENSION_ACTIONS,
                               action_name,
                               action_name,
                               GTK_UI_MANAGER_TOOLITEM,
                               FALSE);

        g_object_unref (item);
    }

    g_list_free (items);
}
