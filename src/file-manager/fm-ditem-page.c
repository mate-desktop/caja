/*
 *  fm-ditem-page.c: Desktop item editing support
 *
 *  Copyright (C) 2004 James Willcox
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: James Willcox <jwillcox@gnome.org>
 *
 */

#include <config.h>
#include "fm-ditem-page.h"

#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libcaja-extension/caja-extension-types.h>
#include <libcaja-extension/caja-file-info.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-file-attributes.h>

#define MAIN_GROUP "Desktop Entry"

#if GTK_CHECK_VERSION (3, 0, 0)
#define gtk_vbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_VERTICAL,Y)
#endif

typedef struct ItemEntry
{
    const char *field;
    const char *description;
    char *current_value;
    gboolean localized;
    gboolean filename;
} ItemEntry;

enum
{
    TARGET_URI_LIST
};

static const GtkTargetEntry target_table[] =
{
    { "text/uri-list",  0, TARGET_URI_LIST }
};

static gboolean
_g_key_file_load_from_gfile (GKeyFile *key_file,
                             GFile *file,
                             GKeyFileFlags flags,
                             GError **error)
{
    char *data;
    gsize len;
    gboolean res;

    if (!g_file_load_contents (file, NULL, &data, &len, NULL, error))
    {
        return FALSE;
    }

    res = g_key_file_load_from_data (key_file, data, len, flags, error);

    g_free (data);

    return res;
}

static gboolean
_g_key_file_save_to_uri (GKeyFile *key_file,
                         const char *uri,
                         GError  **error)
{
    GFile *file;
    char *data;
    gsize len;

    data = g_key_file_to_data (key_file, &len, error);
    if (data == NULL)
    {
        return FALSE;
    }
    file = g_file_new_for_uri (uri);
    if (!g_file_replace_contents (file,
                                  data, len,
                                  NULL, FALSE,
                                  G_FILE_CREATE_NONE,
                                  NULL, NULL, error))
    {
        g_object_unref (file);
        g_free (data);
        return FALSE;
    }
    g_object_unref (file);
    g_free (data);
    return TRUE;
}

static GKeyFile *
_g_key_file_new_from_file (GFile *file,
                           GKeyFileFlags flags,
                           GError **error)
{
    GKeyFile *key_file;

    key_file = g_key_file_new ();
    if (!_g_key_file_load_from_gfile (key_file, file, flags, error))
    {
        g_key_file_free (key_file);
        key_file = NULL;
    }
    return key_file;
}

static GKeyFile *
_g_key_file_new_from_uri (const char *uri,
                          GKeyFileFlags flags,
                          GError **error)
{
    GKeyFile *key_file;
    GFile *file;

    file = g_file_new_for_uri (uri);
    key_file = _g_key_file_new_from_file (file, flags, error);
    g_object_unref (file);
    return key_file;
}

static ItemEntry *
item_entry_new (const char *field,
                const char *description,
                gboolean localized,
                gboolean filename)
{
    ItemEntry *entry;

    entry = g_new0 (ItemEntry, 1);
    entry->field = field;
    entry->description = description;
    entry->localized = localized;
    entry->filename = filename;

    return entry;
}

static void
item_entry_free (ItemEntry *entry)
{
    g_free (entry->current_value);
    g_free (entry);
}

static void
fm_ditem_page_url_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                      int x, int y,
                                      GtkSelectionData *selection_data,
                                      guint info, guint time,
                                      GtkEntry *entry)
{
    char **uris;
    gboolean exactly_one;
    char *path;

    uris = g_strsplit (gtk_selection_data_get_data (selection_data), "\r\n", 0);
    exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

    if (!exactly_one)
    {
        g_strfreev (uris);
        return;
    }

    path = g_filename_from_uri (uris[0], NULL, NULL);
    if (path != NULL)
    {
        gtk_entry_set_text (entry, path);
        g_free (path);
    }
    else
    {
        gtk_entry_set_text (entry, uris[0]);
    }

    g_strfreev (uris);
}

static void
fm_ditem_page_exec_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                       int x, int y,
                                       GtkSelectionData *selection_data,
                                       guint info, guint time,
                                       GtkEntry *entry)
{
    char **uris;
    gboolean exactly_one;
    CajaFile *file;
    GKeyFile *key_file;
    char *uri, *type, *exec;

    uris = g_strsplit (gtk_selection_data_get_data (selection_data), "\r\n", 0);
    exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

    if (!exactly_one)
    {
        g_strfreev (uris);
        return;
    }

    file = caja_file_get_by_uri (uris[0]);
    g_strfreev (uris);

    g_return_if_fail (file != NULL);

    uri = caja_file_get_uri (file);
    if (caja_file_is_mime_type (file, "application/x-desktop"))
    {
        key_file = _g_key_file_new_from_uri (uri, G_KEY_FILE_NONE, NULL);
        if (key_file != NULL)
        {
            type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
            if (type != NULL && strcmp (type, "Application") == 0)
            {
                exec = g_key_file_get_string (key_file, MAIN_GROUP, "Exec", NULL);
                if (exec != NULL)
                {
                    g_free (uri);
                    uri = exec;
                }
            }
            g_free (type);
            g_key_file_free (key_file);
        }
    }
    gtk_entry_set_text (entry,
                        uri?uri:"");
    gtk_widget_grab_focus (GTK_WIDGET (entry));

    g_free (uri);

    caja_file_unref (file);
}

static void
save_entry (GtkEntry *entry, GKeyFile *key_file, const char *uri)
{
    GError *error;
    ItemEntry *item_entry;
    const char *val;
    gchar **languages;

    item_entry = g_object_get_data (G_OBJECT (entry), "item_entry");
    val = gtk_entry_get_text (entry);

    if (strcmp (val, item_entry->current_value) == 0)
    {
        return; /* No actual change, don't update file */
    }

    g_free (item_entry->current_value);
    item_entry->current_value = g_strdup (val);

    if (item_entry->localized)
    {
        languages = (gchar **) g_get_language_names ();
        g_key_file_set_locale_string (key_file, MAIN_GROUP, item_entry->field, languages[0], val);
    }
    else
    {
        g_key_file_set_string (key_file, MAIN_GROUP, item_entry->field, val);
    }

    error = NULL;

    if (!_g_key_file_save_to_uri (key_file, uri, &error))
    {
        g_warning ("%s", error->message);
        g_error_free (error);
    }
}

static void
entry_activate_cb (GtkWidget *entry,
                   GtkWidget *container)
{
    const char *uri;
    GKeyFile *key_file;

    uri = g_object_get_data (G_OBJECT (container), "uri");
    key_file = g_object_get_data (G_OBJECT (container), "keyfile");
    save_entry (GTK_ENTRY (entry), key_file, uri);
}

static gboolean
entry_focus_out_cb (GtkWidget *entry,
                    GdkEventFocus *event,
                    GtkWidget *container)
{
    const char *uri;
    GKeyFile *key_file;

    uri = g_object_get_data (G_OBJECT (container), "uri");
    key_file = g_object_get_data (G_OBJECT (container), "keyfile");
    save_entry (GTK_ENTRY (entry), key_file, uri);
    return FALSE;
}

static GtkWidget *
#if GTK_CHECK_VERSION (3, 0, 0)
build_grid (GtkWidget *container,
#else
build_table (GtkWidget *container,
#endif
             GKeyFile *key_file,
             GtkSizeGroup *label_size_group,
             GList *entries)
{
#if GTK_CHECK_VERSION (3, 0, 0)
	GtkWidget *grid;
#else
    GtkWidget *table;
#endif
    GtkWidget *label;
    GtkWidget *entry;
    GList *l;
    char *val;
#if GTK_CHECK_VERSION (3, 0, 0)

   grid = gtk_grid_new ();
   gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
   gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
   gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
#else
    int i;

    table = gtk_table_new (g_list_length (entries) + 1, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_table_set_col_spacings (GTK_TABLE (table), 12);
    i = 0;
#endif

    for (l = entries; l; l = l->next)
    {
        ItemEntry *item_entry = (ItemEntry *)l->data;
        char *label_text;

        label_text = g_strdup_printf ("%s:", item_entry->description);
        label = gtk_label_new (label_text);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        g_free (label_text);
#if GTK_CHECK_VERSION (3, 16, 0)
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
#else
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
#endif
        gtk_size_group_add_widget (label_size_group, label);

        entry = gtk_entry_new ();
#if GTK_CHECK_VERSION (3, 0, 0)
        gtk_widget_set_hexpand (entry, TRUE);
#endif

        if (item_entry->localized)
        {
            val = g_key_file_get_locale_string (key_file,
                                                MAIN_GROUP,
                                                item_entry->field,
                                                NULL, NULL);
        }
        else
        {
            val = g_key_file_get_string (key_file,
                                         MAIN_GROUP,
                                         item_entry->field,
                                         NULL);
        }

        item_entry->current_value = g_strdup (val?val:"");
        gtk_entry_set_text (GTK_ENTRY (entry), item_entry->current_value);
        g_free (val);

#if GTK_CHECK_VERSION (3, 0, 0)
        gtk_container_add (GTK_CONTAINER (grid), label);
        gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
                                  GTK_POS_RIGHT, 1, 1);
#else
        gtk_table_attach (GTK_TABLE (table), label,
                          0, 1, i, i+1, GTK_FILL, GTK_FILL,
                          0, 0);
        gtk_table_attach (GTK_TABLE (table), entry,
                          1, 2, i, i+1, GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL,
                          0, 0);
#endif

        g_signal_connect (entry, "activate",
                          G_CALLBACK (entry_activate_cb),
                          container);
        g_signal_connect (entry, "focus_out_event",
                          G_CALLBACK (entry_focus_out_cb),
                          container);

        g_object_set_data_full (G_OBJECT (entry), "item_entry", item_entry,
                                (GDestroyNotify)item_entry_free);

        if (item_entry->filename)
        {
            gtk_drag_dest_set (GTK_WIDGET (entry),
                               GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                               target_table, G_N_ELEMENTS (target_table),
                               GDK_ACTION_COPY | GDK_ACTION_MOVE);

            g_signal_connect (entry, "drag_data_received",
                              G_CALLBACK (fm_ditem_page_url_drag_data_received),
                              entry);
        }
        else if (strcmp (item_entry->field, "Exec") == 0)
        {
            gtk_drag_dest_set (GTK_WIDGET (entry),
                               GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
                               target_table, G_N_ELEMENTS (target_table),
                               GDK_ACTION_COPY | GDK_ACTION_MOVE);

            g_signal_connect (entry, "drag_data_received",
                              G_CALLBACK (fm_ditem_page_exec_drag_data_received),
                              entry);
        }

#if !GTK_CHECK_VERSION (3, 0, 0)
        i++;
#endif
    }

    /* append dummy row */
    label = gtk_label_new ("");
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_container_add (GTK_CONTAINER (grid), label);
    gtk_size_group_add_widget (label_size_group, label);

    gtk_widget_show_all (grid);
    return grid;
#else
    gtk_table_attach (GTK_TABLE (table), label,
                      0, 1, i, i+1, GTK_FILL, GTK_FILL,
                      0, 0);
    gtk_size_group_add_widget (label_size_group, label);

    gtk_widget_show_all (table);
    return table;
#endif
}

static void
create_page (GKeyFile *key_file, GtkWidget *box)
{
#if GTK_CHECK_VERSION (3, 0, 0)
    GtkWidget *grid;
#else
    GtkWidget *table;
#endif
    GList *entries;
    GtkSizeGroup *label_size_group;
    char *type;

    entries = NULL;

    type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);

    if (g_strcmp0 (type, "Link") == 0)
    {
        entries = g_list_prepend (entries,
                                  item_entry_new ("Comment",
                                          _("Comment"), TRUE, FALSE));
        entries = g_list_prepend (entries,
                                  item_entry_new ("URL",
                                          _("URL"), FALSE, TRUE));
        entries = g_list_prepend (entries,
                                  item_entry_new ("GenericName",
                                          _("Description"), TRUE, FALSE));
    }
    else if (g_strcmp0 (type, "Application") == 0)
    {
        entries = g_list_prepend (entries,
                                  item_entry_new ("Comment",
                                          _("Comment"), TRUE, FALSE));
        entries = g_list_prepend (entries,
                                  item_entry_new ("Exec",
                                          _("Command"), FALSE, FALSE));
        entries = g_list_prepend (entries,
                                  item_entry_new ("GenericName",
                                          _("Description"), TRUE, FALSE));
    }
    else
    {
        /* we only handle launchers and links */

        /* ensure that we build an empty table with a dummy row at the end */
#if GTK_CHECK_VERSION (3, 0, 0)
        goto build_grid;
#else
        goto build_table;
#endif
    }
    g_free (type);

#if GTK_CHECK_VERSION (3, 0, 0)
build_grid:
    label_size_group = g_object_get_data (G_OBJECT (box), "label-size-group");

    grid = build_grid (box, key_file, label_size_group, entries);
    g_list_free (entries);

    gtk_box_pack_start (GTK_BOX (box), grid, FALSE, TRUE, 0);
#else
build_table:
    label_size_group = g_object_get_data (G_OBJECT (box), "label-size-group");

    table = build_table (box, key_file, label_size_group, entries);
    g_list_free (entries);

    gtk_box_pack_start (GTK_BOX (box), table, FALSE, TRUE, 0);
#endif
    gtk_widget_show_all (GTK_WIDGET (box));
}


static void
ditem_read_cb (GObject *source_object,
               GAsyncResult *res,
               gpointer user_data)
{
    GKeyFile *key_file;
    GtkWidget *box;
    gsize file_size;
    char *file_contents;

    box = GTK_WIDGET (user_data);

    if (g_file_load_contents_finish (G_FILE (source_object),
                                     res,
                                     &file_contents, &file_size,
                                     NULL, NULL))
    {
        key_file = g_key_file_new ();
        g_object_set_data_full (G_OBJECT (box), "keyfile", key_file, (GDestroyNotify)g_key_file_free);
        if (g_key_file_load_from_data (key_file, file_contents, file_size, 0, NULL))
        {
            create_page (key_file, box);
        }
        g_free (file_contents);

    }
    g_object_unref (box);
}

static void
fm_ditem_page_create_begin (const char *uri,
                            GtkWidget *box)
{
    GFile *location;

    location = g_file_new_for_uri (uri);
    g_object_set_data_full (G_OBJECT (box), "uri", g_strdup (uri), g_free);
    g_file_load_contents_async (location, NULL, ditem_read_cb, g_object_ref (box));
    g_object_unref (location);
}

GtkWidget *
fm_ditem_page_make_box (GtkSizeGroup *label_size_group,
                        GList *files)
{
    CajaFileInfo *info;
    char *uri;
    GtkWidget *box;

    g_assert (fm_ditem_page_should_show (files));

    box = gtk_vbox_new (FALSE, 6);
    g_object_set_data_full (G_OBJECT (box), "label-size-group",
                            label_size_group, (GDestroyNotify) g_object_unref);

    info = CAJA_FILE_INFO (files->data);

    uri = caja_file_info_get_uri (info);
    fm_ditem_page_create_begin (uri, box);
    g_free (uri);

    return box;
}

gboolean
fm_ditem_page_should_show (GList *files)
{
    CajaFileInfo *info;

    if (!files || files->next)
    {
        return FALSE;
    }

    info = CAJA_FILE_INFO (files->data);

    if (!caja_file_info_is_mime_type (info, "application/x-desktop"))
    {
        return FALSE;
    }

    return TRUE;
}

