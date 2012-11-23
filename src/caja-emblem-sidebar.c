/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 * Copyright (C) 2001 Red Hat, Inc.
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
 * Authors:  James Willcox  <jwillcox@gnome.org>
 *           Alexander Larsson <alexl@redhat.com>
 *
 * This is a sidebar displaying emblems which can be dragged onto files to
 * set/unset the chosen emblem.
 *
 */

#include <config.h>
#include "caja-emblem-sidebar.h"

#include <stdio.h>
#include <eel/eel-wrap-table.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libcaja-private/caja-icon-dnd.h>
#include <libcaja-private/caja-emblem-utils.h>
#include <libcaja-private/caja-file-utilities.h>
#include <libcaja-private/caja-sidebar-provider.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-signaller.h>

#include "glibcompat.h" /* for g_list_free_full */

struct CajaEmblemSidebarDetails
{
    CajaWindowInfo *window;
    GtkWidget *emblems_table;
    GtkWidget *popup;
    GtkWidget *popup_remove;
    GtkWidget *popup_rename;

    char *popup_emblem_keyword;
    char *popup_emblem_display_name;
    GdkPixbuf *popup_emblem_pixbuf;
};

#define ERASE_EMBLEM_KEYWORD			"erase"
#define STANDARD_EMBLEM_HEIGHT			52
#define EMBLEM_LABEL_SPACING			2

static void caja_emblem_sidebar_populate          (CajaEmblemSidebar        *emblem_sidebar);
static void caja_emblem_sidebar_refresh           (CajaEmblemSidebar        *emblem_sidebar);
static void caja_emblem_sidebar_iface_init        (CajaSidebarIface         *iface);
static void sidebar_provider_iface_init           (CajaSidebarProviderIface *iface);
static GType caja_emblem_sidebar_provider_get_type (void);

static const GtkTargetEntry drag_types[] =
{
    {"property/keyword", 0, 0 }
};

enum
{
    TARGET_URI_LIST,
    TARGET_URI,
    TARGET_NETSCAPE_URL
};

static const GtkTargetEntry dest_types[] =
{
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"text/plain", 0, TARGET_URI},
    {"_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL}
};

typedef struct _Emblem
{
    GdkPixbuf *pixbuf;
    char *uri;
    char *name;
    char *keyword;
} Emblem;

typedef struct
{
    GObject parent;
} CajaEmblemSidebarProvider;

typedef struct
{
    GObjectClass parent;
} CajaEmblemSidebarProviderClass;


G_DEFINE_TYPE_WITH_CODE (CajaEmblemSidebar, caja_emblem_sidebar, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR,
                                 caja_emblem_sidebar_iface_init));

G_DEFINE_TYPE_WITH_CODE (CajaEmblemSidebarProvider, caja_emblem_sidebar_provider, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CAJA_TYPE_SIDEBAR_PROVIDER,
                                 sidebar_provider_iface_init));

static void
caja_emblem_sidebar_drag_data_get_cb (GtkWidget *widget,
                                      GdkDragContext *context,
                                      GtkSelectionData *data,
                                      guint info,
                                      guint time,
                                      CajaEmblemSidebar *emblem_sidebar)
{
    char *keyword;

    keyword = g_object_get_data (G_OBJECT (widget), "emblem-keyword");

    g_return_if_fail (keyword != NULL);

    gtk_selection_data_set (data, gtk_selection_data_get_target (data), 8,
                            keyword,
                            strlen (keyword));
}

static void
caja_emblem_sidebar_enter_notify_cb (GtkWidget *widget,
                                     CajaEmblemSidebar *emblem_sidebar)
{
    GdkPixbuf *pixbuf;
    EelLabeledImage *image;

    pixbuf = g_object_get_data (G_OBJECT (widget), "prelight-pixbuf");
    image = g_object_get_data (G_OBJECT (widget), "labeled-image");

    eel_labeled_image_set_pixbuf (EEL_LABELED_IMAGE (image), pixbuf);
}

static void
caja_emblem_sidebar_leave_notify_cb (GtkWidget *widget,
                                     CajaEmblemSidebar *emblem_sidebar)
{
    GdkPixbuf *pixbuf;
    EelLabeledImage *image;

    pixbuf = g_object_get_data (G_OBJECT (widget), "original-pixbuf");
    image = g_object_get_data (G_OBJECT (widget), "labeled-image");

    eel_labeled_image_set_pixbuf (EEL_LABELED_IMAGE (image), pixbuf);
}

static gboolean
caja_emblem_sidebar_button_press_cb (GtkWidget *widget,
                                     GdkEventButton *event,
                                     CajaEmblemSidebar *emblem_sidebar)
{
    char *keyword, *name;
    GdkPixbuf *pixbuf;

    if (event->button == 3)
    {
        keyword = g_object_get_data (G_OBJECT (widget),
                                     "emblem-keyword");
        name = g_object_get_data (G_OBJECT (widget),
                                  "emblem-display-name");
        pixbuf = g_object_get_data (G_OBJECT (widget),
                                    "original-pixbuf");

        emblem_sidebar->details->popup_emblem_keyword = keyword;
        emblem_sidebar->details->popup_emblem_display_name = name;
        emblem_sidebar->details->popup_emblem_pixbuf = pixbuf;

        gtk_widget_set_sensitive (emblem_sidebar->details->popup_remove,
                                  caja_emblem_can_remove_emblem (keyword));
        gtk_widget_set_sensitive (emblem_sidebar->details->popup_rename,
                                  caja_emblem_can_rename_emblem (keyword));


        gtk_menu_popup (GTK_MENU (emblem_sidebar->details->popup),
                        NULL, NULL, NULL, NULL, event->button,
                        event->time);
    }

    return TRUE;
}

static void
send_emblems_changed (void)
{
    g_signal_emit_by_name (caja_signaller_get_current (),
                           "emblems_changed");
}

static void
emblems_changed_callback (GObject *signaller,
                          CajaEmblemSidebar *emblem_sidebar)
{
    caja_emblem_sidebar_refresh (emblem_sidebar);
}

static void
caja_emblem_sidebar_delete_cb (GtkWidget *menu_item,
                               CajaEmblemSidebar *emblem_sidebar)
{
    char *error;

    if (caja_emblem_remove_emblem (emblem_sidebar->details->popup_emblem_keyword))
    {
        send_emblems_changed ();
    }
    else
    {
        error = g_strdup_printf (_("Could not remove emblem with name '%s'."), emblem_sidebar->details->popup_emblem_display_name);
        eel_show_error_dialog (error, _("This is probably because the emblem is a permanent one, and not one that you added yourself."),
                               NULL);
        g_free (error);
    }
}

static void
rename_dialog_response_cb (GtkWidget *dialog, int response,
                           CajaEmblemSidebar *emblem_sidebar)
{
    GtkWidget *entry;
    char *keyword, *name, *error;

    keyword = g_object_get_data (G_OBJECT (dialog), "emblem-keyword");

    if (response == GTK_RESPONSE_CANCEL)
    {
        g_free (keyword);
        gtk_widget_destroy (dialog);
        return;
    }
    else if (response == GTK_RESPONSE_HELP)
    {
        g_message ("Implement me!");
        return;
    }

    entry = g_object_get_data (G_OBJECT (dialog), "entry");

    name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

    gtk_widget_destroy (dialog);

    if (caja_emblem_rename_emblem (keyword, name))
    {
        send_emblems_changed ();
    }
    else
    {
        error = g_strdup_printf (_("Could not rename emblem with name '%s'."), name);
        eel_show_error_dialog (error, _("This is probably because the emblem is a permanent one, and not one that you added yourself."),
                               NULL);
        g_free (error);
    }

    g_free (keyword);
    g_free (name);
}

static GtkWidget *
create_rename_emblem_dialog (CajaEmblemSidebar *emblem_sidebar,
                             const char *keyword, const char *orig_name,
                             GdkPixbuf *pixbuf)
{
    GtkWidget *dialog, *label, *image, *entry, *hbox;

    image = gtk_image_new_from_pixbuf (pixbuf);
    entry = gtk_entry_new ();

    dialog = gtk_dialog_new_with_buttons (_("Rename Emblem"),
                                          NULL,
                                          0,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK,
                                          GTK_RESPONSE_OK,
                                          GTK_STOCK_HELP,
                                          GTK_RESPONSE_HELP,
                                          NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK);

    g_object_set_data (G_OBJECT (dialog), "emblem-keyword",
                       g_strdup (keyword));
    g_object_set_data (G_OBJECT (dialog), "entry",
                       entry);

    label = gtk_label_new (_("Enter a new name for the displayed emblem:"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), label,
                        FALSE, FALSE, 8);


    hbox = gtk_hbox_new (FALSE, 8);
    gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 8);

    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, FALSE, 8);
    gtk_widget_show_all (hbox);

    /* it would be nice to have the text selected, ready to be overwritten
     * by the user, but that doesn't seem possible.
     */
    gtk_widget_grab_focus (entry);
    gtk_entry_set_text (GTK_ENTRY (entry), orig_name);

    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox,
                        TRUE, TRUE, 8);


    return dialog;
}

static void
caja_emblem_sidebar_rename_cb (GtkWidget *menu_item,
                               CajaEmblemSidebar *emblem_sidebar)
{
    GtkWidget *dialog;

    dialog = create_rename_emblem_dialog (emblem_sidebar,
                                          emblem_sidebar->details->popup_emblem_keyword,
                                          emblem_sidebar->details->popup_emblem_display_name,
                                          emblem_sidebar->details->popup_emblem_pixbuf);
    g_signal_connect (dialog, "response",
                      G_CALLBACK (rename_dialog_response_cb),
                      emblem_sidebar);
    gtk_widget_show (dialog);
}

static void
create_popup_menu (CajaEmblemSidebar *emblem_sidebar)
{
    GtkWidget *popup, *menu_item, *menu_image;

    popup = gtk_menu_new ();

    /* add the "rename" menu item */
    menu_image = gtk_image_new_from_stock (GTK_STOCK_PROPERTIES,
                                           GTK_ICON_SIZE_MENU);
    gtk_widget_show (menu_image);
    menu_item = gtk_image_menu_item_new_with_label (_("Rename"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
                                   menu_image);

    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (caja_emblem_sidebar_rename_cb),
                      emblem_sidebar);
    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
    emblem_sidebar->details->popup_rename = menu_item;

    /* add "delete" menu item */
    menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_DELETE,
                NULL);
    g_signal_connect (menu_item, "activate",
                      G_CALLBACK (caja_emblem_sidebar_delete_cb),
                      emblem_sidebar);
    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
    emblem_sidebar->details->popup_remove = menu_item;

    emblem_sidebar->details->popup = popup;
}

static GtkWidget *
create_emblem_widget_with_pixbuf (CajaEmblemSidebar *emblem_sidebar,
                                  const char *keyword,
                                  const char *display_name,
                                  GdkPixbuf *pixbuf)
{
    GtkWidget *image, *event_box;
    GdkPixbuf *prelight_pixbuf;


    image = eel_labeled_image_new (display_name, pixbuf);

    eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (image),
            STANDARD_EMBLEM_HEIGHT);
    eel_labeled_image_set_spacing (EEL_LABELED_IMAGE (image),
                                   EMBLEM_LABEL_SPACING);
    event_box = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (event_box), image);

    prelight_pixbuf = eel_create_spotlight_pixbuf (pixbuf);


    gtk_drag_source_set (event_box, GDK_BUTTON1_MASK, drag_types,
                         G_N_ELEMENTS (drag_types),
                         GDK_ACTION_COPY | GDK_ACTION_MOVE);

    gtk_drag_source_set_icon_pixbuf (event_box, pixbuf);



    g_signal_connect (event_box, "button_press_event",
                      G_CALLBACK (caja_emblem_sidebar_button_press_cb),
                      emblem_sidebar);
    g_signal_connect (event_box, "drag-data-get",
                      G_CALLBACK (caja_emblem_sidebar_drag_data_get_cb),
                      emblem_sidebar);
    g_signal_connect (event_box, "enter-notify-event",
                      G_CALLBACK (caja_emblem_sidebar_enter_notify_cb),
                      emblem_sidebar);
    g_signal_connect (event_box, "leave-notify-event",
                      G_CALLBACK (caja_emblem_sidebar_leave_notify_cb),
                      emblem_sidebar);

    g_object_set_data_full (G_OBJECT (event_box),
                            "emblem-keyword",
                            g_strdup (keyword), g_free);
    g_object_set_data_full (G_OBJECT (event_box),
                            "emblem-display-name",
                            g_strdup (display_name), g_free);
    g_object_set_data_full (G_OBJECT (event_box),
                            "original-pixbuf",
                            pixbuf, g_object_unref);
    g_object_set_data_full (G_OBJECT (event_box),
                            "prelight-pixbuf",
                            prelight_pixbuf, g_object_unref);
    g_object_set_data (G_OBJECT (event_box),
                       "labeled-image", image);

    return event_box;

}

static GtkWidget *
create_emblem_widget (CajaEmblemSidebar *emblem_sidebar,
                      const char *name)
{
    GtkWidget *ret;
    const char *display_name;
    char *keyword;
    GdkPixbuf *pixbuf;
    CajaIconInfo *info;

    info = caja_icon_info_lookup_from_name (name, CAJA_ICON_SIZE_STANDARD);

    pixbuf = caja_icon_info_get_pixbuf_at_size (info, CAJA_ICON_SIZE_STANDARD);

    display_name = caja_icon_info_get_display_name (info);

    keyword = caja_emblem_get_keyword_from_icon_name (name);
    if (display_name == NULL)
    {
        display_name = keyword;
    }

    ret = create_emblem_widget_with_pixbuf (emblem_sidebar, keyword,
                                            display_name, pixbuf);
    g_free (keyword);
    g_object_unref (info);
    return ret;
}

static void
emblem_name_entry_changed_cb (GtkWidget *entry, Emblem *emblem)
{
    char *text;

    g_free (emblem->name);

    text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

    emblem->name = g_strdup (text);
}


static void
destroy_emblem (Emblem *emblem, gpointer user_data)
{
    g_return_if_fail (emblem != NULL);


    if (emblem->pixbuf != NULL)
    {
        g_object_unref (emblem->pixbuf);
        emblem->pixbuf = NULL;
    }

    if (emblem->name != NULL)
    {
        g_free (emblem->name);
        emblem->name = NULL;
    }

    if (emblem->uri != NULL)
    {
        g_free (emblem->uri);
        emblem->uri = NULL;
    }

    if (emblem->keyword != NULL)
    {
        g_free (emblem->keyword);
        emblem->keyword = NULL;
    }

    g_free (emblem);
}

static void
destroy_emblem_list (GSList *list)
{
    g_slist_foreach (list, (GFunc)destroy_emblem, NULL);
    g_slist_free (list);
}

static GtkWidget *
create_add_emblems_dialog (CajaEmblemSidebar *emblem_sidebar,
                           GSList *emblems)
{
    GtkWidget *dialog, *label, *table, *image;
    GtkWidget *first_entry, *entry, *scroller, *hbox;
    Emblem *emblem;
    GSList *list;
    int num_emblems;

    first_entry = NULL;

    dialog = gtk_dialog_new_with_buttons (_("Add Emblems..."),
                                          NULL,
                                          0,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK,
                                          GTK_RESPONSE_OK,
                                          GTK_STOCK_HELP,
                                          GTK_RESPONSE_HELP,
                                          NULL);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK);

    /* FIXME:  make a better message */
    if (g_slist_length (emblems) > 1)
    {
        label = gtk_label_new (_("Enter a descriptive name next to each emblem.  This name will be used in other places to identify the emblem."));
    }
    else
    {
        label = gtk_label_new (_("Enter a descriptive name next to the emblem.  This name will be used in other places to identify the emblem."));
    }

    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        label, FALSE, FALSE, 8);
    gtk_widget_show (label);

    scroller = eel_scrolled_wrap_table_new (TRUE, GTK_SHADOW_NONE, &table);
    eel_wrap_table_set_x_spacing (EEL_WRAP_TABLE (table), 8);
    eel_wrap_table_set_y_spacing (EEL_WRAP_TABLE (table), 8);

    num_emblems=0;
    list = emblems;
    while (list != NULL)
    {
        /* walk through the list of emblems, and create an image
         * and entry for each one
         */

        emblem = (Emblem *)list->data;
        list = list->next;

        image = gtk_image_new_from_pixbuf (emblem->pixbuf);

        hbox = gtk_hbox_new (TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

        entry = gtk_entry_new ();
        gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
        g_signal_connect (entry, "changed",
                          G_CALLBACK (emblem_name_entry_changed_cb),
                          emblem);

        gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (table), hbox);

        if (num_emblems == 0)
        {
            first_entry = entry;
        }

        num_emblems++;
    }

    gtk_container_set_border_width (GTK_CONTAINER (dialog), 8);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        scroller, TRUE, TRUE, 8);
    gtk_widget_show_all (scroller);

    gtk_widget_grab_focus (first_entry);

    /* we expand the window to hold up to about 4 emblems, but after that
     * let the scroller do its thing.  Is there a better way to do this?
     */
    gtk_window_set_default_size (GTK_WINDOW (dialog), 400,
                                 MIN (120+(60*num_emblems), 350));

    g_object_set_data_full (G_OBJECT (dialog), "emblems-to-add",
                            emblems, (GDestroyNotify)destroy_emblem_list);

    return dialog;
}

static void
remove_widget (GtkWidget *widget, GtkContainer *container)
{
    gtk_container_remove (container, widget);
}

static void
caja_emblem_sidebar_refresh (CajaEmblemSidebar *emblem_sidebar)
{
    caja_emblem_refresh_list ();

    gtk_container_foreach (GTK_CONTAINER (emblem_sidebar->details->emblems_table),
                           (GtkCallback)remove_widget,
                           emblem_sidebar->details->emblems_table);

    caja_emblem_sidebar_populate (emblem_sidebar);
}

static void
add_emblems_dialog_response_cb (GtkWidget *dialog, int response,
                                CajaEmblemSidebar *emblem_sidebar)
{
    Emblem *emblem;
    GSList *emblems;
    GSList *l;

    switch (response)
    {
    case GTK_RESPONSE_CANCEL:
        gtk_widget_destroy (dialog);
        break;

    case GTK_RESPONSE_HELP:
        g_message ("Implement me!");
        break;

    case GTK_RESPONSE_OK:
        emblems = g_object_get_data (G_OBJECT (dialog),
                                     "emblems-to-add");

        for (l = emblems; l; l = l->next)
        {
            char *keyword;

            emblem = (Emblem *)l->data;
            if (emblem->keyword != NULL)
            {
                /* this one has already been verified */
                continue;
            }

            keyword = caja_emblem_create_unique_keyword (emblem->name);
            if (!caja_emblem_verify_keyword
                    (GTK_WINDOW (dialog), keyword, emblem->name))
            {
                g_free (keyword);
                return;
            }
            else
            {
                emblem->keyword = keyword;
            }

        }

        for (l = emblems; l; l = l->next)
        {
            emblem = (Emblem *)l->data;

            caja_emblem_install_custom_emblem (emblem->pixbuf,
                                               emblem->keyword,
                                               emblem->name,
                                               GTK_WINDOW (dialog));
        }

        gtk_widget_destroy (dialog);

        send_emblems_changed ();
        break;
    }
}

static void
show_add_emblems_dialog (CajaEmblemSidebar *emblem_sidebar,
                         GSList *emblems)
{
    GtkWidget *dialog;

    g_return_if_fail (emblems != NULL);

    dialog = create_add_emblems_dialog (emblem_sidebar, emblems);

    if (dialog == NULL)
    {
        return;
    }

    g_signal_connect (dialog, "response",
                      G_CALLBACK (add_emblems_dialog_response_cb),
                      emblem_sidebar);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
caja_emblem_sidebar_drag_received_cb (GtkWidget *widget,
                                      GdkDragContext *drag_context,
                                      gint x,
                                      gint y,
                                      GtkSelectionData *data,
                                      guint info,
                                      guint time,
                                      CajaEmblemSidebar *emblem_sidebar)
{
    GSList *emblems;
    Emblem *emblem;
    GdkPixbuf *pixbuf;
    char *uri, *error, *uri_utf8;
    char **uris;
    GFile *f;
    int i;
    gboolean had_failure;
    gint data_format, data_length;
    const guchar *data_data;

    had_failure = FALSE;
    emblems = NULL;
    data_format = gtk_selection_data_get_format (data);
    data_length = gtk_selection_data_get_length (data);
    data_data = gtk_selection_data_get_data (data);

    switch (info)
    {
    case TARGET_URI_LIST:
        if (data_format != 8 ||
                data_length == 0)
        {
            g_message ("URI list had wrong format (%d) or length (%d)\n",
                       data_format, data_length);
            return;
        }

        uris = g_uri_list_extract_uris (data_data);
        if (uris == NULL)
        {
            break;
        }

        for (i = 0; uris[i] != NULL; ++i)
        {
            f = g_file_new_for_uri (uris[i]);
            pixbuf = caja_emblem_load_pixbuf_for_emblem (f);

            if (pixbuf == NULL)
            {
                /* this one apparently isn't an image, or
                 * at least not one that we know how to read
                 */
                had_failure = TRUE;
                g_object_unref (f);
                continue;
            }

            emblem = g_new (Emblem, 1);
            emblem->uri = g_file_get_uri (f);
            emblem->name = NULL; /* created later on by the user */
            emblem->keyword = NULL;
            emblem->pixbuf = pixbuf;

            g_object_unref (f);

            emblems = g_slist_prepend (emblems, emblem);
        }

        g_strfreev (uris);

        if (had_failure && emblems != NULL)
        {
            eel_show_error_dialog (_("Some of the files could not be added as emblems."), _("The emblems do not appear to be valid images."), NULL);
        }
        else if (had_failure && emblems == NULL)
        {
            eel_show_error_dialog (_("None of the files could be added as emblems."), _("The emblems do not appear to be valid images."), NULL);

        }

        if (emblems != NULL)
        {
            show_add_emblems_dialog (emblem_sidebar, emblems);
        }

        break;

    case TARGET_URI:
        if (data_format != 8 ||
                data_length == 0)
        {
            g_warning ("URI had wrong format (%d) or length (%d)\n",
                       data_format, data_length);
            return;
        }

        uri = g_strndup (data_data, data_length);

        f = g_file_new_for_uri (uri);
        pixbuf = caja_emblem_load_pixbuf_for_emblem (f);

        if (pixbuf != NULL)
        {
            emblem = g_new (Emblem, 1);
            emblem->uri = uri;
            emblem->name = NULL;
            emblem->keyword = NULL;
            emblem->pixbuf = pixbuf;

            emblems = g_slist_prepend (NULL, emblem);

            show_add_emblems_dialog (emblem_sidebar, emblems);
        }
        else
        {
            uri_utf8 = g_file_get_parse_name (f);

            if (uri_utf8)
            {
                error = g_strdup_printf (_("The file '%s' does not appear to be a valid image."), uri_utf8);
                g_free (uri_utf8);
            }
            else
            {
                error = g_strdup (_("The dragged file does not appear to be a valid image."));
            }
            eel_show_error_dialog (_("The emblem cannot be added."), error, NULL);
            g_free (error);
            g_free (uri_utf8);
        }

        g_object_unref (f);
        g_free (uri);

        break;

    case TARGET_NETSCAPE_URL:
        if (data_format != 8 ||
                data_length == 0)
        {
            g_message ("URI had wrong format (%d) or length (%d)\n",
                       data_format, data_length);
            return;
        }

        /* apparently, this is a URI/title pair?  or just a pair
         * of identical URIs?  Regardless, this seems to work...
         */

        uris = g_uri_list_extract_uris (data_data);
        if (uris == NULL)
        {
            break;
        }

        uri = uris[0];
        if (uri == NULL)
        {
            g_strfreev (uris);
            break;
        }

        f = g_file_new_for_uri (uri);
        pixbuf = caja_emblem_load_pixbuf_for_emblem (f);
        g_object_unref (f);

        if (pixbuf != NULL)
        {
            emblem = g_new (Emblem, 1);
            emblem->uri = g_strdup (uri);
            emblem->name = NULL;
            emblem->keyword = NULL;
            emblem->pixbuf = pixbuf;

            emblems = g_slist_prepend (NULL, emblem);

            show_add_emblems_dialog (emblem_sidebar, emblems);
        }
        else
        {
            g_warning ("Tried to load '%s', but failed.\n",
                       uri);
            error = g_strdup_printf (_("The file '%s' does not appear to be a valid image."), uri);
            eel_show_error_dialog (_("The emblem cannot be added."), error, NULL);
            g_free (error);
        }

        g_strfreev (uris);

        break;
    }
}

static GtkWidget *
caja_emblem_sidebar_create_container (CajaEmblemSidebar *emblem_sidebar)
{
    GtkWidget *emblems_table, *scroller;

    /* The emblems wrapped table */
    scroller = eel_scrolled_wrap_table_new (TRUE, GTK_SHADOW_IN, &emblems_table);

    gtk_container_set_border_width (GTK_CONTAINER (emblems_table), 8);

    /* set up dnd for adding emblems */
    gtk_drag_dest_set (scroller,
                       GTK_DEST_DEFAULT_ALL,
                       dest_types, G_N_ELEMENTS (dest_types),
                       GDK_ACTION_COPY | GDK_ACTION_MOVE);

    g_signal_connect (scroller, "drag-data-received",
                      G_CALLBACK (caja_emblem_sidebar_drag_received_cb),
                      emblem_sidebar);

    gtk_widget_show (scroller);

    emblem_sidebar->details->emblems_table = emblems_table;

    return scroller;
}

static gint
emblem_widget_sort_func (gconstpointer a, gconstpointer b)
{
    GObject *obj_a, *obj_b;

    obj_a = G_OBJECT (a);
    obj_b = G_OBJECT (b);

    return strcmp (g_object_get_data (obj_a, "emblem-display-name"),
                   g_object_get_data (obj_b, "emblem-display-name"));
}

static void
caja_emblem_sidebar_populate (CajaEmblemSidebar *emblem_sidebar)
{
    GList *icons, *l, *widgets;
    GtkWidget *emblem_widget;
    char *name;
    char *path;
    GdkPixbuf *erase_pixbuf;

    erase_pixbuf = NULL;

    path = caja_pixmap_file ("erase.png");
    if (path != NULL)
    {
        erase_pixbuf = gdk_pixbuf_new_from_file (path, NULL);
    }
    g_free (path);

    if (erase_pixbuf != NULL)
    {
        emblem_widget = create_emblem_widget_with_pixbuf (emblem_sidebar,
                        ERASE_EMBLEM_KEYWORD,
                        _("Erase"),
                        erase_pixbuf);
        gtk_container_add (GTK_CONTAINER
                           (emblem_sidebar->details->emblems_table),
                           emblem_widget);
    }


    icons = caja_emblem_list_available ();

    l = icons;
    widgets = NULL;
    while (l != NULL)
    {
        name = (char *)l->data;
        l = l->next;

        if (!caja_emblem_should_show_in_list (name))
        {
            continue;
        }

        emblem_widget = create_emblem_widget (emblem_sidebar, name);

        widgets = g_list_prepend (widgets, emblem_widget);
    }
    g_list_free_full (icons, g_free);

    /* sort the emblems by display name */
    widgets = g_list_sort (widgets, emblem_widget_sort_func);

    l = widgets;
    while (l != NULL)
    {
        gtk_container_add
        (GTK_CONTAINER (emblem_sidebar->details->emblems_table),
         l->data);
        l = l->next;
    }
    g_list_free (widgets);

    gtk_widget_show_all (emblem_sidebar->details->emblems_table);
}

static void
caja_emblem_sidebar_init (CajaEmblemSidebar *emblem_sidebar)
{
    GtkWidget *widget;

    emblem_sidebar->details = g_new0 (CajaEmblemSidebarDetails, 1);

    create_popup_menu (emblem_sidebar);

    widget = caja_emblem_sidebar_create_container (emblem_sidebar);
    caja_emblem_sidebar_populate (emblem_sidebar);

    g_signal_connect_object (caja_signaller_get_current (),
                             "emblems_changed",
                             G_CALLBACK (emblems_changed_callback), emblem_sidebar, 0);

    gtk_box_pack_start (GTK_BOX (emblem_sidebar), widget,
                        TRUE, TRUE, 0);
}

static void
caja_emblem_sidebar_finalize (GObject *object)
{
    CajaEmblemSidebar *emblem_sidebar;

    g_assert (CAJA_IS_EMBLEM_SIDEBAR (object));
    emblem_sidebar = CAJA_EMBLEM_SIDEBAR (object);

    if (emblem_sidebar->details != NULL)
    {
        g_free (emblem_sidebar->details);
    }

    G_OBJECT_CLASS (caja_emblem_sidebar_parent_class)->finalize (object);
}

static void
caja_emblem_sidebar_class_init (CajaEmblemSidebarClass *object_klass)
{
    GObjectClass *gobject_class;

    CajaEmblemSidebarClass *klass;

    klass = CAJA_EMBLEM_SIDEBAR_CLASS (object_klass);
    gobject_class = G_OBJECT_CLASS (object_klass);

    gobject_class->finalize = caja_emblem_sidebar_finalize;
}

static const char *
caja_emblem_sidebar_get_sidebar_id (CajaSidebar *sidebar)
{
    return CAJA_EMBLEM_SIDEBAR_ID;
}

static char *
caja_emblem_sidebar_get_tab_label (CajaSidebar *sidebar)
{
    return g_strdup (_("Emblems"));
}

static char *
caja_emblem_sidebar_get_tab_tooltip (CajaSidebar *sidebar)
{
    return g_strdup (_("Show Emblems"));
}

static GdkPixbuf *
caja_emblem_sidebar_get_tab_icon (CajaSidebar *sidebar)
{
    return NULL;
}

static void
caja_emblem_sidebar_is_visible_changed (CajaSidebar *sidebar,
                                        gboolean         is_visible)
{
    /* Do nothing */
}

static void
caja_emblem_sidebar_iface_init (CajaSidebarIface *iface)
{
    iface->get_sidebar_id = caja_emblem_sidebar_get_sidebar_id;
    iface->get_tab_label = caja_emblem_sidebar_get_tab_label;
    iface->get_tab_tooltip = caja_emblem_sidebar_get_tab_tooltip;
    iface->get_tab_icon = caja_emblem_sidebar_get_tab_icon;
    iface->is_visible_changed = caja_emblem_sidebar_is_visible_changed;
}

static void
caja_emblem_sidebar_set_parent_window (CajaEmblemSidebar *sidebar,
                                       CajaWindowInfo *window)
{
    sidebar->details->window = window;
}

static CajaSidebar *
caja_emblem_sidebar_create (CajaSidebarProvider *provider,
                            CajaWindowInfo *window)
{
    CajaEmblemSidebar *sidebar;

    sidebar = g_object_new (caja_emblem_sidebar_get_type (), NULL);
    caja_emblem_sidebar_set_parent_window (sidebar, window);
    g_object_ref_sink (sidebar);

    return CAJA_SIDEBAR (sidebar);
}

static void
sidebar_provider_iface_init (CajaSidebarProviderIface *iface)
{
    iface->create = caja_emblem_sidebar_create;
}

static void
caja_emblem_sidebar_provider_init (CajaEmblemSidebarProvider *sidebar)
{
}

static void
caja_emblem_sidebar_provider_class_init (CajaEmblemSidebarProviderClass *class)
{
}

void
caja_emblem_sidebar_register (void)
{
    caja_module_add_type (caja_emblem_sidebar_provider_get_type ());
}

