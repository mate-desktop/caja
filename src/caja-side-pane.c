/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*  caja-side-pane.c
 *
 *  Copyright (C) 2002 Ximian Inc.
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
 *  Author: Dave Camp <dave@ximian.com>
 */

#include <config.h>
#include "caja-side-pane.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if !GTK_CHECK_VERSION(3,0,0)
#define gtk_widget_get_preferred_size(x,y,z) gtk_widget_size_request(x,y)
#endif

typedef struct
{
    char *title;
    char *tooltip;
    GtkWidget *widget;
    GtkWidget *menu_item;
    GtkWidget *shortcut;
} SidePanel;

struct _CajaSidePaneDetails
{
    GtkWidget *notebook;
    GtkWidget *menu;

    GtkWidget *title_frame;
    GtkWidget *title_hbox;
    GtkWidget *title_label;
    GtkWidget *shortcut_box;
    GList *panels;
};

static void caja_side_pane_class_init (CajaSidePaneClass *klass);
static void caja_side_pane_init       (GObject *object);
static void caja_side_pane_dispose    (GObject *object);
static void caja_side_pane_finalize   (GObject *object);

enum
{
    CLOSE_REQUESTED,
    SWITCH_PAGE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

EEL_CLASS_BOILERPLATE (CajaSidePane, caja_side_pane, GTK_TYPE_VBOX)

static SidePanel *
panel_for_widget (CajaSidePane *side_pane, GtkWidget *widget)
{
    GList *l;
    SidePanel *panel;

    for (l = side_pane->details->panels; l != NULL; l = l->next)
    {
        panel = l->data;
        if (panel->widget == widget)
        {
            return panel;
        }
    }

    return NULL;
}

static void
side_panel_free (SidePanel *panel)
{
    g_free (panel->title);
    g_free (panel->tooltip);
    g_slice_free (SidePanel, panel);
}

static void
switch_page_callback (GtkWidget *notebook,
                      GtkWidget *page,
                      guint page_num,
                      gpointer user_data)
{
    CajaSidePane *side_pane;
    SidePanel *panel;

    side_pane = CAJA_SIDE_PANE (user_data);

    panel = panel_for_widget (side_pane,
                              gtk_notebook_get_nth_page (GTK_NOTEBOOK (side_pane->details->notebook),
                                      page_num));

    if (panel && side_pane->details->title_label)
    {
        gtk_label_set_text (GTK_LABEL (side_pane->details->title_label),
                            panel->title);
    }

    g_signal_emit (side_pane, signals[SWITCH_PAGE], 0,
                   panel ? panel->widget : NULL);
}

static void
select_panel (CajaSidePane *side_pane, SidePanel *panel)
{
    int page_num;

    page_num = gtk_notebook_page_num
               (GTK_NOTEBOOK (side_pane->details->notebook), panel->widget);
    gtk_notebook_set_current_page
    (GTK_NOTEBOOK (side_pane->details->notebook), page_num);
}

static void
caja_side_pane_size_allocate (GtkWidget *widget,
                              GtkAllocation *allocation)
{
    int width;
    GtkAllocation child_allocation, frame_allocation;
    CajaSidePane *pane;
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkRequisition child_requisition;

    GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

    pane = CAJA_SIDE_PANE(widget);
    frame = pane->details->title_frame;
    hbox = pane->details->title_hbox;

    gtk_widget_get_preferred_size (hbox, &child_requisition, NULL);
    width = child_requisition.width;

    gtk_widget_get_allocation (frame, &frame_allocation);
    child_allocation = frame_allocation;
    child_allocation.width = MAX (width, frame_allocation.width);

    gtk_widget_size_allocate (frame, &child_allocation);
}

/* initializing the class object by installing the operations we override */
static void
caja_side_pane_class_init (CajaSidePaneClass *klass)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS (klass);
    widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = caja_side_pane_finalize;
    gobject_class->dispose = caja_side_pane_dispose;
    widget_class->size_allocate = caja_side_pane_size_allocate;

    signals[CLOSE_REQUESTED] = g_signal_new
                               ("close_requested",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_STRUCT_OFFSET (CajaSidePaneClass,
                                        close_requested),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
    signals[SWITCH_PAGE] = g_signal_new
                           ("switch_page",
                            G_TYPE_FROM_CLASS (klass),
                            G_SIGNAL_RUN_LAST,
                            G_STRUCT_OFFSET (CajaSidePaneClass,
                                    switch_page),
                            NULL, NULL,
                            g_cclosure_marshal_VOID__OBJECT,
                            G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

    g_type_class_add_private (gobject_class, sizeof (CajaSidePaneDetails));
}

static void
panel_item_activate_callback (GtkMenuItem *item,
                              gpointer user_data)
{
    CajaSidePane *side_pane;
    SidePanel *panel;

    side_pane = CAJA_SIDE_PANE (user_data);

    panel = g_object_get_data (G_OBJECT (item), "panel-item");

    select_panel (side_pane, panel);
}


static void
menu_position_under (GtkMenu *menu,
                     int *x,
                     int *y,
                     gboolean *push_in,
                     gpointer user_data)
{
    GtkWidget *widget;
    GtkAllocation allocation;

    g_return_if_fail (GTK_IS_BUTTON (user_data));
    g_return_if_fail (!gtk_widget_get_has_window (GTK_WIDGET (user_data)));

    widget = GTK_WIDGET (user_data);

    gdk_window_get_origin (gtk_widget_get_window (widget), x, y);
    gtk_widget_get_allocation (widget, &allocation);

    *x += allocation.x;
    *y += allocation.y + allocation.height;

    *push_in = FALSE;
}

static gboolean
select_button_press_callback (GtkWidget *widget,
                              GdkEventButton *event,
                              gpointer user_data)
{
    CajaSidePane *side_pane;

    side_pane = CAJA_SIDE_PANE (user_data);

    if ((event->type == GDK_BUTTON_PRESS) && event->button == 1)
    {
        GtkRequisition requisition;
        GtkAllocation allocation;
        gint width;

        gtk_widget_get_allocation (widget, &allocation);
        width = allocation.width;
        gtk_widget_set_size_request (side_pane->details->menu, -1, -1);
        gtk_widget_get_preferred_size (side_pane->details->menu, &requisition, NULL);
        gtk_widget_set_size_request (side_pane->details->menu,
                                     MAX (width, requisition.width), -1);

        gtk_widget_grab_focus (widget);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        gtk_menu_popup (GTK_MENU (side_pane->details->menu),
                        NULL, NULL, menu_position_under, widget,
                        event->button, event->time);

        return TRUE;
    }
    return FALSE;
}

static gboolean
select_button_key_press_callback (GtkWidget *widget,
                                  GdkEventKey *event,
                                  gpointer user_data)
{
    CajaSidePane *side_pane;

    side_pane = CAJA_SIDE_PANE (user_data);

    if (event->keyval == GDK_KEY_space ||
        event->keyval == GDK_KEY_KP_Space ||
        event->keyval == GDK_KEY_Return ||
        event->keyval == GDK_KEY_KP_Enter)
    {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
        gtk_menu_popup (GTK_MENU (side_pane->details->menu),
                        NULL, NULL, menu_position_under, widget,
                        1, event->time);
        return TRUE;
    }

    return FALSE;
}

static void
close_clicked_callback (GtkWidget *widget,
                        gpointer user_data)
{
    CajaSidePane *side_pane;

    side_pane = CAJA_SIDE_PANE (user_data);

    g_signal_emit (side_pane, signals[CLOSE_REQUESTED], 0);
}

static void
menu_deactivate_callback (GtkWidget *widget,
                          gpointer user_data)
{
    GtkWidget *menu_button;

    menu_button = GTK_WIDGET (user_data);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (menu_button), FALSE);
}

static void
menu_detach_callback (GtkWidget *widget,
                      GtkMenu *menu)
{
    CajaSidePane *side_pane;

    side_pane = CAJA_SIDE_PANE (widget);

    side_pane->details->menu = NULL;
}

static void
caja_side_pane_init (GObject *object)
{
    CajaSidePane *side_pane;
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *close_button;
    GtkWidget *select_button;
    GtkWidget *select_hbox;
    GtkWidget *arrow;
    GtkWidget *image;

    side_pane = CAJA_SIDE_PANE (object);

    side_pane->details = G_TYPE_INSTANCE_GET_PRIVATE (object, CAJA_TYPE_SIDE_PANE, CajaSidePaneDetails);

    /* The frame (really a vbox) has the border */
    frame = gtk_vbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
    side_pane->details->title_frame = frame;
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (side_pane), frame, FALSE, FALSE, 0);

    /* And the title_hbox is what gets the same size as the other
       headers */
    hbox = gtk_hbox_new (FALSE, 0);
    side_pane->details->title_hbox = hbox;
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (frame), hbox);

    select_button = gtk_toggle_button_new ();
    gtk_button_set_relief (GTK_BUTTON (select_button), GTK_RELIEF_NONE);
    gtk_widget_show (select_button);

    g_signal_connect (select_button,
                      "button_press_event",
                      G_CALLBACK (select_button_press_callback),
                      side_pane);
    g_signal_connect (select_button,
                      "key_press_event",
                      G_CALLBACK (select_button_key_press_callback),
                      side_pane);

    select_hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (select_hbox);

    side_pane->details->title_label = gtk_label_new ("");
    eel_add_weak_pointer (&side_pane->details->title_label);

    gtk_widget_show (side_pane->details->title_label);
    gtk_box_pack_start (GTK_BOX (select_hbox),
                        side_pane->details->title_label,
                        FALSE, FALSE, 0);

    arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    gtk_widget_show (arrow);
    gtk_box_pack_end (GTK_BOX (select_hbox), arrow, FALSE, FALSE, 0);

    gtk_container_add (GTK_CONTAINER (select_button), select_hbox);
    gtk_box_pack_start (GTK_BOX (hbox), select_button, TRUE, TRUE, 0);

    close_button = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (close_button), GTK_RELIEF_NONE);
    g_signal_connect (close_button,
                      "clicked",
                      G_CALLBACK (close_clicked_callback),
                      side_pane);

    gtk_widget_show (close_button);

    image = gtk_image_new_from_stock (GTK_STOCK_CLOSE,
                                      GTK_ICON_SIZE_MENU);
    gtk_widget_show (image);

    gtk_container_add (GTK_CONTAINER (close_button), image);

    gtk_box_pack_end (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);

    side_pane->details->shortcut_box = gtk_hbox_new (TRUE, 0);
    gtk_widget_show (side_pane->details->shortcut_box);
    gtk_box_pack_end (GTK_BOX (hbox),
                      side_pane->details->shortcut_box,
                      FALSE, FALSE, 0);

    side_pane->details->notebook = gtk_notebook_new ();
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (side_pane->details->notebook),
                                FALSE);
    gtk_notebook_set_show_border (GTK_NOTEBOOK (side_pane->details->notebook),
                                  FALSE);
    g_signal_connect_object (side_pane->details->notebook,
                             "switch_page",
                             G_CALLBACK (switch_page_callback),
                             side_pane,
                             0);

    gtk_widget_show (side_pane->details->notebook);

    gtk_box_pack_start (GTK_BOX (side_pane), side_pane->details->notebook,
                        TRUE, TRUE, 0);

    side_pane->details->menu = gtk_menu_new ();
    g_signal_connect (side_pane->details->menu,
                      "deactivate",
                      G_CALLBACK (menu_deactivate_callback),
                      select_button);
    gtk_menu_attach_to_widget (GTK_MENU (side_pane->details->menu),
                               GTK_WIDGET (side_pane),
                               menu_detach_callback);

    gtk_widget_show (side_pane->details->menu);

    gtk_widget_set_tooltip_text (close_button,
                                 _("Close the side pane"));
}

static void
caja_side_pane_dispose (GObject *object)
{
    CajaSidePane *side_pane;

    side_pane = CAJA_SIDE_PANE (object);

    if (side_pane->details->menu)
    {
        gtk_menu_detach (GTK_MENU (side_pane->details->menu));
        side_pane->details->menu = NULL;
    }

    EEL_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
caja_side_pane_finalize (GObject *object)
{
    CajaSidePane *side_pane;
    GList *l;

    side_pane = CAJA_SIDE_PANE (object);

    for (l = side_pane->details->panels; l != NULL; l = l->next)
    {
        side_panel_free (l->data);
    }

    g_list_free (side_pane->details->panels);

    EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

CajaSidePane *
caja_side_pane_new (void)
{
    return CAJA_SIDE_PANE (gtk_widget_new (caja_side_pane_get_type (), NULL));
}

void
caja_side_pane_add_panel (CajaSidePane *side_pane,
                          GtkWidget *widget,
                          const char *title,
                          const char *tooltip)
{
    SidePanel *panel;

    g_return_if_fail (side_pane != NULL);
    g_return_if_fail (CAJA_IS_SIDE_PANE (side_pane));
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (title != NULL);
    g_return_if_fail (tooltip != NULL);

    panel = g_slice_new0 (SidePanel);
    panel->title = g_strdup (title);
    panel->tooltip = g_strdup (tooltip);
    panel->widget = widget;

    gtk_widget_show (widget);

    panel->menu_item = gtk_image_menu_item_new_with_label (title);
    gtk_widget_show (panel->menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (side_pane->details->menu),
                           panel->menu_item);
    g_object_set_data (G_OBJECT (panel->menu_item), "panel-item", panel);

    g_signal_connect (panel->menu_item,
                      "activate",
                      G_CALLBACK (panel_item_activate_callback),
                      side_pane);

    side_pane->details->panels = g_list_append (side_pane->details->panels,
                                 panel);

    gtk_notebook_append_page (GTK_NOTEBOOK (side_pane->details->notebook),
                              widget,
                              NULL);
}

void
caja_side_pane_remove_panel (CajaSidePane *side_pane,
                             GtkWidget *widget)
{
    SidePanel *panel;
    int page_num;

    g_return_if_fail (side_pane != NULL);
    g_return_if_fail (CAJA_IS_SIDE_PANE (side_pane));
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_WIDGET (widget));

    panel = panel_for_widget (side_pane, widget);

    g_return_if_fail (panel != NULL);

    if (panel)
    {
        page_num = gtk_notebook_page_num (GTK_NOTEBOOK (side_pane->details->notebook),
                                          widget);
        gtk_notebook_remove_page (GTK_NOTEBOOK (side_pane->details->notebook),
                                  page_num);
        gtk_container_remove (GTK_CONTAINER (side_pane->details->menu),
                              panel->menu_item);

        side_pane->details->panels =
            g_list_remove (side_pane->details->panels,
                           panel);

        side_panel_free (panel);
    }
}

void
caja_side_pane_show_panel (CajaSidePane *side_pane,
                           GtkWidget        *widget)
{
    SidePanel *panel;
    int page_num;

    g_return_if_fail (side_pane != NULL);
    g_return_if_fail (CAJA_IS_SIDE_PANE (side_pane));
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_WIDGET (widget));

    panel = panel_for_widget (side_pane, widget);

    g_return_if_fail (panel != NULL);

    page_num = gtk_notebook_page_num (GTK_NOTEBOOK (side_pane->details->notebook),
                                      widget);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (side_pane->details->notebook),
                                   page_num);
}


static void
shortcut_clicked_callback (GtkWidget *button,
                           gpointer user_data)
{
    CajaSidePane *side_pane;
    GtkWidget *page;

    side_pane = CAJA_SIDE_PANE (user_data);

    page = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "side-page"));

    caja_side_pane_show_panel (side_pane, page);
}

static GtkWidget *
create_shortcut (CajaSidePane *side_pane,
                 SidePanel *panel,
                 GdkPixbuf *pixbuf)
{
    GtkWidget *button;
    GtkWidget *image;

    button = gtk_button_new ();
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

    g_object_set_data (G_OBJECT (button), "side-page", panel->widget);
    g_signal_connect (button, "clicked",
                      G_CALLBACK (shortcut_clicked_callback), side_pane);

    gtk_widget_set_tooltip_text (button, panel->tooltip);

    image = gtk_image_new_from_pixbuf (pixbuf);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (button), image);

    return button;
}

void
caja_side_pane_set_panel_image (CajaSidePane *side_pane,
                                GtkWidget *widget,
                                GdkPixbuf *pixbuf)
{
    SidePanel *panel;
    GtkWidget *image;

    g_return_if_fail (side_pane != NULL);
    g_return_if_fail (CAJA_IS_SIDE_PANE (side_pane));
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_WIDGET (widget));
    g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

    panel = panel_for_widget (side_pane, widget);

    g_return_if_fail (panel != NULL);

    if (pixbuf)
    {
        image = gtk_image_new_from_pixbuf (pixbuf);
        gtk_widget_show (image);
    }
    else
    {
        image = NULL;
    }

    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (panel->menu_item),
                                   image);

    if (panel->shortcut)
    {
        gtk_widget_destroy (panel->shortcut);
        panel->shortcut = NULL;
    }

    if (pixbuf)
    {
        panel->shortcut = create_shortcut (side_pane, panel, pixbuf);
        gtk_widget_show (panel->shortcut);
        gtk_box_pack_start (GTK_BOX (side_pane->details->shortcut_box),
                            panel->shortcut,
                            FALSE, FALSE, 0);
    }
}

GtkWidget *
caja_side_pane_get_current_panel (CajaSidePane *side_pane)
{
    int index;

    index = gtk_notebook_get_current_page (GTK_NOTEBOOK (side_pane->details->notebook));
    return gtk_notebook_get_nth_page (GTK_NOTEBOOK (side_pane->details->notebook), index);
}

GtkWidget *
caja_side_pane_get_title (CajaSidePane *side_pane)
{
    return side_pane->details->title_hbox;
}
