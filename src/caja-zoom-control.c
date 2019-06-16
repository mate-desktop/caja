/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 *
 * This is the zoom control for the location bar
 *
 */

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <atk/atkaction.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>
#include <gdk/gdkkeysyms.h>

#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gtk-extensions.h>

#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-global-preferences.h>

#include "caja-zoom-control.h"

enum
{
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_TO_LEVEL,
    ZOOM_TO_DEFAULT,
    CHANGE_VALUE,
    LAST_SIGNAL
};

struct _CajaZoomControlPrivate
{
    GtkWidget *zoom_in;
    GtkWidget *zoom_out;
    GtkWidget *zoom_label;
    GtkWidget *zoom_button;

    CajaZoomLevel zoom_level;
    CajaZoomLevel min_zoom_level;
    CajaZoomLevel max_zoom_level;
    gboolean has_min_zoom_level;
    gboolean has_max_zoom_level;
    GList *preferred_zoom_levels;

    gboolean marking_menu_items;
};


static guint signals[LAST_SIGNAL] = { 0 };

static gpointer accessible_parent_class;

static const char * const caja_zoom_control_accessible_action_names[] =
{
    N_("Zoom In"),
    N_("Zoom Out"),
    N_("Zoom to Default"),
};

static const int caja_zoom_control_accessible_action_signals[] =
{
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_TO_DEFAULT,
};

static const char * const caja_zoom_control_accessible_action_descriptions[] =
{
    N_("Increase the view size"),
    N_("Decrease the view size"),
    N_("Use the normal view size")
};

static GtkMenu *create_zoom_menu (CajaZoomControl *zoom_control);

static GType caja_zoom_control_accessible_get_type (void);

/* button assignments */
#define CONTEXTUAL_MENU_BUTTON 3

#define NUM_ACTIONS ((int)G_N_ELEMENTS (caja_zoom_control_accessible_action_names))

G_DEFINE_TYPE_WITH_PRIVATE (CajaZoomControl, caja_zoom_control, GTK_TYPE_BOX);

static void
caja_zoom_control_finalize (GObject *object)
{
    g_list_free (CAJA_ZOOM_CONTROL (object)->details->preferred_zoom_levels);

    G_OBJECT_CLASS (caja_zoom_control_parent_class)->finalize (object);
}

static void
zoom_button_clicked (GtkButton *button, CajaZoomControl *zoom_control)
{
    g_signal_emit (zoom_control, signals[ZOOM_TO_DEFAULT], 0);
}

static void
zoom_popup_menu_show (GtkWidget *widget, GdkEventButton *event, CajaZoomControl *zoom_control)
{
    GtkMenu *menu;

    menu = create_zoom_menu (zoom_control);
    gtk_menu_popup_at_widget (menu,
                              widget,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              (const GdkEvent*) event);
}

static void
zoom_popup_menu (GtkWidget *widget, CajaZoomControl *zoom_control)
{
    GtkMenu *menu;

    menu = create_zoom_menu (zoom_control);
    gtk_menu_popup_at_widget (menu,
                              widget,
                              GDK_GRAVITY_SOUTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              NULL);

    gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
}

/* handle button presses */
static gboolean
caja_zoom_control_button_press_event (GtkWidget *widget,
                                      GdkEventButton *event,
                                      CajaZoomControl *zoom_control)
{
    if (event->type != GDK_BUTTON_PRESS)
    {
        return FALSE;
    }

    /* check for the context menu button and show the menu */
    if (event->button == CONTEXTUAL_MENU_BUTTON)
    {
        zoom_popup_menu_show (widget, event, zoom_control);
        return TRUE;
    }
    /* We don't change our state (to reflect the new zoom) here.
       The zoomable will call back with the new level.
       Actually, the callback goes to the viewframe containing the
       zoomable which, in turn, emits zoom_level_changed,
       which someone (e.g. caja_window) picks up and handles by
       calling into is - caja_zoom_control_set_zoom_level.
    */

    return FALSE;
}

static void
zoom_out_clicked (GtkButton *button,
                  CajaZoomControl *zoom_control)
{
    if (caja_zoom_control_can_zoom_out (zoom_control))
    {
        g_signal_emit (G_OBJECT (zoom_control), signals[ZOOM_OUT], 0);
    }
}

static void
zoom_in_clicked (GtkButton *button,
                 CajaZoomControl *zoom_control)
{
    if (caja_zoom_control_can_zoom_in (zoom_control))
    {
        g_signal_emit (G_OBJECT (zoom_control), signals[ZOOM_IN], 0);
    }
}

static void
set_label_size (CajaZoomControl *zoom_control)
{
    const char *text;
    PangoLayout *layout;
    int width;
    int height;

    text = gtk_label_get_text (GTK_LABEL (zoom_control->details->zoom_label));
    layout = gtk_label_get_layout (GTK_LABEL (zoom_control->details->zoom_label));
    pango_layout_set_text (layout, "100%", -1);
    pango_layout_get_pixel_size (layout, &width, &height);
    gtk_widget_set_size_request (zoom_control->details->zoom_label, width, height);
    gtk_label_set_text (GTK_LABEL (zoom_control->details->zoom_label),
                        text);
}

static void
label_style_set_callback (GtkWidget *label,
                          GtkStyleContext *style,
                          gpointer user_data)
{
    set_label_size (CAJA_ZOOM_CONTROL (user_data));
}

static void
caja_zoom_control_init (CajaZoomControl *zoom_control)
{
    GtkWidget *image;
    int i;

    zoom_control->details = caja_zoom_control_get_instance_private (zoom_control);

    zoom_control->details->zoom_level = CAJA_ZOOM_LEVEL_STANDARD;
    zoom_control->details->min_zoom_level = CAJA_ZOOM_LEVEL_SMALLEST;
    zoom_control->details->max_zoom_level = CAJA_ZOOM_LEVEL_LARGEST;
    zoom_control->details->has_min_zoom_level = TRUE;
    zoom_control->details->has_max_zoom_level = TRUE;

    for (i = CAJA_ZOOM_LEVEL_LARGEST; i >= CAJA_ZOOM_LEVEL_SMALLEST; i--)
    {
        zoom_control->details->preferred_zoom_levels = g_list_prepend (
                    zoom_control->details->preferred_zoom_levels,
                    GINT_TO_POINTER (i));
    }

    image = gtk_image_new_from_icon_name ("zoom-out", GTK_ICON_SIZE_MENU);
    zoom_control->details->zoom_out = gtk_button_new ();
    gtk_widget_set_focus_on_click (zoom_control->details->zoom_out, FALSE);
    gtk_button_set_relief (GTK_BUTTON (zoom_control->details->zoom_out),
                           GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (zoom_control->details->zoom_out,
                                 _("Decrease the view size"));
    g_signal_connect (G_OBJECT (zoom_control->details->zoom_out),
                      "clicked", G_CALLBACK (zoom_out_clicked),
                      zoom_control);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (zoom_control), GTK_ORIENTATION_HORIZONTAL);

    gtk_container_add (GTK_CONTAINER (zoom_control->details->zoom_out), image);
    gtk_box_pack_start (GTK_BOX (zoom_control),
                        zoom_control->details->zoom_out, FALSE, FALSE, 0);

    zoom_control->details->zoom_button = gtk_button_new ();
    gtk_widget_set_focus_on_click (zoom_control->details->zoom_button, FALSE);
    gtk_button_set_relief (GTK_BUTTON (zoom_control->details->zoom_button),
                           GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (zoom_control->details->zoom_button,
                                 _("Use the normal view size"));

    gtk_widget_add_events (GTK_WIDGET (zoom_control->details->zoom_button),
                           GDK_BUTTON_PRESS_MASK
                           | GDK_BUTTON_RELEASE_MASK
                           | GDK_POINTER_MOTION_MASK
                           | GDK_SCROLL_MASK);

    g_signal_connect (G_OBJECT (zoom_control->details->zoom_button),
                      "button-press-event",
                      G_CALLBACK (caja_zoom_control_button_press_event),
                      zoom_control);

    g_signal_connect (G_OBJECT (zoom_control->details->zoom_button),
                      "clicked", G_CALLBACK (zoom_button_clicked),
                      zoom_control);

    g_signal_connect (G_OBJECT (zoom_control->details->zoom_button),
                      "popup-menu", G_CALLBACK (zoom_popup_menu),
                      zoom_control);

    zoom_control->details->zoom_label = gtk_label_new ("100%");
    g_signal_connect (zoom_control->details->zoom_label,
                      "style_set",
                      G_CALLBACK (label_style_set_callback),
                      zoom_control);
    set_label_size (zoom_control);

    gtk_container_add (GTK_CONTAINER (zoom_control->details->zoom_button), zoom_control->details->zoom_label);

    gtk_box_pack_start (GTK_BOX (zoom_control),
                        zoom_control->details->zoom_button, TRUE, TRUE, 0);

    image = gtk_image_new_from_icon_name ("zoom-in", GTK_ICON_SIZE_MENU);
    zoom_control->details->zoom_in = gtk_button_new ();
    gtk_widget_set_focus_on_click (zoom_control->details->zoom_in, FALSE);
    gtk_button_set_relief (GTK_BUTTON (zoom_control->details->zoom_in),
                           GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text (zoom_control->details->zoom_in,
                                 _("Increase the view size"));
    g_signal_connect (G_OBJECT (zoom_control->details->zoom_in),
                      "clicked", G_CALLBACK (zoom_in_clicked),
                      zoom_control);

    gtk_container_add (GTK_CONTAINER (zoom_control->details->zoom_in), image);
    gtk_box_pack_start (GTK_BOX (zoom_control),
                        zoom_control->details->zoom_in, FALSE, FALSE, 0);

    gtk_widget_show_all (zoom_control->details->zoom_out);
    gtk_widget_show_all (zoom_control->details->zoom_button);
    gtk_widget_show_all (zoom_control->details->zoom_in);
}

/* Allocate a new zoom control */
GtkWidget *
caja_zoom_control_new (void)
{
    return gtk_widget_new (caja_zoom_control_get_type (), NULL);
}

static void
caja_zoom_control_redraw (CajaZoomControl *zoom_control)
{
    int percent;
    char *num_str;

    gtk_widget_set_sensitive (zoom_control->details->zoom_in,
                              caja_zoom_control_can_zoom_in (zoom_control));
    gtk_widget_set_sensitive (zoom_control->details->zoom_out,
                              caja_zoom_control_can_zoom_out (zoom_control));

    percent = floor ((100.0 * caja_get_relative_icon_size_for_zoom_level (zoom_control->details->zoom_level)) + .2);
    num_str = g_strdup_printf ("%d%%", percent);
    gtk_label_set_text (GTK_LABEL (zoom_control->details->zoom_label), num_str);
    g_free (num_str);
}

/* routines to create and handle the zoom menu */

static void
zoom_menu_callback (GtkMenuItem *item, gpointer callback_data)
{
    CajaZoomLevel zoom_level;
    CajaZoomControl *zoom_control;
    gboolean can_zoom;

    zoom_control = CAJA_ZOOM_CONTROL (callback_data);

    /* Don't do anything if we're just setting the toggle state of menu items. */
    if (zoom_control->details->marking_menu_items)
    {
        return;
    }

    /* Don't send the signal if the menuitem was toggled off */
    if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
    {
        return;
    }

    zoom_level = (CajaZoomLevel) GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "zoom_level"));

    /* Assume we can zoom and then check whether we're right. */
    can_zoom = TRUE;
    if (zoom_control->details->has_min_zoom_level &&
            zoom_level < zoom_control->details->min_zoom_level)
        can_zoom = FALSE; /* no, we're below the minimum zoom level. */
    if (zoom_control->details->has_max_zoom_level &&
            zoom_level > zoom_control->details->max_zoom_level)
        can_zoom = FALSE; /* no, we're beyond the upper zoom level. */

    /* if we can zoom */
    if (can_zoom)
    {
        g_signal_emit (zoom_control, signals[ZOOM_TO_LEVEL], 0, zoom_level);
    }
}

static GtkRadioMenuItem *
create_zoom_menu_item (CajaZoomControl *zoom_control, GtkMenu *menu,
                       CajaZoomLevel zoom_level,
                       GtkRadioMenuItem *previous_radio_item)
{
    GtkWidget *menu_item;
    char *item_text;
    GSList *radio_item_group;
    int percent;

    /* Set flag so that callback isn't activated when set_active called
     * to set toggle state of other radio items.
     */
    zoom_control->details->marking_menu_items = TRUE;

    percent = floor ((100.0 * caja_get_relative_icon_size_for_zoom_level (zoom_level)) + .5);
    item_text = g_strdup_printf ("%d%%", percent);

    radio_item_group = previous_radio_item == NULL
                       ? NULL
                       : gtk_radio_menu_item_get_group (previous_radio_item);
    menu_item = gtk_radio_menu_item_new_with_label (radio_item_group, item_text);
    g_free (item_text);

    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                    zoom_level == zoom_control->details->zoom_level);

    g_object_set_data (G_OBJECT (menu_item), "zoom_level", GINT_TO_POINTER (zoom_level));
    g_signal_connect_object (menu_item, "activate",
                             G_CALLBACK (zoom_menu_callback), zoom_control, 0);

    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    zoom_control->details->marking_menu_items = FALSE;

    return GTK_RADIO_MENU_ITEM (menu_item);
}

static GtkMenu *
create_zoom_menu (CajaZoomControl *zoom_control)
{
    GtkMenu *menu;
    GtkRadioMenuItem *previous_item;
    GList *node;

    menu = GTK_MENU (gtk_menu_new ());

    previous_item = NULL;
    for (node = zoom_control->details->preferred_zoom_levels; node != NULL; node = node->next)
    {
        previous_item = create_zoom_menu_item
                        (zoom_control, menu, GPOINTER_TO_INT (node->data), previous_item);
    }

    return menu;
}

static void
caja_zoom_control_change_value (CajaZoomControl *zoom_control,
                                GtkScrollType scroll)
{
    switch (scroll)
    {
    case GTK_SCROLL_STEP_DOWN :
        if (caja_zoom_control_can_zoom_out (zoom_control))
        {
            g_signal_emit (zoom_control, signals[ZOOM_OUT], 0);
        }
        break;
    case GTK_SCROLL_STEP_UP :
        if (caja_zoom_control_can_zoom_in (zoom_control))
        {
            g_signal_emit (zoom_control, signals[ZOOM_IN], 0);
        }
        break;
    default :
        g_warning ("Invalid scroll type %d for CajaZoomControl:change_value", scroll);
    }
}

void
caja_zoom_control_set_zoom_level (CajaZoomControl *zoom_control,
                                  CajaZoomLevel zoom_level)
{
    zoom_control->details->zoom_level = zoom_level;
    caja_zoom_control_redraw (zoom_control);
}

void
caja_zoom_control_set_parameters (CajaZoomControl *zoom_control,
                                  CajaZoomLevel min_zoom_level,
                                  CajaZoomLevel max_zoom_level,
                                  gboolean has_min_zoom_level,
                                  gboolean has_max_zoom_level,
                                  GList *zoom_levels)
{
    g_return_if_fail (CAJA_IS_ZOOM_CONTROL (zoom_control));

    zoom_control->details->min_zoom_level = min_zoom_level;
    zoom_control->details->max_zoom_level = max_zoom_level;
    zoom_control->details->has_min_zoom_level = has_min_zoom_level;
    zoom_control->details->has_max_zoom_level = has_max_zoom_level;

    g_list_free (zoom_control->details->preferred_zoom_levels);
    zoom_control->details->preferred_zoom_levels = zoom_levels;

    caja_zoom_control_redraw (zoom_control);
}

CajaZoomLevel
caja_zoom_control_get_zoom_level (CajaZoomControl *zoom_control)
{
    return zoom_control->details->zoom_level;
}

CajaZoomLevel
caja_zoom_control_get_min_zoom_level (CajaZoomControl *zoom_control)
{
    return zoom_control->details->min_zoom_level;
}

CajaZoomLevel
caja_zoom_control_get_max_zoom_level (CajaZoomControl *zoom_control)
{
    return zoom_control->details->max_zoom_level;
}

gboolean
caja_zoom_control_has_min_zoom_level (CajaZoomControl *zoom_control)
{
    return zoom_control->details->has_min_zoom_level;
}

gboolean
caja_zoom_control_has_max_zoom_level (CajaZoomControl *zoom_control)
{
    return zoom_control->details->has_max_zoom_level;
}

gboolean
caja_zoom_control_can_zoom_in (CajaZoomControl *zoom_control)
{
    return !zoom_control->details->has_max_zoom_level ||
           (zoom_control->details->zoom_level
            < zoom_control->details->max_zoom_level);
}

gboolean
caja_zoom_control_can_zoom_out (CajaZoomControl *zoom_control)
{
    return !zoom_control->details->has_min_zoom_level ||
           (zoom_control->details->zoom_level
            > zoom_control->details->min_zoom_level);
}

static gboolean
caja_zoom_control_scroll_event (GtkWidget *widget, GdkEventScroll *event)
{
    CajaZoomControl *zoom_control;

    zoom_control = CAJA_ZOOM_CONTROL (widget);

    if (event->type != GDK_SCROLL)
    {
        return FALSE;
    }

    if (event->direction == GDK_SCROLL_DOWN &&
            caja_zoom_control_can_zoom_out (zoom_control))
    {
        g_signal_emit (widget, signals[ZOOM_OUT], 0);
    }
    else if (event->direction == GDK_SCROLL_UP &&
             caja_zoom_control_can_zoom_in (zoom_control))
    {
        g_signal_emit (widget, signals[ZOOM_IN], 0);
    }

    /* We don't change our state (to reflect the new zoom) here. The zoomable will
     * call back with the new level. Actually, the callback goes to the view-frame
     * containing the zoomable which, in turn, emits zoom_level_changed, which
     * someone (e.g. caja_window) picks up and handles by calling into us -
     * caja_zoom_control_set_zoom_level.
     */
    return TRUE;
}



static void
caja_zoom_control_class_init (CajaZoomControlClass *class)
{
    GtkWidgetClass *widget_class;
    GtkBindingSet *binding_set;

    G_OBJECT_CLASS (class)->finalize = caja_zoom_control_finalize;

    widget_class = GTK_WIDGET_CLASS (class);


    gtk_widget_class_set_accessible_type (widget_class,
                                          caja_zoom_control_accessible_get_type ());

    widget_class->scroll_event = caja_zoom_control_scroll_event;

    class->change_value = caja_zoom_control_change_value;

    signals[ZOOM_IN] =
        g_signal_new ("zoom_in",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaZoomControlClass,
                                       zoom_in),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[ZOOM_OUT] =
        g_signal_new ("zoom_out",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaZoomControlClass,
                                       zoom_out),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[ZOOM_TO_LEVEL] =
        g_signal_new ("zoom_to_level",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaZoomControlClass,
                                       zoom_to_level),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__INT,
                      G_TYPE_NONE,
                      1,
                      G_TYPE_INT);

    signals[ZOOM_TO_DEFAULT] =
        g_signal_new ("zoom_to_default",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (CajaZoomControlClass,
                                       zoom_to_default),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[CHANGE_VALUE] =
        g_signal_new ("change_value",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET (CajaZoomControlClass,
                                       change_value),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__ENUM,
                      G_TYPE_NONE, 1, GTK_TYPE_SCROLL_TYPE);

    binding_set = gtk_binding_set_by_class (class);

    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_KP_Subtract, 0,
                                  "change_value",
                                  1, GTK_TYPE_SCROLL_TYPE,
                                  GTK_SCROLL_STEP_DOWN);
    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_minus, 0,
                                  "change_value",
                                  1, GTK_TYPE_SCROLL_TYPE,
                                  GTK_SCROLL_STEP_DOWN);

    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_KP_Equal, 0,
                                  "zoom_to_default",
                                  0);
    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_KP_Equal, 0,
                                  "zoom_to_default",
                                  0);

    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_KP_Add, 0,
                                  "change_value",
                                  1, GTK_TYPE_SCROLL_TYPE,
                                  GTK_SCROLL_STEP_UP);
    gtk_binding_entry_add_signal (binding_set,
				      GDK_KEY_plus, 0,
                                  "change_value",
                                  1, GTK_TYPE_SCROLL_TYPE,
                                  GTK_SCROLL_STEP_UP);
}

static gboolean
caja_zoom_control_accessible_do_action (AtkAction *accessible, int i)
{
    GtkWidget *widget;

    g_assert (i >= 0 && i < NUM_ACTIONS);

    widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
    if (!widget)
    {
        return FALSE;
    }

    g_signal_emit (widget,
                   signals[caja_zoom_control_accessible_action_signals [i]],
                   0);

    return TRUE;
}

static int
caja_zoom_control_accessible_get_n_actions (AtkAction *accessible)
{

    return NUM_ACTIONS;
}

static const char* caja_zoom_control_accessible_action_get_description(AtkAction* accessible, int i)
{
    g_assert(i >= 0 && i < NUM_ACTIONS);

    return _(caja_zoom_control_accessible_action_descriptions[i]);
}

static const char* caja_zoom_control_accessible_action_get_name(AtkAction* accessible, int i)
{
    g_assert (i >= 0 && i < NUM_ACTIONS);

    return _(caja_zoom_control_accessible_action_names[i]);
}

static void caja_zoom_control_accessible_action_interface_init(AtkActionIface* iface)
{
    iface->do_action = caja_zoom_control_accessible_do_action;
    iface->get_n_actions = caja_zoom_control_accessible_get_n_actions;
    iface->get_description = caja_zoom_control_accessible_action_get_description;
    iface->get_name = caja_zoom_control_accessible_action_get_name;
}

static void
caja_zoom_control_accessible_get_current_value (AtkValue *accessible,
        GValue *value)
{
    CajaZoomControl *control;

    g_value_init (value, G_TYPE_INT);

    control = CAJA_ZOOM_CONTROL (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
    if (!control)
    {
        g_value_set_int (value, CAJA_ZOOM_LEVEL_STANDARD);
        return;
    }

    g_value_set_int (value, control->details->zoom_level);
}

static void
caja_zoom_control_accessible_get_maximum_value (AtkValue *accessible,
        GValue *value)
{
    CajaZoomControl *control;

    g_value_init (value, G_TYPE_INT);

    control = CAJA_ZOOM_CONTROL (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
    if (!control)
    {
        g_value_set_int (value, CAJA_ZOOM_LEVEL_STANDARD);
        return;
    }

    g_value_set_int (value, control->details->max_zoom_level);
}

static void
caja_zoom_control_accessible_get_minimum_value (AtkValue *accessible,
        GValue *value)
{
    CajaZoomControl *control;

    g_value_init (value, G_TYPE_INT);

    control = CAJA_ZOOM_CONTROL (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
    if (!control)
    {
        g_value_set_int (value, CAJA_ZOOM_LEVEL_STANDARD);
        return;
    }

    g_value_set_int (value, control->details->min_zoom_level);
}

static CajaZoomLevel
nearest_preferred (CajaZoomControl *zoom_control, CajaZoomLevel value)
{
    CajaZoomLevel last_value;
    CajaZoomLevel current_value;
    GList *l;

    if (!zoom_control->details->preferred_zoom_levels)
    {
        return value;
    }

    last_value = GPOINTER_TO_INT (zoom_control->details->preferred_zoom_levels->data);
    current_value = last_value;

    for (l = zoom_control->details->preferred_zoom_levels; l != NULL; l = l->next)
    {
        current_value = GPOINTER_TO_INT (l->data);

        if (current_value > value)
        {
            float center = (last_value + current_value) / 2;

            return (value < center) ? last_value : current_value;

        }

        last_value = current_value;
    }

    return current_value;
}

static gboolean
caja_zoom_control_accessible_set_current_value (AtkValue *accessible,
        const GValue *value)
{
    CajaZoomControl *control;
    CajaZoomLevel zoom;

    control = CAJA_ZOOM_CONTROL (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
    if (!control)
    {
        return FALSE;
    }

    zoom = nearest_preferred (control, g_value_get_int (value));

    g_signal_emit (control, signals[ZOOM_TO_LEVEL], 0, zoom);

    return TRUE;
}

static void
caja_zoom_control_accessible_value_interface_init (AtkValueIface *iface)
{
    iface->get_current_value = caja_zoom_control_accessible_get_current_value;
    iface->get_maximum_value = caja_zoom_control_accessible_get_maximum_value;
    iface->get_minimum_value = caja_zoom_control_accessible_get_minimum_value;
    iface->set_current_value = caja_zoom_control_accessible_set_current_value;
}

static const char* caja_zoom_control_accessible_get_name(AtkObject* accessible)
{
    return _("Zoom");
}

static const char* caja_zoom_control_accessible_get_description(AtkObject* accessible)
{
    return _("Set the zoom level of the current view");
}

static void
caja_zoom_control_accessible_initialize (AtkObject *accessible,
        gpointer  data)
{
    if (ATK_OBJECT_CLASS (accessible_parent_class)->initialize != NULL)
    {
        ATK_OBJECT_CLASS (accessible_parent_class)->initialize (accessible, data);
    }
    atk_object_set_role (accessible, ATK_ROLE_DIAL);
}

typedef struct _CajaZoomControlAccessible CajaZoomControlAccessible;
typedef struct _CajaZoomControlAccessibleClass CajaZoomControlAccessibleClass;

struct _CajaZoomControlAccessible
{
    GtkContainerAccessible parent;
};

struct _CajaZoomControlAccessibleClass
{
    GtkContainerAccessibleClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (CajaZoomControlAccessible,
                         caja_zoom_control_accessible,
                         GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION,
                                                caja_zoom_control_accessible_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE,
                                                caja_zoom_control_accessible_value_interface_init));
static void
caja_zoom_control_accessible_class_init (CajaZoomControlAccessibleClass *klass)
{
    AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);
    accessible_parent_class = g_type_class_peek_parent (klass);

    atk_class->get_name = caja_zoom_control_accessible_get_name;
    atk_class->get_description = caja_zoom_control_accessible_get_description;
    atk_class->initialize = caja_zoom_control_accessible_initialize;
}

static void
caja_zoom_control_accessible_init (CajaZoomControlAccessible *accessible)
{
}

void
caja_zoom_control_set_active_appearance (CajaZoomControl *zoom_control, gboolean is_active)
{
    gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (zoom_control->details->zoom_in)), is_active);
    gtk_widget_set_sensitive (gtk_bin_get_child (GTK_BIN (zoom_control->details->zoom_out)), is_active);
    gtk_widget_set_sensitive (zoom_control->details->zoom_label, is_active);
}
