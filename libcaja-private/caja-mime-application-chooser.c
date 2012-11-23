/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   caja-mime-application-chooser.c: an mime-application chooser

   Copyright (C) 2004 Novell, Inc.
   Copyright (C) 2007 Red Hat, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Dave Camp <dave@novell.com>
            Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "caja-mime-application-chooser.h"

#include "caja-open-with-dialog.h"
#include "caja-signaller.h"
#include "caja-file.h"
#include <eel/eel-stock-dialogs.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <src/glibcompat.h> /* for g_list_free_full */

struct _CajaMimeApplicationChooserDetails
{
    char *uri;

    char *content_type;
    char *extension;
    char *type_description;
    char *orig_mime_type;

    guint refresh_timeout;

    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *treeview;
    GtkWidget *remove_button;

    gboolean for_multiple_files;

    GtkListStore *model;
    GtkCellRenderer *toggle_renderer;
};

enum
{
    COLUMN_APPINFO,
    COLUMN_DEFAULT,
    COLUMN_ICON,
    COLUMN_NAME,
    NUM_COLUMNS
};

G_DEFINE_TYPE (CajaMimeApplicationChooser, caja_mime_application_chooser, GTK_TYPE_VBOX);

static void refresh_model             (CajaMimeApplicationChooser *chooser);
static void refresh_model_soon        (CajaMimeApplicationChooser *chooser);
static void mime_type_data_changed_cb (GObject                        *signaller,
                                       gpointer                        user_data);

static void
caja_mime_application_chooser_finalize (GObject *object)
{
    CajaMimeApplicationChooser *chooser;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (object);

    if (chooser->details->refresh_timeout)
    {
        g_source_remove (chooser->details->refresh_timeout);
    }

    g_signal_handlers_disconnect_by_func (caja_signaller_get_current (),
                                          G_CALLBACK (mime_type_data_changed_cb),
                                          chooser);


    g_free (chooser->details->uri);
    g_free (chooser->details->content_type);
    g_free (chooser->details->extension);
    g_free (chooser->details->type_description);
    g_free (chooser->details->orig_mime_type);

    g_free (chooser->details);

    G_OBJECT_CLASS (caja_mime_application_chooser_parent_class)->finalize (object);
}

static void
caja_mime_application_chooser_class_init (CajaMimeApplicationChooserClass *class)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->finalize = caja_mime_application_chooser_finalize;
}

static void
default_toggled_cb (GtkCellRendererToggle *renderer,
                    const char *path_str,
                    gpointer user_data)
{
    CajaMimeApplicationChooser *chooser;
    GtkTreeIter iter;
    GtkTreePath *path;
    GError *error;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    path = gtk_tree_path_new_from_string (path_str);
    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (chooser->details->model),
                                 &iter, path))
    {
        gboolean is_default;
        gboolean success;
        GAppInfo *info;
        char *message;

        gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->model),
                            &iter,
                            COLUMN_DEFAULT, &is_default,
                            COLUMN_APPINFO, &info,
                            -1);

        if (!is_default && info != NULL)
        {
            error = NULL;
            if (chooser->details->extension)
            {
                success = g_app_info_set_as_default_for_extension (info,
                          chooser->details->extension,
                          &error);
            }
            else
            {
                success = g_app_info_set_as_default_for_type (info,
                          chooser->details->content_type,
                          &error);
            }

            if (!success)
            {
                message = g_strdup_printf (_("Could not set application as the default: %s"), error->message);
                eel_show_error_dialog (_("Could not set as default application"),
                                       message,
                                       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
                g_free (message);
                g_error_free (error);
            }

            g_signal_emit_by_name (caja_signaller_get_current (),
                                   "mime_data_changed");
        }
        g_object_unref (info);
    }
    gtk_tree_path_free (path);
}

static GAppInfo *
get_selected_application (CajaMimeApplicationChooser *chooser)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GAppInfo *info;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->details->treeview));

    info = NULL;
    if (gtk_tree_selection_get_selected (selection,
                                         NULL,
                                         &iter))
    {
        gtk_tree_model_get (GTK_TREE_MODEL (chooser->details->model),
                            &iter,
                            COLUMN_APPINFO, &info,
                            -1);
    }

    return info;
}

static void
selection_changed_cb (GtkTreeSelection *selection,
                      gpointer user_data)
{
    CajaMimeApplicationChooser *chooser;
    GAppInfo *info;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    info = get_selected_application (chooser);
    if (info)
    {
        gtk_widget_set_sensitive (chooser->details->remove_button,
                                  g_app_info_can_remove_supports_type (info));

        g_object_unref (info);
    }
    else
    {
        gtk_widget_set_sensitive (chooser->details->remove_button,
                                  FALSE);
    }
}

static GtkWidget *
create_tree_view (CajaMimeApplicationChooser *chooser)
{
    GtkWidget *treeview;
    GtkListStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;

    treeview = gtk_tree_view_new ();
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

    store = gtk_list_store_new (NUM_COLUMNS,
                                G_TYPE_APP_INFO,
                                G_TYPE_BOOLEAN,
                                G_TYPE_ICON,
                                G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                          COLUMN_NAME,
                                          GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                             GTK_TREE_MODEL (store));
    chooser->details->model = store;

    renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (renderer, "toggled",
                      G_CALLBACK (default_toggled_cb),
                      chooser);
    gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (renderer),
                                        TRUE);

    column = gtk_tree_view_column_new_with_attributes (_("Default"),
             renderer,
             "active",
             COLUMN_DEFAULT,
             NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
    chooser->details->toggle_renderer = renderer;

    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "stock-size", GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Icon"),
             renderer,
             "gicon",
             COLUMN_ICON,
             NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Name"),
             renderer,
             "markup",
             COLUMN_NAME,
             NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    g_signal_connect (selection, "changed",
                      G_CALLBACK (selection_changed_cb),
                      chooser);

    return treeview;
}

static void
add_clicked_cb (GtkButton *button,
                gpointer user_data)
{
    CajaMimeApplicationChooser *chooser;
    GtkWidget *dialog;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    if (chooser->details->for_multiple_files)
    {
        dialog = caja_add_application_dialog_new_for_multiple_files (chooser->details->extension,
                 chooser->details->orig_mime_type);
    }
    else
    {
        dialog = caja_add_application_dialog_new (chooser->details->uri,
                 chooser->details->orig_mime_type);
    }
    gtk_window_set_screen (GTK_WINDOW (dialog),
                           gtk_widget_get_screen (GTK_WIDGET (chooser)));
    gtk_widget_show (dialog);
}

static void
remove_clicked_cb (GtkButton *button,
                   gpointer user_data)
{
    CajaMimeApplicationChooser *chooser;
    GError *error;
    GAppInfo *info;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    info = get_selected_application (chooser);

    if (info)
    {
        error = NULL;
        if (!g_app_info_remove_supports_type (info,
                                              chooser->details->content_type,
                                              &error))
        {
            eel_show_error_dialog (_("Could not remove application"),
                                   error->message,
                                   GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
            g_error_free (error);

        }
        g_signal_emit_by_name (caja_signaller_get_current (),
                               "mime_data_changed");
        g_object_unref (info);
    }
}

static void
reset_clicked_cb (GtkButton *button,
                  gpointer   user_data)
{
    CajaMimeApplicationChooser *chooser;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    g_app_info_reset_type_associations (chooser->details->content_type);

    g_signal_emit_by_name (caja_signaller_get_current (),
                           "mime_data_changed");
}

static void
mime_type_data_changed_cb (GObject *signaller,
                           gpointer user_data)
{
    CajaMimeApplicationChooser *chooser;

    chooser = CAJA_MIME_APPLICATION_CHOOSER (user_data);

    refresh_model_soon (chooser);
}

static void
caja_mime_application_chooser_init (CajaMimeApplicationChooser *chooser)
{
    GtkWidget *box;
    GtkWidget *scrolled;
    GtkWidget *button;

    chooser->details = g_new0 (CajaMimeApplicationChooserDetails, 1);

    chooser->details->for_multiple_files = FALSE;
    gtk_container_set_border_width (GTK_CONTAINER (chooser), 8);
    gtk_box_set_spacing (GTK_BOX (chooser), 0);
    gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);

    chooser->details->label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (chooser->details->label), 0.0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (chooser->details->label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (chooser->details->label),
                                  PANGO_WRAP_WORD_CHAR);
    gtk_box_pack_start (GTK_BOX (chooser), chooser->details->label,
                        FALSE, FALSE, 0);

    gtk_widget_show (chooser->details->label);

    scrolled = gtk_scrolled_window_new (NULL, NULL);

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
                                         GTK_SHADOW_IN);

    gtk_widget_show (scrolled);
    gtk_box_pack_start (GTK_BOX (chooser), scrolled, TRUE, TRUE, 6);

    chooser->details->treeview = create_tree_view (chooser);
    gtk_widget_show (chooser->details->treeview);

    gtk_container_add (GTK_CONTAINER (scrolled),
                       chooser->details->treeview);

    box = gtk_hbutton_box_new ();
    gtk_box_set_spacing (GTK_BOX (box), 6);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_END);
    gtk_box_pack_start (GTK_BOX (chooser), box, FALSE, FALSE, 6);
    gtk_widget_show (box);

    button = gtk_button_new_from_stock (GTK_STOCK_ADD);
    g_signal_connect (button, "clicked",
                      G_CALLBACK (add_clicked_cb),
                      chooser);

    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (box), button);

    button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
    g_signal_connect (button, "clicked",
                      G_CALLBACK (remove_clicked_cb),
                      chooser);

    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (box), button);

    chooser->details->remove_button = button;

    button = gtk_button_new_with_label (_("Reset"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (reset_clicked_cb),
                      chooser);

    gtk_widget_show (button);
    gtk_container_add (GTK_CONTAINER (box), button);

    g_signal_connect (caja_signaller_get_current (),
                      "mime_data_changed",
                      G_CALLBACK (mime_type_data_changed_cb),
                      chooser);
}

static char *
get_extension (const char *basename)
{
    char *p;

    p = strrchr (basename, '.');

    if (p && *(p + 1) != '\0')
    {
        return g_strdup (p + 1);
    }
    else
    {
        return NULL;
    }
}

static gboolean
refresh_model_timeout (gpointer data)
{
    CajaMimeApplicationChooser *chooser = data;

    chooser->details->refresh_timeout = 0;

    refresh_model (chooser);

    return FALSE;
}

/* This adds a slight delay so that we're sure the mime data is
   done writing */
static void
refresh_model_soon (CajaMimeApplicationChooser *chooser)
{
    if (chooser->details->refresh_timeout != 0)
        return;

    chooser->details->refresh_timeout =
        g_timeout_add (300,
                       refresh_model_timeout,
                       chooser);
}

static void
refresh_model (CajaMimeApplicationChooser *chooser)
{
    GList *applications;
    GAppInfo *default_app;
    GList *l;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;

    column = gtk_tree_view_get_column (GTK_TREE_VIEW (chooser->details->treeview), 0);
    gtk_tree_view_column_set_visible (column, TRUE);

    gtk_list_store_clear (chooser->details->model);

    applications = g_app_info_get_all_for_type (chooser->details->content_type);
    default_app = g_app_info_get_default_for_type (chooser->details->content_type, FALSE);

    for (l = applications; l != NULL; l = l->next)
    {
        GtkTreeIter iter;
        gboolean is_default;
        GAppInfo *application;
        char *escaped;
        GIcon *icon;

        application = l->data;

        is_default = default_app && g_app_info_equal (default_app, application);

        escaped = g_markup_escape_text (g_app_info_get_display_name (application), -1);

        icon = g_app_info_get_icon (application);

        gtk_list_store_append (chooser->details->model, &iter);
        gtk_list_store_set (chooser->details->model, &iter,
                            COLUMN_APPINFO, application,
                            COLUMN_DEFAULT, is_default,
                            COLUMN_ICON, icon,
                            COLUMN_NAME, escaped,
                            -1);

        g_free (escaped);
    }

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->details->treeview));

    if (applications)
    {
        g_object_set (chooser->details->toggle_renderer,
                      "visible", TRUE,
                      NULL);
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    }
    else
    {
        GtkTreeIter iter;
        char *name;

        gtk_tree_view_column_set_visible (column, FALSE);
        gtk_list_store_append (chooser->details->model, &iter);
        name = g_strdup_printf ("<i>%s</i>", _("No applications selected"));
        gtk_list_store_set (chooser->details->model, &iter,
                            COLUMN_NAME, name,
                            COLUMN_APPINFO, NULL,
                            -1);
        g_free (name);

        gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
    }

    if (default_app)
    {
        g_object_unref (default_app);
    }

    g_list_free_full (applications, g_object_unref);
}

static void
set_extension_and_description (CajaMimeApplicationChooser *chooser,
                               const char *extension,
                               const char *mime_type)
{
    if (extension != NULL &&
            g_content_type_is_unknown (mime_type))
    {
        chooser->details->extension = g_strdup (extension);
        chooser->details->content_type = g_strdup_printf ("application/x-extension-%s", extension);
        /* the %s here is a file extension */
        chooser->details->type_description =
            g_strdup_printf (_("%s document"), extension);
    }
    else
    {
        char *description;

        chooser->details->content_type = g_strdup (mime_type);
        description = g_content_type_get_description (mime_type);
        if (description == NULL)
        {
            description = g_strdup (_("Unknown"));
        }

        chooser->details->type_description = description;
    }
}

static gboolean
set_uri_and_type (CajaMimeApplicationChooser *chooser,
                  const char *uri,
                  const char *mime_type)
{
    char *label;
    char *name;
    char *emname;
    char *extension;
    GFile *file;

    chooser->details->uri = g_strdup (uri);

    file = g_file_new_for_uri (uri);
    name = g_file_get_basename (file);
    g_object_unref (file);

    chooser->details->orig_mime_type = g_strdup (mime_type);

    extension = get_extension (name);
    set_extension_and_description (CAJA_MIME_APPLICATION_CHOOSER (chooser),
                                   extension, mime_type);
    g_free (extension);

    /* first %s is filename, second %s is mime-type description */
    emname = g_strdup_printf ("<i>%s</i>", name);
    label = g_strdup_printf (_("Select an application to open %s and other files of type \"%s\""),
                             emname, chooser->details->type_description);
    g_free (emname);

    gtk_label_set_markup (GTK_LABEL (chooser->details->label), label);

    g_free (label);
    g_free (name);

    refresh_model (chooser);

    return TRUE;
}

static char *
get_extension_from_file (CajaFile *nfile)
{
    char *name;
    char *extension;

    name = caja_file_get_name (nfile);
    extension = get_extension (name);

    g_free (name);

    return extension;
}

static gboolean
set_uri_and_type_for_multiple_files (CajaMimeApplicationChooser *chooser,
                                     GList *uris,
                                     const char *mime_type)
{
    char *label;
    char *first_extension;
    gboolean same_extension;
    GList *iter;

    chooser->details->for_multiple_files = TRUE;
    chooser->details->uri = NULL;
    chooser->details->orig_mime_type = g_strdup (mime_type);
    same_extension = TRUE;
    first_extension = get_extension_from_file (CAJA_FILE (uris->data));
    iter = uris->next;

    while (iter != NULL)
    {
        char *extension_current;

        extension_current = get_extension_from_file (CAJA_FILE (iter->data));
        if (g_strcmp0 (first_extension, extension_current)) {
            same_extension = FALSE;
            g_free (extension_current);
            break;
        }
        iter = iter->next;

        g_free (extension_current);
    }
    if (!same_extension)
    {
        set_extension_and_description (CAJA_MIME_APPLICATION_CHOOSER (chooser),
                                       NULL, mime_type);
    }
    else
    {
        set_extension_and_description (CAJA_MIME_APPLICATION_CHOOSER (chooser),
                                       first_extension, mime_type);
    }

    g_free (first_extension);

    label = g_strdup_printf (_("Open all files of type \"%s\" with:"),
                             chooser->details->type_description);
    gtk_label_set_markup (GTK_LABEL (chooser->details->label), label);

    g_free (label);

    refresh_model (chooser);

    return TRUE;
}

GtkWidget *
caja_mime_application_chooser_new (const char *uri,
                                   const char *mime_type)
{
    GtkWidget *chooser;

    chooser = gtk_widget_new (CAJA_TYPE_MIME_APPLICATION_CHOOSER, NULL);

    set_uri_and_type (CAJA_MIME_APPLICATION_CHOOSER (chooser), uri, mime_type);

    return chooser;
}

GtkWidget *
caja_mime_application_chooser_new_for_multiple_files (GList *uris,
        const char *mime_type)
{
    GtkWidget *chooser;

    chooser = gtk_widget_new (CAJA_TYPE_MIME_APPLICATION_CHOOSER, NULL);

    set_uri_and_type_for_multiple_files (CAJA_MIME_APPLICATION_CHOOSER (chooser),
                                         uris, mime_type);

    return chooser;
}

