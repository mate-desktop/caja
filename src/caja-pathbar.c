/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* caja-pathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-icon-names.h>
#include <libcaja-private/caja-trash-monitor.h>
#include <libcaja-private/caja-dnd.h>
#include <libcaja-private/caja-icon-dnd.h>

#include "caja-pathbar.h"

enum
{
    PATH_CLICKED,
    PATH_EVENT,
    LAST_SIGNAL
};

typedef enum
{
    NORMAL_BUTTON,
    ROOT_BUTTON,
    HOME_BUTTON,
    DESKTOP_BUTTON,
    MOUNT_BUTTON,
    DEFAULT_LOCATION_BUTTON,
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *)(x))

#define SCROLL_TIMEOUT           150
#define INITIAL_SCROLL_TIMEOUT   300

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

static gboolean desktop_is_home;

#define CAJA_PATH_BAR_ICON_SIZE 16

typedef struct _ButtonData ButtonData;

struct _ButtonData
{
    GtkWidget *button;
    ButtonType type;
    char *dir_name;
    GFile *path;
    CajaFile *file;
    unsigned int file_changed_signal_id;

    /* custom icon */
    cairo_surface_t *custom_icon;

    /* flag to indicate its the base folder in the URI */
    gboolean is_base_dir;

    GtkWidget *image;
    GtkWidget *label;
    guint ignore_changes : 1;
    guint file_is_hidden : 1;
    guint fake_root : 1;

    CajaDragSlotProxyInfo drag_info;
};

G_DEFINE_TYPE (CajaPathBar,
               caja_path_bar,
               GTK_TYPE_CONTAINER);

static void     caja_path_bar_finalize                 (GObject         *object);
static void     caja_path_bar_dispose                  (GObject         *object);

static void     caja_path_bar_get_preferred_width      (GtkWidget        *widget,
        						gint             *minimum,
        						gint             *natural);
static void     caja_path_bar_get_preferred_height     (GtkWidget        *widget,
        						gint             *minimum,
        						gint             *natural);

static void     caja_path_bar_unmap                    (GtkWidget       *widget);
static void     caja_path_bar_size_allocate            (GtkWidget       *widget,
        GtkAllocation   *allocation);
static void     caja_path_bar_add                      (GtkContainer    *container,
        GtkWidget       *widget);
static void     caja_path_bar_remove                   (GtkContainer    *container,
        GtkWidget       *widget);
static void     caja_path_bar_forall                   (GtkContainer    *container,
        gboolean         include_internals,
        GtkCallback      callback,
        gpointer         callback_data);
static void     caja_path_bar_scroll_up                (CajaPathBar *path_bar);
static void     caja_path_bar_scroll_down              (CajaPathBar *path_bar);
static gboolean caja_path_bar_scroll                   (GtkWidget       *path_bar,
        GdkEventScroll  *scroll);
static void     caja_path_bar_stop_scrolling           (CajaPathBar *path_bar);
static gboolean caja_path_bar_slider_button_press      (GtkWidget       *widget,
        GdkEventButton  *event,
        CajaPathBar *path_bar);
static gboolean caja_path_bar_slider_button_release    (GtkWidget       *widget,
        GdkEventButton  *event,
        CajaPathBar *path_bar);
static void     caja_path_bar_grab_notify              (GtkWidget       *widget,
        gboolean         was_grabbed);
static void     caja_path_bar_state_changed            (GtkWidget       *widget,
        GtkStateType     previous_state);

static void     caja_path_bar_style_updated            (GtkWidget       *widget);

static void     caja_path_bar_screen_changed           (GtkWidget       *widget,
        GdkScreen       *previous_screen);
static void     caja_path_bar_check_icon_theme         (CajaPathBar *path_bar);
static void     caja_path_bar_update_button_appearance (ButtonData      *button_data);
static void     caja_path_bar_update_button_state      (ButtonData      *button_data,
        gboolean         current_dir);
static gboolean caja_path_bar_update_path              (CajaPathBar *path_bar,
        GFile           *file_path,
        gboolean         emit_signal);


static GtkWidget *
get_slider_button (CajaPathBar  *path_bar,
                   const gchar  *arrow_type)
{
    GtkWidget *button;

    button = gtk_button_new ();
    gtk_widget_set_focus_on_click (button, FALSE);
    gtk_widget_add_events (button, GDK_SCROLL_MASK);
    gtk_container_add (GTK_CONTAINER (button),
                       gtk_image_new_from_icon_name (arrow_type, GTK_ICON_SIZE_MENU));
    gtk_container_add (GTK_CONTAINER (path_bar), button);
    gtk_widget_show_all (button);

    return button;
}

static void
update_button_types (CajaPathBar *path_bar)
{
    GList *list;
    GFile *path = NULL;

    for (list = path_bar->button_list; list; list = list->next)
    {
        ButtonData *button_data;
        button_data = BUTTON_DATA (list->data);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)))
        {
            /*Increase the reference count on path so it does not get cleared
             *by caja_path_bar_clear_buttons during caja_path_bar_update_path
             */
            path = g_object_ref (button_data->path);
            break;
        }
    }
    if (path != NULL)
    {
        caja_path_bar_update_path (path_bar, path, TRUE);
        g_object_unref (path);
    }
}


static void
desktop_location_changed_callback (gpointer user_data)
{
    CajaPathBar *path_bar;

    path_bar = CAJA_PATH_BAR (user_data);

    g_object_unref (path_bar->desktop_path);
    g_object_unref (path_bar->home_path);
    path_bar->desktop_path = caja_get_desktop_location ();
    path_bar->home_path = g_file_new_for_path (g_get_home_dir ());
    desktop_is_home = g_file_equal (path_bar->home_path, path_bar->desktop_path);

    update_button_types (path_bar);
}

static void
trash_state_changed_cb (CajaTrashMonitor *monitor,
                        gboolean state,
                        CajaPathBar *path_bar)
{
    GFile *file;
    GList *list;
    gint scale;

    file = g_file_new_for_uri ("trash:///");
    scale = gtk_widget_get_scale_factor (GTK_WIDGET (path_bar));
    for (list = path_bar->button_list; list; list = list->next)
    {
        ButtonData *button_data;
        button_data = BUTTON_DATA (list->data);
        if (g_file_equal (file, button_data->path))
        {
            GIcon *icon;
            CajaIconInfo *icon_info;
            cairo_surface_t *surface;

            icon = caja_trash_monitor_get_icon ();
            icon_info = caja_icon_info_lookup (icon, CAJA_PATH_BAR_ICON_SIZE, scale);
            surface = caja_icon_info_get_surface_at_size (icon_info, CAJA_PATH_BAR_ICON_SIZE);
            gtk_image_set_from_surface (GTK_IMAGE (button_data->image), surface);
        }
    }
    g_object_unref (file);
}

static gboolean
slider_timeout (gpointer user_data)
{
    CajaPathBar *path_bar;

    path_bar = CAJA_PATH_BAR (user_data);

    path_bar->drag_slider_timeout = 0;

    if (gtk_widget_get_visible (GTK_WIDGET (path_bar)))
    {
        if (path_bar->drag_slider_timeout_for_up_button)
        {
            caja_path_bar_scroll_up (path_bar);
        }
        else
        {
            caja_path_bar_scroll_down (path_bar);
        }
    }

    return FALSE;
}

static void
caja_path_bar_slider_drag_motion (GtkWidget      *widget,
                                  GdkDragContext *context,
                                  int             x,
                                  int             y,
                                  unsigned int    time,
                                  gpointer        user_data)
{
    CajaPathBar *path_bar;
    unsigned int timeout;

    path_bar = CAJA_PATH_BAR (user_data);

    if (path_bar->drag_slider_timeout == 0)
    {
        GtkSettings *settings;

        settings = gtk_widget_get_settings (widget);

        g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);
        path_bar->drag_slider_timeout =
            g_timeout_add (timeout,
                           slider_timeout,
                           path_bar);

        path_bar->drag_slider_timeout_for_up_button =
            widget == path_bar->up_slider_button;
    }
}

static void
caja_path_bar_slider_drag_leave (GtkWidget      *widget,
                                 GdkDragContext *context,
                                 unsigned int    time,
                                 gpointer        user_data)
{
    CajaPathBar *path_bar;

    path_bar = CAJA_PATH_BAR (user_data);

    if (path_bar->drag_slider_timeout != 0)
    {
        g_source_remove (path_bar->drag_slider_timeout);
        path_bar->drag_slider_timeout = 0;
    }
}

static void
caja_path_bar_init (CajaPathBar *path_bar)
{
    char *p;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (GTK_WIDGET (path_bar));
    gtk_style_context_add_class (context, "caja-pathbar");

    gtk_widget_set_has_window (GTK_WIDGET (path_bar), FALSE);
    gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

    path_bar->spacing = 3;
    path_bar->up_slider_button = get_slider_button (path_bar, "pan-start-symbolic");
    path_bar->down_slider_button = get_slider_button (path_bar, "pan-end-symbolic");
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (path_bar->up_slider_button)),
                                 "slider-button");
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (path_bar->down_slider_button)),
                                 "slider-button");

    path_bar->icon_size = CAJA_PATH_BAR_ICON_SIZE;

    p = caja_get_desktop_directory ();
    path_bar->desktop_path = g_file_new_for_path (p);
    g_free (p);
    path_bar->home_path = g_file_new_for_path (g_get_home_dir ());
    path_bar->root_path = g_file_new_for_path ("/");
    path_bar->current_path = NULL;
    path_bar->current_button_data = NULL;

    desktop_is_home = g_file_equal (path_bar->home_path, path_bar->desktop_path);

    g_signal_connect_swapped (caja_preferences, "changed::" CAJA_PREFERENCES_DESKTOP_IS_HOME_DIR,
                              G_CALLBACK(desktop_location_changed_callback),
                              path_bar);

    g_signal_connect_swapped (path_bar->up_slider_button, "clicked", G_CALLBACK (caja_path_bar_scroll_up), path_bar);
    g_signal_connect_swapped (path_bar->down_slider_button, "clicked", G_CALLBACK (caja_path_bar_scroll_down), path_bar);

    g_signal_connect (path_bar->up_slider_button, "button_press_event", G_CALLBACK (caja_path_bar_slider_button_press), path_bar);
    g_signal_connect (path_bar->up_slider_button, "button_release_event", G_CALLBACK (caja_path_bar_slider_button_release), path_bar);
    g_signal_connect (path_bar->down_slider_button, "button_press_event", G_CALLBACK (caja_path_bar_slider_button_press), path_bar);
    g_signal_connect (path_bar->down_slider_button, "button_release_event", G_CALLBACK (caja_path_bar_slider_button_release), path_bar);

    gtk_drag_dest_set (GTK_WIDGET (path_bar->up_slider_button),
                       0, NULL, 0, 0);
    gtk_drag_dest_set_track_motion (GTK_WIDGET (path_bar->up_slider_button), TRUE);
    g_signal_connect (path_bar->up_slider_button,
                      "drag-motion",
                      G_CALLBACK (caja_path_bar_slider_drag_motion),
                      path_bar);
    g_signal_connect (path_bar->up_slider_button,
                      "drag-leave",
                      G_CALLBACK (caja_path_bar_slider_drag_leave),
                      path_bar);

    gtk_drag_dest_set (GTK_WIDGET (path_bar->down_slider_button),
                       0, NULL, 0, 0);
    gtk_drag_dest_set_track_motion (GTK_WIDGET (path_bar->up_slider_button), TRUE);
    g_signal_connect (path_bar->down_slider_button,
                      "drag-motion",
                      G_CALLBACK (caja_path_bar_slider_drag_motion),
                      path_bar);
    g_signal_connect (path_bar->down_slider_button,
                      "drag-leave",
                      G_CALLBACK (caja_path_bar_slider_drag_leave),
                      path_bar);

    g_signal_connect (caja_trash_monitor_get (),
                      "trash_state_changed",
                      G_CALLBACK (trash_state_changed_cb),
                      path_bar);
}

static void
caja_path_bar_class_init (CajaPathBarClass *path_bar_class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;
    GtkContainerClass *container_class;

    gobject_class = (GObjectClass *) path_bar_class;
    widget_class = (GtkWidgetClass *) path_bar_class;
    container_class = (GtkContainerClass *) path_bar_class;

    gobject_class->finalize = caja_path_bar_finalize;
    gobject_class->dispose = caja_path_bar_dispose;

    widget_class->get_preferred_height = caja_path_bar_get_preferred_height;
    widget_class->get_preferred_width = caja_path_bar_get_preferred_width;

    widget_class->unmap = caja_path_bar_unmap;
    widget_class->size_allocate = caja_path_bar_size_allocate;
    widget_class->style_updated = caja_path_bar_style_updated;

    widget_class->screen_changed = caja_path_bar_screen_changed;
    widget_class->grab_notify = caja_path_bar_grab_notify;
    widget_class->state_changed = caja_path_bar_state_changed;
    widget_class->scroll_event = caja_path_bar_scroll;

    container_class->add = caja_path_bar_add;
    container_class->forall = caja_path_bar_forall;
    container_class->remove = caja_path_bar_remove;

    path_bar_signals [PATH_CLICKED] =
        g_signal_new ("path-clicked",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (CajaPathBarClass, path_clicked),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      G_TYPE_FILE);

    path_bar_signals [PATH_EVENT] =
       g_signal_new ("path-event",
                      G_OBJECT_CLASS_TYPE (path_bar_class),
                      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaPathBarClass, path_event),
                      NULL, NULL, NULL,
                      G_TYPE_BOOLEAN, 2,
                      G_TYPE_FILE,
                      GDK_TYPE_EVENT);

    gtk_container_class_handle_border_width (container_class);
}


static void
caja_path_bar_finalize (GObject *object)
{
    CajaPathBar *path_bar;

    path_bar = CAJA_PATH_BAR (object);

    caja_path_bar_stop_scrolling (path_bar);

    if (path_bar->drag_slider_timeout != 0)
    {
        g_source_remove (path_bar->drag_slider_timeout);
        path_bar->drag_slider_timeout = 0;
    }

    g_list_free (path_bar->button_list);
    if (path_bar->root_path)
    {
        g_object_unref (path_bar->root_path);
        path_bar->root_path = NULL;
    }
    if (path_bar->home_path)
    {
        g_object_unref (path_bar->home_path);
        path_bar->home_path = NULL;
    }
    if (path_bar->desktop_path)
    {
        g_object_unref (path_bar->desktop_path);
        path_bar->desktop_path = NULL;
    }

    g_signal_handlers_disconnect_by_func (caja_trash_monitor_get (),
                                          trash_state_changed_cb, path_bar);
    g_signal_handlers_disconnect_by_func (caja_preferences,
                                          desktop_location_changed_callback,
                                          path_bar);

    G_OBJECT_CLASS (caja_path_bar_parent_class)->finalize (object);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (CajaPathBar *path_bar,
                        GdkScreen  *screen)
{
    if (path_bar->settings_signal_id)
    {
        GtkSettings *settings;

        settings = gtk_settings_get_for_screen (screen);
        g_signal_handler_disconnect (settings,
                                     path_bar->settings_signal_id);
        path_bar->settings_signal_id = 0;
    }
}

static void
caja_path_bar_dispose (GObject *object)
{
    remove_settings_signal (CAJA_PATH_BAR (object), gtk_widget_get_screen (GTK_WIDGET (object)));

    G_OBJECT_CLASS (caja_path_bar_parent_class)->dispose (object);
}

/* Size requisition:
 *
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */

static void
caja_path_bar_get_preferred_width (GtkWidget *widget,
    			       gint      *minimum,
    			       gint      *natural)
{
    CajaPathBar *path_bar;
    GList *list;
    gint child_height;
    gint height;
    gint child_min, child_nat;
    gint slider_width;
    ButtonData *button_data = NULL;

    path_bar = CAJA_PATH_BAR (widget);

    *minimum = *natural = 0;
    height = 0;

    for (list = path_bar->button_list; list; list = list->next) {
    	button_data = BUTTON_DATA (list->data);
    	gtk_widget_get_preferred_width (button_data->button, &child_min, &child_nat);
    	gtk_widget_get_preferred_height (button_data->button, &child_height, NULL);
    	height = MAX (height, child_height);

    	if (button_data->type == NORMAL_BUTTON) {
    		/* Use 2*Height as button width because of ellipsized label.  */
    		child_min = MAX (child_min, child_height * 2);
    		child_nat = MAX (child_min, child_height * 2);
    	}

    	*minimum = MAX (*minimum, child_min);
    	*natural = MAX (*natural, child_nat);
    }

    /* Add space for slider, if we have more than one path */
    /* Theoretically, the slider could be bigger than the other button.  But we're
     * not going to worry about that now.
     */
    gtk_widget_get_preferred_width (path_bar->down_slider_button,
                                    &slider_width,
                                    NULL);
    gtk_widget_get_preferred_width (path_bar->up_slider_button,
                                    &slider_width,
                                    NULL);


    if (path_bar->button_list) {
        *minimum += (path_bar->spacing + slider_width) * 2;
        *natural += (path_bar->spacing + slider_width) * 2;
    }
    /*Let's keep the rest of this as it was */
    path_bar->slider_width = slider_width;
}

static void
caja_path_bar_get_preferred_height (GtkWidget *widget,
    				gint      *minimum,
    				gint      *natural)
{
    CajaPathBar *path_bar;
    GList *list;
    gint child_min, child_nat;
    ButtonData *button_data = NULL;

    path_bar = CAJA_PATH_BAR (widget);

    *minimum = *natural = 0;

    for (list = path_bar->button_list; list; list = list->next) {
    	button_data = BUTTON_DATA (list->data);
    	gtk_widget_get_preferred_height (button_data->button, &child_min, &child_nat);

    	*minimum = MAX (*minimum, child_min);
    	*natural = MAX (*natural, child_nat);
    }
}

static void
caja_path_bar_update_slider_buttons (CajaPathBar *path_bar)
{
    if (path_bar->button_list)
    {

        GtkWidget *button;

        button = BUTTON_DATA (path_bar->button_list->data)->button;
        if (gtk_widget_get_child_visible (button))
        {
            gtk_widget_set_sensitive (path_bar->down_slider_button, FALSE);
        }
        else
        {
            gtk_widget_set_sensitive (path_bar->down_slider_button, TRUE);
        }
        button = BUTTON_DATA (g_list_last (path_bar->button_list)->data)->button;
        if (gtk_widget_get_child_visible (button))
        {
            gtk_widget_set_sensitive (path_bar->up_slider_button, FALSE);
        }
        else
        {
            gtk_widget_set_sensitive (path_bar->up_slider_button, TRUE);
        }
    }
}

static void
caja_path_bar_unmap (GtkWidget *widget)
{
    caja_path_bar_stop_scrolling (CAJA_PATH_BAR (widget));

    GTK_WIDGET_CLASS (caja_path_bar_parent_class)->unmap (widget);
}

/* This is a tad complicated */
static void
caja_path_bar_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
    GtkWidget *child;
    CajaPathBar *path_bar;
    GtkTextDirection direction;
    GtkAllocation child_allocation;
    GList *list, *first_button;
    gint width;
    gint allocation_width;
    gboolean need_sliders;
    gint up_slider_offset;
    gint down_slider_offset;
    GtkRequisition child_requisition;
    GtkAllocation widget_allocation;

    need_sliders = TRUE;
    up_slider_offset = 0;
    down_slider_offset = 0;
    path_bar = CAJA_PATH_BAR (widget);

    gtk_widget_set_allocation (widget, allocation);

    /* No path is set so we don't have to allocate anything. */
    if (path_bar->button_list == NULL)
    {
        return;
    }
    direction = gtk_widget_get_direction (widget);

    allocation_width = allocation->width;

    /* First, we check to see if we need the scrollbars. */
    if (path_bar->fake_root)
    {
        width = path_bar->spacing + path_bar->slider_width;
    }
    else
    {
        width = 0;
    }

    gtk_widget_get_preferred_size (BUTTON_DATA (path_bar->button_list->data)->button,
    				   &child_requisition, NULL);
    width += child_requisition.width;

    for (list = path_bar->button_list->next; list; list = list->next)
    {
        child = BUTTON_DATA (list->data)->button;
        gtk_widget_get_preferred_size (child, &child_requisition, NULL);
        width += child_requisition.width + path_bar->spacing;

        if (list == path_bar->fake_root)
        {
            break;
        }
    }

    if (width <= allocation_width)
    {
        if (path_bar->fake_root)
        {
            first_button = path_bar->fake_root;
        }
        else
        {
            first_button = g_list_last (path_bar->button_list);
        }
    }
    else
    {
        gboolean reached_end;
        gint slider_space;
        reached_end = FALSE;
        slider_space = 2 * (path_bar->spacing + path_bar->slider_width);

        if (path_bar->first_scrolled_button)
        {
            first_button = path_bar->first_scrolled_button;
        }
        else
        {
            first_button = path_bar->button_list;
        }

        need_sliders = TRUE;
        /* To see how much space we have, and how many buttons we can display.
        * We start at the first button, count forward until hit the new
        * button, then count backwards.
        */
        /* Count down the path chain towards the end. */
        gtk_widget_get_preferred_size (BUTTON_DATA (first_button->data)->button,
        			       &child_requisition, NULL);
        width = child_requisition.width;
        list = first_button->prev;
        while (list && !reached_end)
        {
            child = BUTTON_DATA (list->data)->button;
            gtk_widget_get_preferred_size (child, &child_requisition, NULL);

            if (width + child_requisition.width + path_bar->spacing + slider_space > allocation_width)
            {
                reached_end = TRUE;
            }
            else
            {
                if (list == path_bar->fake_root)
                {
                    break;
                }
                else
                {
                    width += child_requisition.width + path_bar->spacing;
                }
            }

            list = list->prev;
        }

        /* Finally, we walk up, seeing how many of the previous buttons we can add*/

        while (first_button->next && ! reached_end)
        {
            child = BUTTON_DATA (first_button->next->data)->button;
            gtk_widget_get_preferred_size (child, &child_requisition, NULL);

            if (width + child_requisition.width + path_bar->spacing + slider_space > allocation_width)
            {
                reached_end = TRUE;
            }
            else
            {
                width += child_requisition.width + path_bar->spacing;
                if (first_button == path_bar->fake_root)
                {
                    break;
                }
                first_button = first_button->next;
            }
        }
    }

    /* Now, we allocate space to the buttons */
    child_allocation.y = allocation->y;
    child_allocation.height = allocation->height;

    if (direction == GTK_TEXT_DIR_RTL)
    {
        child_allocation.x = allocation->x + allocation->width;

        if (need_sliders || path_bar->fake_root)
        {
            child_allocation.x -= (path_bar->spacing + path_bar->slider_width);
            up_slider_offset = allocation->width - path_bar->slider_width;

        }
    }
    else
    {
        child_allocation.x = allocation->x;

        if (need_sliders || path_bar->fake_root)
        {
            up_slider_offset = 0;
            child_allocation.x += (path_bar->spacing + path_bar->slider_width);
        }
    }

    for (list = first_button; list; list = list->prev)
    {
        child = BUTTON_DATA (list->data)->button;
        gtk_widget_get_preferred_size (child, &child_requisition, NULL);

        gtk_widget_get_allocation (widget, &widget_allocation);

        child_allocation.width = child_requisition.width;
        if (direction == GTK_TEXT_DIR_RTL)
        {
            child_allocation.x -= child_allocation.width;
        }
        /* Check to see if we've don't have any more space to allocate buttons */
        if (need_sliders && direction == GTK_TEXT_DIR_RTL)
        {
            if (child_allocation.x - path_bar->spacing - path_bar->slider_width < widget_allocation.x)
            {
                break;
            }
        }
        else
        {
            if (need_sliders && direction == GTK_TEXT_DIR_LTR)
            {
                if (child_allocation.x + child_allocation.width + path_bar->spacing + path_bar->slider_width > widget_allocation.x + allocation_width)

                {
                    break;
                }
            }
        }

        gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, TRUE);
        gtk_widget_size_allocate (child, &child_allocation);

        if (direction == GTK_TEXT_DIR_RTL)
        {
            child_allocation.x -= path_bar->spacing;
            down_slider_offset = child_allocation.x - allocation->x - path_bar->slider_width;
        }
        else
        {
            down_slider_offset = child_allocation.x - widget_allocation.x;
            down_slider_offset += child_allocation.width + path_bar->spacing;

            child_allocation.x += child_allocation.width + path_bar->spacing;
        }
    }
    /* Now we go hide all the widgets that don't fit */
    while (list)
    {
        gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
        list = list->prev;
    }
    for (list = first_button->next; list; list = list->next)
    {
        gtk_widget_set_child_visible (BUTTON_DATA (list->data)->button, FALSE);
    }

    if (need_sliders || path_bar->fake_root)
    {
        child_allocation.width = path_bar->slider_width;
        child_allocation.x = up_slider_offset + allocation->x;
        gtk_widget_size_allocate (path_bar->up_slider_button, &child_allocation);

        gtk_widget_set_child_visible (path_bar->up_slider_button, TRUE);
        gtk_widget_show_all (path_bar->up_slider_button);
    }
    else
    {
        gtk_widget_set_child_visible (path_bar->up_slider_button, FALSE);
    }

    if (need_sliders)
    {
        child_allocation.width = path_bar->slider_width;
        child_allocation.x = down_slider_offset + allocation->x;
        gtk_widget_size_allocate (path_bar->down_slider_button, &child_allocation);

        gtk_widget_set_child_visible (path_bar->down_slider_button, TRUE);
        gtk_widget_show_all (path_bar->down_slider_button);
        caja_path_bar_update_slider_buttons (path_bar);
    }
    else
    {
        gtk_widget_set_child_visible (path_bar->down_slider_button, FALSE);
    }
}

static void
caja_path_bar_style_updated (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (caja_path_bar_parent_class)->style_updated)
    {
        GTK_WIDGET_CLASS (caja_path_bar_parent_class)->style_updated (widget);
    }

    caja_path_bar_check_icon_theme (CAJA_PATH_BAR (widget));
}

static void
caja_path_bar_screen_changed (GtkWidget *widget,
                              GdkScreen *previous_screen)
{
    if (GTK_WIDGET_CLASS (caja_path_bar_parent_class)->screen_changed)
    {
        GTK_WIDGET_CLASS (caja_path_bar_parent_class)->screen_changed (widget, previous_screen);
    }
    /* We might nave a new settings, so we remove the old one */
    if (previous_screen)
    {
        remove_settings_signal (CAJA_PATH_BAR (widget), previous_screen);
    }
    caja_path_bar_check_icon_theme (CAJA_PATH_BAR (widget));
}

static gboolean
caja_path_bar_scroll (GtkWidget      *widget,
                      GdkEventScroll *event)
{
    CajaPathBar *path_bar;

    path_bar = CAJA_PATH_BAR (widget);

    switch (event->direction)
    {
    case GDK_SCROLL_RIGHT:
    case GDK_SCROLL_DOWN:
        caja_path_bar_scroll_down (path_bar);
        return TRUE;

    case GDK_SCROLL_LEFT:
    case GDK_SCROLL_UP:
        caja_path_bar_scroll_up (path_bar);
        return TRUE;

    case GDK_SCROLL_SMOOTH:
        break;
    }

    return FALSE;
}


static void
caja_path_bar_add (GtkContainer *container,
                   GtkWidget    *widget)
{
    gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
caja_path_bar_remove_1 (GtkContainer *container,
                        GtkWidget    *widget)
{
    gboolean was_visible = gtk_widget_get_visible (widget);
    gtk_widget_unparent (widget);
    if (was_visible)
    {
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
caja_path_bar_remove (GtkContainer *container,
                      GtkWidget    *widget)
{
    CajaPathBar *path_bar;
    GList *children;

    path_bar = CAJA_PATH_BAR (container);

    if (widget == path_bar->up_slider_button)
    {
        caja_path_bar_remove_1 (container, widget);
        path_bar->up_slider_button = NULL;
        return;
    }

    if (widget == path_bar->down_slider_button)
    {
        caja_path_bar_remove_1 (container, widget);
        path_bar->down_slider_button = NULL;
        return;
    }

    children = path_bar->button_list;
    while (children)
    {
        if (widget == BUTTON_DATA (children->data)->button)
        {
            caja_path_bar_remove_1 (container, widget);
            path_bar->button_list = g_list_remove_link (path_bar->button_list, children);
            g_list_free_1 (children);
            return;
        }
        children = children->next;
    }
}

static void
caja_path_bar_forall (GtkContainer *container,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
    CajaPathBar *path_bar;
    GList *children;

    g_return_if_fail (callback != NULL);
    path_bar = CAJA_PATH_BAR (container);

    children = path_bar->button_list;
    while (children)
    {
        GtkWidget *child;
        child = BUTTON_DATA (children->data)->button;
        children = children->next;
        (* callback) (child, callback_data);
    }

    if (path_bar->up_slider_button)
    {
        (* callback) (path_bar->up_slider_button, callback_data);
    }

    if (path_bar->down_slider_button)
    {
        (* callback) (path_bar->down_slider_button, callback_data);
    }
}

static void
caja_path_bar_scroll_down (CajaPathBar *path_bar)
{
    GList *list;
    GList *down_button;
    GList *up_button;
    gint space_available;
    gint space_needed;
    GtkTextDirection direction;
    GtkAllocation allocation, button_allocation, slider_allocation;

    down_button = NULL;
    up_button = NULL;

    if (path_bar->ignore_click)
    {
        path_bar->ignore_click = FALSE;
        return;
    }

    gtk_widget_queue_resize (GTK_WIDGET (path_bar));

    direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));

    /* We find the button at the 'down' end that we have to make */
    /* visible */
    for (list = path_bar->button_list; list; list = list->next)
    {
        if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->button))
        {
            down_button = list;
            break;
        }
    }

    if (down_button == NULL)
    {
        return;
    }

    /* Find the last visible button on the 'up' end */
    for (list = g_list_last (path_bar->button_list); list; list = list->prev)
    {
        if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button))
        {
            up_button = list;
            break;
        }
    }

    gtk_widget_get_allocation (BUTTON_DATA (down_button->data)->button, &button_allocation);
    gtk_widget_get_allocation (GTK_WIDGET (path_bar), &allocation);
    gtk_widget_get_allocation (path_bar->down_slider_button, &slider_allocation);

    space_needed = button_allocation.width + path_bar->spacing;
    if (direction == GTK_TEXT_DIR_RTL)
    {
        space_available = slider_allocation.x - allocation.x;
    }
    else
    {
        space_available = (allocation.x + allocation.width) -
                          (slider_allocation.x + slider_allocation.width);
    }

    /* We have space_available extra space that's not being used.  We
    * need space_needed space to make the button fit.  So we walk down
    * from the end, removing buttons until we get all the space we
    * need. */
    gtk_widget_get_allocation (BUTTON_DATA (up_button->data)->button, &button_allocation);
    while (space_available < space_needed && up_button)
    {
        space_available += button_allocation.width + path_bar->spacing;
        up_button = up_button->prev;
        path_bar->first_scrolled_button = up_button;
    }
}

static void
caja_path_bar_scroll_up (CajaPathBar *path_bar)
{
    GList *list;

    if (path_bar->ignore_click)
    {
        path_bar->ignore_click = FALSE;
        return;
    }

    gtk_widget_queue_resize (GTK_WIDGET (path_bar));

    for (list = g_list_last (path_bar->button_list); list; list = list->prev)
    {
        if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->button))
        {
            if (list->prev == path_bar->fake_root)
            {
                path_bar->fake_root = NULL;
            }
            path_bar->first_scrolled_button = list;
            return;
        }
    }
}

static gboolean
caja_path_bar_scroll_timeout (CajaPathBar *path_bar)
{
    gboolean retval = FALSE;

    if (path_bar->timer)
    {
        if (gtk_widget_has_focus (path_bar->up_slider_button))
        {
            caja_path_bar_scroll_up (path_bar);
        }
        else
        {
            if (gtk_widget_has_focus (path_bar->down_slider_button))
            {
                caja_path_bar_scroll_down (path_bar);
            }
        }
        if (path_bar->need_timer)
        {
            path_bar->need_timer = FALSE;

            path_bar->timer = g_timeout_add (SCROLL_TIMEOUT,
                                             (GSourceFunc)caja_path_bar_scroll_timeout,
                                             path_bar);

        }
        else
        {
            retval = TRUE;
        }
    }

    return retval;
}

static void
caja_path_bar_stop_scrolling (CajaPathBar *path_bar)
{
    if (path_bar->timer)
    {
        g_source_remove (path_bar->timer);
        path_bar->timer = 0;
        path_bar->need_timer = FALSE;
    }
}

static gboolean
caja_path_bar_slider_button_press (GtkWidget       *widget,
                                   GdkEventButton  *event,
                                   CajaPathBar *path_bar)
{
    if (!gtk_widget_has_focus (widget))
    {
        gtk_widget_grab_focus (widget);
    }

    if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    {
        return FALSE;
    }

    path_bar->ignore_click = FALSE;

    if (widget == path_bar->up_slider_button)
    {
        caja_path_bar_scroll_up (path_bar);
    }
    else
    {
        if (widget == path_bar->down_slider_button)
        {
            caja_path_bar_scroll_down (path_bar);
        }
    }

    if (!path_bar->timer)
    {
        path_bar->need_timer = TRUE;
        path_bar->timer = g_timeout_add (INITIAL_SCROLL_TIMEOUT,
                                         (GSourceFunc)caja_path_bar_scroll_timeout,
                                         path_bar);
    }

    return FALSE;
}

static gboolean
caja_path_bar_slider_button_release (GtkWidget      *widget,
                                     GdkEventButton *event,
                                     CajaPathBar     *path_bar)
{
    if (event->type != GDK_BUTTON_RELEASE)
    {
        return FALSE;
    }

    path_bar->ignore_click = TRUE;
    caja_path_bar_stop_scrolling (path_bar);

    return FALSE;
}

static void
caja_path_bar_grab_notify (GtkWidget *widget,
                           gboolean   was_grabbed)
{
    if (!was_grabbed)
    {
        caja_path_bar_stop_scrolling (CAJA_PATH_BAR (widget));
    }
}

static void
caja_path_bar_state_changed (GtkWidget    *widget,
                             GtkStateType  previous_state)
{
    if (!gtk_widget_get_sensitive (widget))
    {
        caja_path_bar_stop_scrolling (CAJA_PATH_BAR (widget));
    }
}



/* Changes the icons wherever it is needed */
static void
reload_icons (CajaPathBar *path_bar)
{
    GList *list;

    for (list = path_bar->button_list; list; list = list->next)
    {
        ButtonData *button_data;

        button_data = BUTTON_DATA (list->data);
        if (button_data->type != NORMAL_BUTTON || button_data->is_base_dir)
        {
            caja_path_bar_update_button_appearance (button_data);
        }

    }
}

static void
change_icon_theme (CajaPathBar *path_bar)
{
    path_bar->icon_size = CAJA_PATH_BAR_ICON_SIZE;
    reload_icons (path_bar);
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject    *object,
                    GParamSpec *pspec,
                    CajaPathBar *path_bar)
{
    const char *name;

    name = g_param_spec_get_name (pspec);

    if (! strcmp (name, "gtk-icon-theme-name") || ! strcmp (name, "gtk-icon-sizes"))
    {
        change_icon_theme (path_bar);
    }
}

static void
caja_path_bar_check_icon_theme (CajaPathBar *path_bar)
{
    GtkSettings *settings;

    if (path_bar->settings_signal_id)
    {
        return;
    }

    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_bar)));
    path_bar->settings_signal_id = g_signal_connect (settings, "notify", G_CALLBACK (settings_notify_cb), path_bar);

    change_icon_theme (path_bar);
}

/* Public functions and their helpers */
void
caja_path_bar_clear_buttons (CajaPathBar *path_bar)
{
    while (path_bar->button_list != NULL)
    {
        gtk_container_remove (GTK_CONTAINER (path_bar), BUTTON_DATA (path_bar->button_list->data)->button);
    }
    path_bar->first_scrolled_button = NULL;
    path_bar->fake_root = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
                   gpointer   data)
{
    ButtonData *button_data;
    CajaPathBar *path_bar;
    GList *button_list;

    button_data = BUTTON_DATA (data);
    if (button_data->ignore_changes)
    {
        return;
    }

    path_bar = CAJA_PATH_BAR (gtk_widget_get_parent (button));

    button_list = g_list_find (path_bar->button_list, button_data);
    g_assert (button_list != NULL);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

    g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0, button_data->path);
}

static gboolean
button_event_cb (GtkWidget *button,
		 GdkEventButton *event,
		 gpointer   data)
{
        ButtonData *button_data;
        CajaPathBar *path_bar;
        GList *button_list;
        gboolean retval;

        button_data = BUTTON_DATA (data);
        path_bar = CAJA_PATH_BAR (gtk_widget_get_parent (button));

	if (event->type == GDK_BUTTON_PRESS) {
		g_object_set_data (G_OBJECT (button), "handle-button-release",
				   GINT_TO_POINTER (TRUE));
	}

	if (event->type == GDK_BUTTON_RELEASE &&
	    !GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button),
						  "handle-button-release"))) {
		return FALSE;
	}

        button_list = g_list_find (path_bar->button_list, button_data);
        g_assert (button_list != NULL);

        g_signal_emit (path_bar, path_bar_signals [PATH_EVENT], 0, button_data->path, event, &retval);

	return retval;
}

static void
button_drag_begin_cb (GtkWidget *widget,
		      GdkDragContext *drag_context,
		      gpointer user_data)
{
	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (FALSE));
}


static CajaIconInfo *
get_type_icon_info (ButtonData *button_data)
{
    gint icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (button_data->button));

    switch (button_data->type)
    {
    case ROOT_BUTTON:
        return caja_icon_info_lookup_from_name (CAJA_ICON_FILESYSTEM,
                                                CAJA_PATH_BAR_ICON_SIZE,
                                                icon_scale);

    case HOME_BUTTON:
        return caja_icon_info_lookup_from_name (CAJA_ICON_HOME,
                                                CAJA_PATH_BAR_ICON_SIZE,
                                                icon_scale);

    case DESKTOP_BUTTON:
        return caja_icon_info_lookup_from_name (CAJA_ICON_DESKTOP,
                                                CAJA_PATH_BAR_ICON_SIZE,
                                                icon_scale);

    case NORMAL_BUTTON:
        if (button_data->is_base_dir)
        {
            return caja_file_get_icon (button_data->file,
                                       CAJA_PATH_BAR_ICON_SIZE, icon_scale,
                                       CAJA_FILE_ICON_FLAGS_NONE);
        }

    default:
        return NULL;
    }

    return NULL;
}

static void
button_data_free (ButtonData *button_data)
{
    g_object_unref (button_data->path);
    g_free (button_data->dir_name);
    if (button_data->custom_icon)
    {
        cairo_surface_destroy (button_data->custom_icon);
    }
    if (button_data->file != NULL)
    {
        g_signal_handler_disconnect (button_data->file,
                                     button_data->file_changed_signal_id);
        caja_file_monitor_remove (button_data->file, button_data);
        caja_file_unref (button_data->file);
    }

    g_object_unref (button_data->drag_info.target_location);
    button_data->drag_info.target_location = NULL;

    g_free (button_data);
}

static const char *
get_dir_name (ButtonData *button_data)
{
    if (button_data->type == DESKTOP_BUTTON)
    {
        return _("Desktop");
    }
    else
    {
        return button_data->dir_name;
    }
}

/* We always want to request the same size for the label, whether
 * or not the contents are bold
 */
static void
set_label_padding_size (ButtonData *button_data)
{
    const gchar *dir_name = get_dir_name (button_data);
    PangoLayout *layout;
    gint width, height, bold_width, bold_height;
    gint pad_left, pad_right;
    gchar *markup;

    layout = gtk_widget_create_pango_layout (button_data->label, dir_name);
    pango_layout_get_pixel_size (layout, &width, &height);

    markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
    pango_layout_set_markup (layout, markup, -1);
    g_free (markup);

    pango_layout_get_pixel_size (layout, &bold_width, &bold_height);

    pad_left = (bold_width - width) / 2;
    pad_right = (bold_width + 1 - width) / 2; /* this ensures rounding up - the
    pixel size difference between bold and normal fonts is not always even and
    will give an off-by-one error when dividing by 2 */

    gtk_widget_set_margin_start (GTK_WIDGET (button_data->label), pad_left);
    gtk_widget_set_margin_end (GTK_WIDGET (button_data->label), pad_right);

    g_object_unref (layout);
}

static void
caja_path_bar_update_button_appearance (ButtonData *button_data)
{
    const gchar *dir_name = get_dir_name (button_data);

    if (button_data->label != NULL)
    {
        if (gtk_label_get_use_markup (GTK_LABEL (button_data->label)))
        {
            char *markup;

            markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);
            gtk_label_set_markup (GTK_LABEL (button_data->label), markup);

            gtk_widget_set_margin_end (GTK_WIDGET (button_data->label), 0);
            gtk_widget_set_margin_start (GTK_WIDGET (button_data->label), 0);
            g_free(markup);
        }
        else
        {
            gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
            set_label_padding_size (button_data);
        }
    }

    if (button_data->image != NULL)
    {
        if (button_data->custom_icon)
        {
            gtk_image_set_from_surface (GTK_IMAGE (button_data->image), button_data->custom_icon);
            gtk_widget_show (GTK_WIDGET (button_data->image));
        }
        else
        {
            CajaIconInfo *icon_info;
            cairo_surface_t *surface;

            icon_info = get_type_icon_info (button_data);
            surface = NULL;

            if (icon_info != NULL)
            {
                surface = caja_icon_info_get_surface_at_size (icon_info, CAJA_PATH_BAR_ICON_SIZE);
                g_object_unref (icon_info);
            }

            if (surface != NULL)
            {
                gtk_image_set_from_surface (GTK_IMAGE (button_data->image), surface);
                gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
                                             "image-button");
                gtk_widget_show (GTK_WIDGET (button_data->image));
                cairo_surface_destroy (surface);
            }
            else
            {
                gtk_widget_hide (GTK_WIDGET (button_data->image));
                gtk_style_context_remove_class (gtk_widget_get_style_context (button_data->button),
                                                "image-button");
            }
        }
    }

}

static void
caja_path_bar_update_button_state (ButtonData *button_data,
                                   gboolean    current_dir)
{
    if (button_data->label != NULL)
    {
        g_object_set (button_data->label, "use-markup", current_dir, NULL);
    }

    caja_path_bar_update_button_appearance (button_data);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir)
    {
        button_data->ignore_changes = TRUE;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
        button_data->ignore_changes = FALSE;
    }
}

static gboolean
setup_file_path_mounted_mount (GFile *location, ButtonData *button_data)
{
    GVolumeMonitor *volume_monitor;
    GList *mounts, *l;
    gboolean result;
    GIcon *icon;
    CajaIconInfo *info;
    GFile *root, *default_location;
    gint scale;
    GMount *mount = NULL;

    /* Return false if button has not been set up yet or has been destroyed*/
    if (!button_data->button)
        return FALSE;
    result = FALSE;
    volume_monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (volume_monitor);

    scale = gtk_widget_get_scale_factor (GTK_WIDGET (button_data->button));
    for (l = mounts; l != NULL; l = l->next)
    {
        mount = l->data;
        if (g_mount_is_shadowed (mount))
        {
            continue;
        }
        if (result)
        {
            continue;
        }
        root = g_mount_get_root (mount);
        if (g_file_equal (location, root))
        {
            result = TRUE;
            /* set mount specific details in button_data */
            if (button_data)
            {
                icon = g_mount_get_icon (mount);
                if (icon == NULL)
                {
                    icon = g_themed_icon_new (CAJA_ICON_FOLDER);
                }
                info = caja_icon_info_lookup (icon, CAJA_PATH_BAR_ICON_SIZE, scale);
                g_object_unref (icon);
                button_data->custom_icon = caja_icon_info_get_surface_at_size (info, CAJA_PATH_BAR_ICON_SIZE);
                g_object_unref (info);
                button_data->dir_name = g_mount_get_name (mount);
                button_data->type = MOUNT_BUTTON;
                button_data->fake_root = TRUE;
            }
            g_object_unref (root);
            break;
        }
        default_location = g_mount_get_default_location (mount);
        if (!g_file_equal (default_location, root) &&
                g_file_equal (location, default_location))
        {
            result = TRUE;
            /* set mount specific details in button_data */
            if (button_data)
            {
                icon = g_mount_get_icon (mount);
                if (icon == NULL)
                {
                    icon = g_themed_icon_new (CAJA_ICON_FOLDER);
                }
                info = caja_icon_info_lookup (icon, CAJA_PATH_BAR_ICON_SIZE, scale);
                g_object_unref (icon);
                button_data->custom_icon = caja_icon_info_get_surface_at_size (info, CAJA_PATH_BAR_ICON_SIZE);
                g_object_unref (info);
                button_data->type = DEFAULT_LOCATION_BUTTON;
                button_data->fake_root = TRUE;
            }
            g_object_unref (default_location);
            g_object_unref (root);
            break;
        }
        g_object_unref (default_location);
        g_object_unref (root);
    }
    g_list_free_full (mounts, g_object_unref);
    return result;
}

static void
setup_button_type (ButtonData       *button_data,
                   CajaPathBar  *path_bar,
                   GFile *location)
{
    if (path_bar->root_path != NULL && g_file_equal (location, path_bar->root_path))
    {
        button_data->type = ROOT_BUTTON;
    }
    else if (path_bar->home_path != NULL && g_file_equal (location, path_bar->home_path))
    {
        button_data->type = HOME_BUTTON;
        button_data->fake_root = TRUE;
    }
    else if (path_bar->desktop_path != NULL && g_file_equal (location, path_bar->desktop_path))
    {
        if (!desktop_is_home)
        {
            button_data->type = DESKTOP_BUTTON;
        }
        else
        {
            button_data->type = NORMAL_BUTTON;
        }
    }
    else if (setup_file_path_mounted_mount (location, button_data))
    {
        /* already setup */
    }
    else
    {
        button_data->type = NORMAL_BUTTON;
    }
}

static void
button_drag_data_get_cb (GtkWidget          *widget,
                         GdkDragContext     *context,
                         GtkSelectionData   *selection_data,
                         guint               info,
                         guint               time_,
                         gpointer            user_data)
{
    ButtonData *button_data;
    char *uri_list[2];

    button_data = user_data;

    uri_list[0] = g_file_get_uri (button_data->path);
    uri_list[1] = NULL;

    if (info == CAJA_ICON_DND_MATE_ICON_LIST)
    {
        char *tmp;

        tmp = g_strdup_printf ("%s\r\n", uri_list[0]);
        gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data),
                                8, tmp, strlen (tmp));
        g_free (tmp);
    }
    else if (info == CAJA_ICON_DND_URI_LIST)
    {
        gtk_selection_data_set_uris (selection_data, uri_list);
    }

    g_free (uri_list[0]);
}

static void
setup_button_drag_source (ButtonData *button_data)
{
    GtkTargetList *target_list;
    const GtkTargetEntry targets[] =
    {
        { CAJA_ICON_DND_MATE_ICON_LIST_TYPE, 0, CAJA_ICON_DND_MATE_ICON_LIST }
    };

    gtk_drag_source_set (button_data->button,
                         GDK_BUTTON1_MASK |
                         GDK_BUTTON2_MASK,
                         NULL, 0,
                         GDK_ACTION_MOVE |
                         GDK_ACTION_COPY |
                         GDK_ACTION_LINK |
                         GDK_ACTION_ASK);

    target_list = gtk_target_list_new (targets, G_N_ELEMENTS (targets));
    gtk_target_list_add_uri_targets (target_list, CAJA_ICON_DND_URI_LIST);
    gtk_drag_source_set_target_list (button_data->button, target_list);
    gtk_target_list_unref (target_list);

    g_signal_connect (button_data->button, "drag-data-get",
                      G_CALLBACK (button_drag_data_get_cb),
                      button_data);
}

static void
button_data_file_changed (CajaFile *file,
                          ButtonData *button_data)
{
    GFile *location, *current_location;
    ButtonData *current_button_data;
    CajaPathBar *path_bar;
    gboolean renamed, child;

    path_bar = (CajaPathBar *) gtk_widget_get_ancestor (button_data->button,
               CAJA_TYPE_PATH_BAR);
    if (path_bar == NULL)
    {
        return;
    }

    g_return_if_fail (path_bar->current_path != NULL);
    g_return_if_fail (path_bar->current_button_data != NULL);

    current_button_data = path_bar->current_button_data;

    location = caja_file_get_location (file);
    if (!g_file_equal (button_data->path, location))
    {
        GFile *parent, *button_parent;

        parent = g_file_get_parent (location);
        button_parent = g_file_get_parent (button_data->path);

        renamed = (parent != NULL && button_parent != NULL) &&
                  g_file_equal (parent, button_parent);

        if (parent != NULL)
        {
            g_object_unref (parent);
        }
        if (button_parent != NULL)
        {
            g_object_unref (button_parent);
        }

        if (renamed)
        {
            button_data->path = g_object_ref (location);
        }
        else
        {
            /* the file has been moved.
             * If it was below the currently displayed location, remove it.
             * If it was not below the currently displayed location, update the path bar
             */
            child = g_file_has_prefix (button_data->path,
                                       path_bar->current_path);

            if (child)
            {
                /* moved file inside current path hierarchy */
                g_object_unref (location);
                location = g_file_get_parent (button_data->path);
                current_location = g_object_ref (path_bar->current_path);
            }
            else
            {
                /* moved current path, or file outside current path hierarchy.
                 * Update path bar to new locations.
                 */
                current_location = caja_file_get_location (current_button_data->file);
            }

            caja_path_bar_update_path (path_bar, location, FALSE);
            caja_path_bar_set_path (path_bar, current_location);
            g_object_unref (location);
            g_object_unref (current_location);
            return;
        }
    }
    else if (caja_file_is_gone (file))
    {
        gint idx, position;

        /* if the current or a parent location are gone, don't do anything, as the view
         * will get the event too and call us back.
         */
        current_location = caja_file_get_location (current_button_data->file);

        if (g_file_has_prefix (location, current_location))
        {
            /* remove this and the following buttons */
            position = g_list_position (path_bar->button_list,
                                        g_list_find (path_bar->button_list, button_data));

            if (position != -1)
            {
                for (idx = 0; idx <= position; idx++)
                {
                    gtk_container_remove (GTK_CONTAINER (path_bar),
                                          BUTTON_DATA (path_bar->button_list->data)->button);
                }
            }
        }

        g_object_unref (current_location);
        g_object_unref (location);
        return;
    }
    g_object_unref (location);

    /* MOUNTs use the GMount as the name, so don't update for those */
    if (button_data->type != MOUNT_BUTTON)
    {
        char *display_name;

        display_name = caja_file_get_display_name (file);
        if (g_strcmp0 (display_name, button_data->dir_name) != 0)
        {
            g_free (button_data->dir_name);
            button_data->dir_name = g_strdup (display_name);
        }

        g_free (display_name);
    }
    caja_path_bar_update_button_appearance (button_data);
}

static ButtonData *
make_directory_button (CajaPathBar  *path_bar,
                       CajaFile     *file,
                       gboolean          current_dir,
                       gboolean          base_dir,
                       gboolean          file_is_hidden)
{
    GFile *path;
    GtkWidget *child;
    ButtonData *button_data;

    path = caja_file_get_location (file);

    child = NULL;

    file_is_hidden = !! file_is_hidden;
    /* Is it a special button? */
    button_data = g_new0 (ButtonData, 1);

    setup_button_type (button_data, path_bar, path);
    button_data->button = gtk_toggle_button_new ();
    gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
                                 "text-button");
    gtk_widget_set_focus_on_click (button_data->button, FALSE);
    gtk_widget_add_events (button_data->button, GDK_SCROLL_MASK);
    /* TODO update button type when xdg directories change */

    button_data->drag_info.target_location = g_object_ref (path);

    button_data->image = gtk_image_new ();

    switch (button_data->type)
    {
    case ROOT_BUTTON:
    /* Fall through */
    case HOME_BUTTON:
    /* Fall through */
    case DESKTOP_BUTTON:
    /* Fall through */
    case MOUNT_BUTTON:
    /* Fall through */
    case DEFAULT_LOCATION_BUTTON:
        button_data->label = gtk_label_new (NULL);
        child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (child), button_data->label, FALSE, FALSE, 0);

        break;
    case NORMAL_BUTTON:
    default:
        button_data->label = gtk_label_new (NULL);
        child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (child), button_data->label, FALSE, FALSE, 0);
        button_data->is_base_dir = base_dir;
    }

    if (button_data->path == NULL)
    {
        button_data->path = g_object_ref (path);
    }
    if (button_data->dir_name == NULL)
    {
        button_data->dir_name = caja_file_get_display_name (file);
    }
    if (button_data->file == NULL)
    {
        button_data->file = caja_file_ref (file);
        caja_file_monitor_add (button_data->file, button_data,
                               CAJA_FILE_ATTRIBUTES_FOR_ICON);
        button_data->file_changed_signal_id =
            g_signal_connect (button_data->file, "changed",
                              G_CALLBACK (button_data_file_changed),
                              button_data);
    }

    button_data->file_is_hidden = file_is_hidden;

    gtk_container_add (GTK_CONTAINER (button_data->button), child);
    gtk_widget_show_all (button_data->button);

    caja_path_bar_update_button_state (button_data, current_dir);

    g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);
    g_signal_connect (button_data->button, "button-press-event", G_CALLBACK (button_event_cb), button_data);
    g_signal_connect (button_data->button, "button-release-event", G_CALLBACK (button_event_cb), button_data);
    g_signal_connect (button_data->button, "drag-begin", G_CALLBACK (button_drag_begin_cb), button_data);
    g_object_weak_ref (G_OBJECT (button_data->button), (GWeakNotify) button_data_free, button_data);

    setup_button_drag_source (button_data);

    caja_drag_slot_proxy_init (button_data->button,
                               &(button_data->drag_info));

    g_object_unref (path);

    return button_data;
}

static gboolean
caja_path_bar_check_parent_path (CajaPathBar *path_bar,
                                 GFile *location,
                                 ButtonData **current_button_data)
{
    GList *list;
    GList *current_path;
    gboolean need_new_fake_root;

    current_path = NULL;
    need_new_fake_root = FALSE;

    if (current_button_data)
    {
        *current_button_data = NULL;
    }

    for (list = path_bar->button_list; list; list = list->next)
    {
        ButtonData *button_data;

        button_data = list->data;
        if (g_file_equal (location, button_data->path))
        {
            current_path = list;

            if (current_button_data)
            {
                *current_button_data = button_data;
            }
            break;
        }
        if (list == path_bar->fake_root)
        {
            need_new_fake_root = TRUE;
        }
    }

    if (current_path)
    {

        if (need_new_fake_root)
        {
            path_bar->fake_root = NULL;
            for (list = current_path; list; list = list->next)
            {
                ButtonData *button_data;

                button_data = list->data;
                if (list->prev != NULL &&
                        button_data->fake_root)
                {
                    path_bar->fake_root = list;
                    break;
                }
            }
        }

        for (list = path_bar->button_list; list; list = list->next)
        {

            caja_path_bar_update_button_state (BUTTON_DATA (list->data),
                                               (list == current_path) ? TRUE : FALSE);
        }

        if (!gtk_widget_get_child_visible (BUTTON_DATA (current_path->data)->button))
        {
            path_bar->first_scrolled_button = current_path;
            gtk_widget_queue_resize (GTK_WIDGET (path_bar));
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean
caja_path_bar_update_path (CajaPathBar *path_bar,
                           GFile *file_path,
                           gboolean emit_signal)
{
    CajaFile *file, *parent_file;
    gboolean first_directory, last_directory;
    gboolean result;
    GList *new_buttons, *l, *fake_root;
    ButtonData *button_data, *current_button_data;

    g_return_val_if_fail (CAJA_IS_PATH_BAR (path_bar), FALSE);
    g_return_val_if_fail (file_path != NULL, FALSE);

    fake_root = NULL;
    result = TRUE;
    first_directory = TRUE;
    new_buttons = NULL;
    current_button_data = NULL;

    file = caja_file_get (file_path);

    while (file != NULL)
    {
        parent_file = caja_file_get_parent (file);
        last_directory = !parent_file;
        button_data = make_directory_button (path_bar, file, first_directory, last_directory, FALSE);
        caja_file_unref (file);

        if (first_directory)
        {
            current_button_data = button_data;
        }

        new_buttons = g_list_prepend (new_buttons, button_data);

        if (parent_file != NULL &&
                button_data->fake_root)
        {
            fake_root = new_buttons;
        }

        file = parent_file;
        first_directory = FALSE;
    }

    caja_path_bar_clear_buttons (path_bar);
    path_bar->button_list = g_list_reverse (new_buttons);
    path_bar->fake_root = fake_root;

    for (l = path_bar->button_list; l; l = l->next)
    {
        GtkWidget *button;
        button = BUTTON_DATA (l->data)->button;
        gtk_container_add (GTK_CONTAINER (path_bar), button);
    }

    if (path_bar->current_path != NULL)
    {
        g_object_unref (path_bar->current_path);
    }

    path_bar->current_path = g_object_ref (file_path);

    path_bar->current_button_data = current_button_data;

    return result;
}

gboolean
caja_path_bar_set_path (CajaPathBar *path_bar, GFile *file_path)
{
    ButtonData *button_data;

    g_return_val_if_fail (CAJA_IS_PATH_BAR (path_bar), FALSE);
    g_return_val_if_fail (file_path != NULL, FALSE);

    /* Check whether the new path is already present in the pathbar as buttons.
     * This could be a parent directory or a previous selected subdirectory. */
    if (caja_path_bar_check_parent_path (path_bar, file_path, &button_data))
    {
        if (path_bar->current_path != NULL)
        {
            g_object_unref (path_bar->current_path);
        }

        path_bar->current_path = g_object_ref (file_path);
        path_bar->current_button_data = button_data;

        return TRUE;
    }

    return caja_path_bar_update_path (path_bar, file_path, TRUE);
}

GFile *
caja_path_bar_get_path_for_button (CajaPathBar *path_bar,
                                   GtkWidget       *button)
{
    GList *list;

    g_return_val_if_fail (CAJA_IS_PATH_BAR (path_bar), NULL);
    g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

    for (list = path_bar->button_list; list; list = list->next)
    {
        ButtonData *button_data;
        button_data = BUTTON_DATA (list->data);
        if (button_data->button == button)
        {
            return g_object_ref (button_data->path);
        }
    }

    return NULL;
}

GtkWidget *
caja_path_bar_get_button_from_button_list_entry (gpointer entry)
{
    return BUTTON_DATA(entry)->button;
}
