/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
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
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *
 */

#include <config.h>

#include <gtk/gtk.h>

#include <eel/eel-gtk-extensions.h>

#include "caja-navigation-action.h"
#include "caja-navigation-window.h"
#include "caja-window-private.h"
#include "caja-navigation-window-slot.h"

struct _CajaNavigationActionPrivate
{
    CajaNavigationWindow *window;
    CajaNavigationDirection direction;
    char *arrow_tooltip;
};

enum
{
    PROP_0,
    PROP_ARROW_TOOLTIP,
    PROP_DIRECTION,
    PROP_WINDOW
};

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
G_DEFINE_TYPE_WITH_PRIVATE (CajaNavigationAction, caja_navigation_action, GTK_TYPE_ACTION)
G_GNUC_END_IGNORE_DEPRECATIONS;

static gboolean
should_open_in_new_tab (void)
{
    /* FIXME this is duplicated */
    GdkEvent *event;

    event = gtk_get_current_event ();
    if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
    {
        return event->button.button == 2;
    }

    gdk_event_free (event);

    return FALSE;
}

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item,
                                    CajaNavigationWindow *window,
                                    gboolean back)
{
    int index;

    g_assert (GTK_IS_MENU_ITEM (menu_item));
    g_assert (CAJA_IS_NAVIGATION_WINDOW (window));

    index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

    caja_navigation_window_back_or_forward (window, back, index, should_open_in_new_tab ());
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item, CajaNavigationWindow *window)
{
    activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, CajaNavigationWindow *window)
{
    activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (CajaNavigationWindow *window,
           GtkWidget *menu,
           gboolean back)
{
    CajaNavigationWindowSlot *slot;
    int index;
    GList *list;
    gboolean list_void;
    GtkWidget *menu_item = NULL;

    g_assert (CAJA_IS_NAVIGATION_WINDOW (window));

    slot = CAJA_NAVIGATION_WINDOW_SLOT (CAJA_WINDOW (window)->details->active_pane->active_slot);

    list = back ? slot->back_list : slot->forward_list;
    index = 0;
    list_void = TRUE;

    while (list != NULL)
    {
        menu_item = caja_bookmark_menu_item_new (CAJA_BOOKMARK (list->data));

        if (menu_item) {
            list_void = FALSE;
            g_object_set_data (G_OBJECT (menu_item), "user_data", GINT_TO_POINTER (index));
            gtk_widget_show (GTK_WIDGET (menu_item));
            g_signal_connect_object (menu_item, "activate",
                                     back
                                     ? G_CALLBACK (activate_back_menu_item_callback)
                                     : G_CALLBACK (activate_forward_menu_item_callback),
                                     window, 0);

            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
        }

        list = g_list_next (list);
        ++index;
    }

    if (list_void)
    {
        gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                               eel_image_menu_item_new_from_icon ("dialog-error", _("folder removed")));
        if (back)
        {
            caja_navigation_window_slot_clear_back_list (slot);
            caja_navigation_window_allow_back (window, FALSE);
        }
        else
        {
            caja_navigation_window_slot_clear_forward_list (slot);
            caja_navigation_window_allow_forward (window, FALSE);
        }
    }
}

static void
show_menu_callback (GtkMenuToolButton *button,
                    CajaNavigationAction *action)
{
    CajaNavigationActionPrivate *p;
    CajaNavigationWindow *window;
    GtkWidget *menu;
    GList *children;
    GList *li;

    p = action->priv;
    window = action->priv->window;

    menu = gtk_menu_tool_button_get_menu (button);

    children = gtk_container_get_children (GTK_CONTAINER (menu));
    for (li = children; li; li = li->next)
    {
        gtk_container_remove (GTK_CONTAINER (menu), li->data);
    }
    g_list_free (children);

    switch (p->direction)
    {
    case CAJA_NAVIGATION_DIRECTION_FORWARD:
        fill_menu (window, menu, FALSE);
        break;
    case CAJA_NAVIGATION_DIRECTION_BACK:
        fill_menu (window, menu, TRUE);
        break;
    default:
        g_assert_not_reached ();
        break;
    }
}

static gboolean
proxy_button_press_event_cb (GtkButton *button,
                             GdkEventButton *event,
                             gpointer user_data)
{
    if (event->button == 2)
    {
        g_signal_emit_by_name (button, "pressed", 0);
    }

    return FALSE;
}

static gboolean
proxy_button_release_event_cb (GtkButton *button,
                               GdkEventButton *event,
                               gpointer user_data)
{
    if (event->button == 2)
    {
        g_signal_emit_by_name (button, "released", 0);
    }

    return FALSE;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
    if (GTK_IS_MENU_TOOL_BUTTON (proxy))
    {
        CajaNavigationAction *naction = CAJA_NAVIGATION_ACTION (action);
        GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (proxy);
        GtkWidget *menu;
        GtkWidget *child;

        /* set an empty menu, so the arrow button becomes sensitive */
        menu = gtk_menu_new ();

        gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);

        gtk_menu_tool_button_set_menu (button, menu);

        gtk_menu_tool_button_set_arrow_tooltip_text (button,
                naction->priv->arrow_tooltip);

        g_signal_connect (proxy, "show-menu",
                          G_CALLBACK (show_menu_callback), action);

        /* Make sure that middle click works. Note that there is some code duplication
         * between here and caja-window-menus.c */
        child = eel_gtk_menu_tool_button_get_button (button);
        g_signal_connect (child, "button-press-event", G_CALLBACK (proxy_button_press_event_cb), NULL);
        g_signal_connect (child, "button-release-event", G_CALLBACK (proxy_button_release_event_cb), NULL);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    (* GTK_ACTION_CLASS (caja_navigation_action_parent_class)->connect_proxy) (action, proxy);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
disconnect_proxy (GtkAction *action, GtkWidget *proxy)
{
    if (GTK_IS_MENU_TOOL_BUTTON (proxy))
    {
        GtkWidget *child;

        g_signal_handlers_disconnect_by_func (proxy, G_CALLBACK (show_menu_callback), action);

        child = eel_gtk_menu_tool_button_get_button (GTK_MENU_TOOL_BUTTON (proxy));
        g_signal_handlers_disconnect_by_func (child, G_CALLBACK (proxy_button_press_event_cb), NULL);
        g_signal_handlers_disconnect_by_func (child, G_CALLBACK (proxy_button_release_event_cb), NULL);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    (* GTK_ACTION_CLASS (caja_navigation_action_parent_class)->disconnect_proxy) (action, proxy);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static void
caja_navigation_action_finalize (GObject *object)
{
    CajaNavigationAction *action = CAJA_NAVIGATION_ACTION (object);

    g_free (action->priv->arrow_tooltip);

    (* G_OBJECT_CLASS (caja_navigation_action_parent_class)->finalize) (object);
}

static void
caja_navigation_action_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
    CajaNavigationAction *nav;

    nav = CAJA_NAVIGATION_ACTION (object);

    switch (prop_id)
    {
    case PROP_ARROW_TOOLTIP:
        g_free (nav->priv->arrow_tooltip);
        nav->priv->arrow_tooltip = g_value_dup_string (value);
        break;
    case PROP_DIRECTION:
        nav->priv->direction = g_value_get_int (value);
        break;
    case PROP_WINDOW:
        nav->priv->window = CAJA_NAVIGATION_WINDOW (g_value_get_object (value));
        break;
    }
}

static void
caja_navigation_action_get_property (GObject *object,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
    CajaNavigationAction *nav;

    nav = CAJA_NAVIGATION_ACTION (object);

    switch (prop_id)
    {
    case PROP_ARROW_TOOLTIP:
        g_value_set_string (value, nav->priv->arrow_tooltip);
        break;
    case PROP_DIRECTION:
        g_value_set_int (value, nav->priv->direction);
        break;
    case PROP_WINDOW:
        g_value_set_object (value, nav->priv->window);
        break;
    }
}

static void
caja_navigation_action_class_init (CajaNavigationActionClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    GtkActionClass *action_class = GTK_ACTION_CLASS (class);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    object_class->finalize = caja_navigation_action_finalize;
    object_class->set_property = caja_navigation_action_set_property;
    object_class->get_property = caja_navigation_action_get_property;

    action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
    action_class->connect_proxy = connect_proxy;
    action_class->disconnect_proxy = disconnect_proxy;

    g_object_class_install_property (object_class,
                                     PROP_ARROW_TOOLTIP,
                                     g_param_spec_string ("arrow-tooltip",
                                             "Arrow Tooltip",
                                             "Arrow Tooltip",
                                             NULL,
                                             G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_DIRECTION,
                                     g_param_spec_int ("direction",
                                             "Direction",
                                             "Direction",
                                             0,
                                             G_MAXINT,
                                             0,
                                             G_PARAM_READWRITE));
    g_object_class_install_property (object_class,
                                     PROP_WINDOW,
                                     g_param_spec_object ("window",
                                             "Window",
                                             "The navigation window",
                                             G_TYPE_OBJECT,
                                             G_PARAM_READWRITE));
}

static void
caja_navigation_action_init (CajaNavigationAction *action)
{
    action->priv = caja_navigation_action_get_instance_private (action);
}
