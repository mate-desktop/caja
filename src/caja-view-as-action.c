/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 2009 Red Hat, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>

#include <gtk/gtk.h>

#include <eel/eel-gtk-extensions.h>

#include <libcaja-private/caja-view-factory.h>

#include "caja-view-as-action.h"
#include "caja-navigation-window.h"
#include "caja-window-private.h"
#include "caja-navigation-window-slot.h"

static GObjectClass *parent_class = NULL;

struct _CajaViewAsActionPrivate
{
    CajaNavigationWindow *window;
};

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_PRIVATE (CajaViewAsAction, caja_view_as_action, GTK_TYPE_ACTION)
G_GNUC_END_IGNORE_DEPRECATIONS;

enum
{
    PROP_0,
    PROP_WINDOW
};


static void
activate_nth_short_list_item (CajaWindow *window, guint index)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = caja_window_get_active_slot (window);
    g_assert (index < g_list_length (window->details->short_list_viewers));

    caja_window_slot_set_content_view (slot,
                                       g_list_nth_data (window->details->short_list_viewers, index));
}

static void
activate_extra_viewer (CajaWindow *window)
{
    CajaWindowSlot *slot;

    g_assert (CAJA_IS_WINDOW (window));

    slot = caja_window_get_active_slot (window);
    g_assert (window->details->extra_viewer != NULL);

    caja_window_slot_set_content_view (slot, window->details->extra_viewer);
}

static void
view_as_menu_switch_views_callback (GtkComboBox *combo_box, CajaNavigationWindow *window)
{
    int active;

    g_assert (GTK_IS_COMBO_BOX (combo_box));
    g_assert (CAJA_IS_NAVIGATION_WINDOW (window));

    active = gtk_combo_box_get_active (combo_box);

    if (active < 0)
    {
        return;
    }
    else if (active < GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo_box), "num viewers")))
    {
        activate_nth_short_list_item (CAJA_WINDOW (window), active);
    }
    else
    {
        activate_extra_viewer (CAJA_WINDOW (window));
    }
}

static void
view_as_changed_callback (CajaWindow *window,
                          GtkComboBox *combo_box)
{
    CajaWindowSlot *slot;
    GList *node;
    int index;
    int selected_index = -1;
    GtkTreeModel *model;
    GtkListStore *store;
    const CajaViewInfo *info;

    /* Clear the contents of ComboBox in a wacky way because there
     * is no function to clear all items and also no function to obtain
     * the number of items in a combobox.
     */
    model = gtk_combo_box_get_model (combo_box);
    g_return_if_fail (GTK_IS_LIST_STORE (model));
    store = GTK_LIST_STORE (model);
    gtk_list_store_clear (store);

    slot = caja_window_get_active_slot (window);

    /* Add a menu item for each view in the preferred list for this location. */
    for (node = window->details->short_list_viewers, index = 0;
            node != NULL;
            node = node->next, ++index)
    {
        info = caja_view_factory_lookup (node->data);
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), _(info->view_combo_label));

        if (caja_window_slot_content_view_matches_iid (slot, (char *)node->data))
        {
            selected_index = index;
        }
    }
    g_object_set_data (G_OBJECT (combo_box), "num viewers", GINT_TO_POINTER (index));

    if (g_list_length (window->details->short_list_viewers) == 1)
    {
        selected_index = 0;
    }

    if (selected_index == -1)
    {
        const char *id;
        /* We're using an extra viewer, add a menu item for it */

        id = caja_window_slot_get_content_view_id (slot);
        info = caja_view_factory_lookup (id);
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box),
                                        _(info->view_combo_label));
        selected_index = index;
    }
    gtk_combo_box_set_active (combo_box, selected_index);
    if (g_list_length (window->details->short_list_viewers) == 1) {
        gtk_widget_hide(GTK_WIDGET(combo_box));
    } else {
        gtk_widget_show(GTK_WIDGET(combo_box));
    }
}


static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
    if (GTK_IS_TOOL_ITEM (proxy))
    {
        GtkToolItem *item = GTK_TOOL_ITEM (proxy);
        CajaViewAsAction *vaction = CAJA_VIEW_AS_ACTION (action);
        CajaNavigationWindow *window = vaction->priv->window;
        GtkWidget *view_as_menu_vbox;
        GtkWidget *view_as_combo_box;

        /* Option menu for content view types; it's empty here, filled in when a uri is set.
         * Pack it into vbox so it doesn't grow vertically when location bar does.
         */
        view_as_menu_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_show (view_as_menu_vbox);

        gtk_container_add (GTK_CONTAINER (item), view_as_menu_vbox);

        view_as_combo_box = gtk_combo_box_text_new ();

        gtk_widget_set_focus_on_click (view_as_combo_box, FALSE);
        gtk_box_pack_end (GTK_BOX (view_as_menu_vbox), view_as_combo_box, TRUE, FALSE, 0);
        gtk_widget_show (view_as_combo_box);
        g_signal_connect_object (view_as_combo_box, "changed",
                                 G_CALLBACK (view_as_menu_switch_views_callback), window, 0);

        g_signal_connect (window, "view-as-changed",
                          G_CALLBACK (view_as_changed_callback),
                          view_as_combo_box);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    (* GTK_ACTION_CLASS (parent_class)->connect_proxy) (action, proxy);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
disconnect_proxy (GtkAction *action,
                  GtkWidget *proxy)
{
    if (GTK_IS_TOOL_ITEM (proxy))
    {
        CajaViewAsAction *vaction = CAJA_VIEW_AS_ACTION (action);
        CajaNavigationWindow *window = vaction->priv->window;

        g_signal_handlers_disconnect_matched (window,
                                              G_SIGNAL_MATCH_FUNC,
                                              0, 0, NULL, G_CALLBACK (view_as_changed_callback), NULL);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    (* GTK_ACTION_CLASS (parent_class)->disconnect_proxy) (action, proxy);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
caja_view_as_action_finalize (GObject *object)
{
    (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
caja_view_as_action_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
    CajaViewAsAction *zoom;

    zoom = CAJA_VIEW_AS_ACTION (object);

    switch (prop_id)
    {
    case PROP_WINDOW:
        zoom->priv->window = CAJA_NAVIGATION_WINDOW (g_value_get_object (value));
        break;
    }
}

static void
caja_view_as_action_get_property (GObject *object,
                                  guint prop_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
    CajaViewAsAction *zoom;

    zoom = CAJA_VIEW_AS_ACTION (object);

    switch (prop_id)
    {
    case PROP_WINDOW:
        g_value_set_object (value, zoom->priv->window);
        break;
    }
}

static void
caja_view_as_action_class_init (CajaViewAsActionClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    GtkActionClass *action_class = GTK_ACTION_CLASS (class);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    object_class->finalize = caja_view_as_action_finalize;
    object_class->set_property = caja_view_as_action_set_property;
    object_class->get_property = caja_view_as_action_get_property;

    parent_class = g_type_class_peek_parent (class);

    action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
    action_class->connect_proxy = connect_proxy;
    action_class->disconnect_proxy = disconnect_proxy;

    g_object_class_install_property (object_class,
                                     PROP_WINDOW,
                                     g_param_spec_object ("window",
                                             "Window",
                                             "The navigation window",
                                             G_TYPE_OBJECT,
                                             G_PARAM_READWRITE));
}

static void
caja_view_as_action_init (CajaViewAsAction *action)
{
    action->priv = caja_view_as_action_get_instance_private (action);
}
