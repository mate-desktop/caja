/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-file-management-properties.c - Functions to create and show the caja preference dialog.

   Copyright (C) 2002 Jan Arne Petersen

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#include <config.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <cairo-gobject.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>

#include <libcaja-private/caja-column-chooser.h>
#include <libcaja-private/caja-column-utilities.h>
#include <libcaja-private/caja-extensions.h>
#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-module.h>
#include <libcaja-private/caja-autorun.h>

#include <libcaja-extension/caja-configurable.h>

#include "caja-file-management-properties.h"

/* string enum preferences */
#define CAJA_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET "default_view_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET "icon_view_zoom_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_COMPACT_VIEW_ZOOM_WIDGET "compact_view_zoom_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET "list_view_zoom_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET "sort_order_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET "date_format_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET "preview_text_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET "preview_image_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET "preview_sound_combobox"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET "preview_folder_combobox"

/* bool preferences */
#define CAJA_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET "sort_folders_first_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET "compact_layout_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_LABELS_BESIDE_ICONS_WIDGET "labels_beside_icons_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_ALL_COLUMNS_SAME_WIDTH "all_columns_same_width_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_ALWAYS_USE_BROWSER_WIDGET "always_use_browser_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET "trash_confirm_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_TRASH_WIDGET "trash_confirm_trash_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET "trash_delete_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET "hidden_files_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_BACKUP_WIDGET "backup_files_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET "treeview_folders_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTOMOUNT_OPEN "media_automount_open_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTORUN_NEVER "media_autorun_never_checkbutton"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_USE_IEC_UNITS_WIDGET "use_iec_units"
#define CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_ICONS_IN_LIST_VIEW "show_icons_in_list_view"

/* int enums */
#define CAJA_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET "preview_image_size_combobox"

static const char * const default_view_values[] =
{
    "icon-view",
    "list-view",
    "compact-view",
    NULL
};

static const char * const zoom_values[] =
{
    "smallest",
    "smaller",
    "small",
    "standard",
    "large",
    "larger",
    "largest",
    NULL
};

/*
 * This array corresponds to the object with id "model2" in
 * caja-file-management-properties.ui. It has to positionally match with it.
 * The purpose is to map values from a combo box to values of the gsettings
 * enum.
 */
static const char * const sort_order_values[] =
{
    "name",
    "directory",
    "size",
    "size_on_disk",
    "type",
    "mtime",
    "atime",
    "emblems",
    "extension",
    "trash-time",
    NULL
};

static const char * const date_format_values[] =
{
    "locale",
    "iso",
    "informal",
    NULL
};

static const char * const preview_values[] =
{
    "always",
    "local-only",
    "never",
    NULL
};

static const char * const click_behavior_components[] =
{
    "single_click_radiobutton",
    "double_click_radiobutton",
    NULL
};

static const char * const click_behavior_values[] =
{
    "single",
    "double",
    NULL
};

static const char * const executable_text_components[] =
{
    "scripts_execute_radiobutton",
    "scripts_view_radiobutton",
    "scripts_confirm_radiobutton",
    NULL
};

static const char * const executable_text_values[] =
{
    "launch",
    "display",
    "ask",
    NULL
};

static const guint64 thumbnail_limit_values[] =
{
    102400,
    512000,
    1048576,
    3145728,
    5242880,
    10485760,
    104857600,
    1073741824,
    2147483648U,
    4294967295U
};

static const char * const icon_captions_components[] =
{
    "captions_0_combobox",
    "captions_1_combobox",
    "captions_2_combobox",
    NULL
};

enum
{
	EXT_STATE_COLUMN,
	EXT_ICON_COLUMN,
	EXT_INFO_COLUMN,
	EXT_STRUCT_COLUMN
};

static void caja_file_management_properties_dialog_update_media_sensitivity (GtkBuilder *builder);

static void
preferences_show_help (GtkWindow *parent,
                       char const *helpfile,
                       char const *sect_id)
{
    GError *error = NULL;
    char *help_string;

    g_assert (helpfile != NULL);
    g_assert (sect_id != NULL);

    help_string = g_strdup_printf ("help:%s/%s", helpfile, sect_id);

    gtk_show_uri_on_window (parent,
                            help_string, gtk_get_current_event_time (),
                            &error);
    g_free (help_string);

    if (error)
    {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_OK,
                                         _("There was an error displaying help: \n%s"),
                                         error->message);

        g_signal_connect (G_OBJECT (dialog),
                          "response", G_CALLBACK (gtk_widget_destroy),
                          NULL);
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_widget_show (dialog);
        g_error_free (error);
    }
}


static void
caja_file_management_properties_dialog_response_cb (GtkDialog *parent,
        int response_id,
        GtkBuilder *builder)
{
    if (response_id == GTK_RESPONSE_HELP)
    {
        char *section;

        switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (gtk_builder_get_object (builder, "notebook1"))))
        {
        default:
        case 0:
            section = "goscaja-438";
            break;
        case 1:
            section = "goscaja-56";
            break;
        case 2:
            section = "goscaja-439";
            break;
        case 3:
            section = "goscaja-490";
            break;
        case 4:
            section = "goscaja-60";
            break;
        case 5:
            section = "goscaja-61";
            break;
        }
        preferences_show_help (GTK_WINDOW (parent), "mate-user-guide", section);
    }
    else if (response_id == GTK_RESPONSE_CLOSE)
    {
        g_signal_handlers_disconnect_by_func (caja_media_preferences,
                                              caja_file_management_properties_dialog_update_media_sensitivity,
                                              builder);
    }
}

static void
columns_changed_callback (CajaColumnChooser *chooser,
                          gpointer callback_data)
{
    char **visible_columns;
    char **column_order;

    caja_column_chooser_get_settings (CAJA_COLUMN_CHOOSER (chooser),
                                      &visible_columns,
                                      &column_order);

    g_settings_set_strv (caja_list_view_preferences,
                         CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
                         (const char * const *)visible_columns);
    g_settings_set_strv (caja_list_view_preferences,
                         CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
                         (const char * const *)column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
free_column_names_array (GPtrArray *column_names)
{
    g_ptr_array_foreach (column_names, (GFunc) g_free, NULL);
    g_ptr_array_free (column_names, TRUE);
}

static void
create_icon_caption_combo_box_items (GtkComboBoxText *combo_box,
                                     GList *columns)
{
    GList *l;
    GPtrArray *column_names;

    column_names = g_ptr_array_new ();

    /* Translators: this is referred to captions under icons. */
    gtk_combo_box_text_append_text (combo_box, _("None"));
    g_ptr_array_add (column_names, g_strdup ("none"));

    for (l = columns; l != NULL; l = l->next)
    {
        CajaColumn *column;
        char *name;
        char *label;

        column = CAJA_COLUMN (l->data);

        g_object_get (G_OBJECT (column),
                      "name", &name, "label", &label,
                      NULL);

        /* Don't show name here, it doesn't make sense */
        if (!strcmp (name, "name"))
        {
            g_free (name);
            g_free (label);
            continue;
        }

        gtk_combo_box_text_append_text (combo_box, label);
        g_ptr_array_add (column_names, name);

        g_free (label);
    }
    g_object_set_data_full (G_OBJECT (combo_box), "column_names",
                            column_names,
                            (GDestroyNotify) free_column_names_array);
}

static void
icon_captions_changed_callback (GtkComboBox *combo_box,
                                gpointer user_data)
{
    GPtrArray *captions;
    GtkBuilder *builder;
    int i;

    builder = GTK_BUILDER (user_data);

    captions = g_ptr_array_new ();

    for (i = 0; icon_captions_components[i] != NULL; i++)
    {
        GtkWidget *combo_box;
        int active;
        GPtrArray *column_names;
        char *name;

        combo_box = GTK_WIDGET (gtk_builder_get_object
                                (builder, icon_captions_components[i]));
        active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

        column_names = g_object_get_data (G_OBJECT (combo_box),
                                          "column_names");

        name = g_ptr_array_index (column_names, active);
        g_ptr_array_add (captions, name);
    }
    g_ptr_array_add (captions, NULL);

    g_settings_set_strv (caja_icon_view_preferences,
                         CAJA_PREFERENCES_ICON_VIEW_CAPTIONS,
                         (const char **)captions->pdata);
    g_ptr_array_free (captions, TRUE);
}

static void
update_caption_combo_box (GtkBuilder *builder,
                          const char *combo_box_name,
                          const char *name)
{
    GtkWidget *combo_box;
    int i;
    GPtrArray *column_names;

    combo_box = GTK_WIDGET (gtk_builder_get_object (builder, combo_box_name));

    g_signal_handlers_block_by_func
    (combo_box,
     G_CALLBACK (icon_captions_changed_callback),
     builder);

    column_names = g_object_get_data (G_OBJECT (combo_box),
                                      "column_names");

    for (i = 0; i < column_names->len; ++i)
    {
        if (!strcmp (name, g_ptr_array_index (column_names, i)))
        {
            gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), i);
            break;
        }
    }

    g_signal_handlers_unblock_by_func
    (combo_box,
     G_CALLBACK (icon_captions_changed_callback),
     builder);
}

static void
update_icon_captions_from_settings (GtkBuilder *builder)
{
    char **captions;
    int i, j;

    captions = g_settings_get_strv (caja_icon_view_preferences, CAJA_PREFERENCES_ICON_VIEW_CAPTIONS);
    if (captions == NULL)
        return;

    for (i = 0, j = 0;
            icon_captions_components[i] != NULL;
            i++)
    {
        char *data;

        if (captions[j])
        {
            data = captions[j];
            ++j;
        }
        else
        {
            data = "none";
        }

        update_caption_combo_box (builder,
                                  icon_captions_components[i],
                                  data);
    }

    g_strfreev (captions);
}

static void
caja_file_management_properties_dialog_setup_icon_caption_page (GtkBuilder *builder)
{
    GList *columns;
    int i;
    gboolean writable;

    writable = g_settings_is_writable (caja_icon_view_preferences, CAJA_PREFERENCES_ICON_VIEW_CAPTIONS);

    columns = caja_get_common_columns ();

    for (i = 0; icon_captions_components[i] != NULL; i++)
    {
        GtkWidget *combo_box;

        combo_box = GTK_WIDGET (gtk_builder_get_object (builder,
                                icon_captions_components[i]));

        create_icon_caption_combo_box_items (GTK_COMBO_BOX_TEXT (combo_box), columns);
        gtk_widget_set_sensitive (combo_box, writable);

        g_signal_connect (combo_box, "changed",
                          G_CALLBACK (icon_captions_changed_callback),
                          builder);
    }

    caja_column_list_free (columns);

    update_icon_captions_from_settings (builder);
}

static void
create_date_format_menu (GtkBuilder *builder)
{
    GtkComboBoxText *combo_box;
    gchar *date_string;
    GDateTime *now;

    combo_box = GTK_COMBO_BOX_TEXT
            (gtk_builder_get_object (builder,
                                     CAJA_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET));

    now = g_date_time_new_now_local ();

    date_string = g_date_time_format (now, "%c");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), date_string);
    g_free (date_string);

    date_string = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), date_string);
    g_free (date_string);

    date_string = g_date_time_format (now, _("today at %-I:%M:%S %p"));
    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), date_string);
    g_free (date_string);

    g_date_time_unref (now);
}

static void
set_columns_from_settings (CajaColumnChooser *chooser)
{
    char **visible_columns;
    char **column_order;

    visible_columns = g_settings_get_strv (caja_list_view_preferences,
                                           CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
    column_order = g_settings_get_strv (caja_list_view_preferences,
                                        CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

    caja_column_chooser_set_settings (CAJA_COLUMN_CHOOSER (chooser),
                                      visible_columns,
                                      column_order);

    g_strfreev (visible_columns);
    g_strfreev (column_order);
}

static void
use_default_callback (CajaColumnChooser *chooser,
                      gpointer user_data)
{
    g_settings_reset (caja_list_view_preferences,
                      CAJA_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
    g_settings_reset (caja_list_view_preferences,
                      CAJA_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
    set_columns_from_settings (chooser);
}

static void
caja_file_management_properties_dialog_setup_list_column_page (GtkBuilder *builder)
{
    GtkWidget *chooser;
    GtkWidget *box;

    chooser = caja_column_chooser_new (NULL);
    g_signal_connect (chooser, "changed",
                      G_CALLBACK (columns_changed_callback), chooser);
    g_signal_connect (chooser, "use_default",
                      G_CALLBACK (use_default_callback), chooser);

    set_columns_from_settings (CAJA_COLUMN_CHOOSER (chooser));

    gtk_widget_show (chooser);
    box = GTK_WIDGET (gtk_builder_get_object (builder, "list_columns_vbox"));

    gtk_box_pack_start (GTK_BOX (box), chooser, TRUE, TRUE, 0);
}

static void
caja_file_management_properties_dialog_update_media_sensitivity (GtkBuilder *builder)
{
    gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (builder, "media_handling_vbox")),
                              ! g_settings_get_boolean (caja_media_preferences, CAJA_PREFERENCES_MEDIA_AUTORUN_NEVER));
}

static void
other_type_combo_box_changed (GtkComboBox *combo_box, GtkComboBox *action_combo_box)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    char *x_content_type;

    x_content_type = NULL;

    if (!gtk_combo_box_get_active_iter (combo_box, &iter))
    {
        goto out;
    }

    model = gtk_combo_box_get_model (combo_box);
    if (model == NULL)
    {
        goto out;
    }

    gtk_tree_model_get (model, &iter,
                        2, &x_content_type,
                        -1);

    caja_autorun_prepare_combo_box (GTK_WIDGET (action_combo_box),
                                    x_content_type,
                                    TRUE,
                                    TRUE,
                                    TRUE,
                                    NULL, NULL);
out:
    g_free (x_content_type);
}

static gulong extension_about_id = 0;
static gulong extension_configure_id = 0;

static void
extension_about_clicked (GtkButton *button, Extension *ext)
{
    GtkAboutDialog *extension_about_dialog;

    extension_about_dialog = (GtkAboutDialog *) gtk_about_dialog_new();
    gtk_about_dialog_set_program_name (extension_about_dialog, ext->name != NULL ? ext->name : ext->filename);
    gtk_about_dialog_set_comments (extension_about_dialog, ext->description);
    gtk_about_dialog_set_logo_icon_name (extension_about_dialog, ext->icon != NULL ? ext->icon : "system-run");
    gtk_about_dialog_set_copyright (extension_about_dialog, ext->copyright);
    gtk_about_dialog_set_authors (extension_about_dialog, (const gchar **) ext->author);
    gtk_about_dialog_set_version (extension_about_dialog, ext->version);
    gtk_about_dialog_set_website (extension_about_dialog, ext->website);
    gtk_window_set_title (GTK_WINDOW(extension_about_dialog), _("About Extension"));
    gtk_window_set_icon_name (GTK_WINDOW(extension_about_dialog), ext->icon != NULL ? ext->icon : "system-run");
    gtk_dialog_run (GTK_DIALOG (extension_about_dialog));
    gtk_widget_destroy (GTK_WIDGET (extension_about_dialog));
}

static int extension_configure_check (Extension *ext)
{
    if (!ext->state) // For now, only allow configuring enabled extensions.
    {
        return 0;
    }
    if (!CAJA_IS_CONFIGURABLE(ext->module))
    {
        return 0;
    }
    return 1;
}

static void
extension_configure_clicked (GtkButton *button, Extension *ext)
{
    if (extension_configure_check(ext)) {
        caja_configurable_run_config(CAJA_CONFIGURABLE(ext->module));
    }
}

static void
extension_list_selection_changed_about (GtkTreeSelection *selection, GtkButton *about_button)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    Extension *ext;

    gtk_widget_set_sensitive (GTK_WIDGET (about_button), FALSE);

    if (extension_about_id > 0)
    {
        g_signal_handler_disconnect (about_button, extension_about_id);
        extension_about_id = 0;
    }

    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    gtk_tree_model_get (model, &iter, EXT_STRUCT_COLUMN, &ext, -1);
    if (ext != NULL) {
        gtk_widget_set_sensitive (GTK_WIDGET (about_button), TRUE);
        extension_about_id = g_signal_connect (about_button, "clicked", G_CALLBACK (extension_about_clicked), ext);
    }
}

static void
extension_list_selection_changed_configure (GtkTreeSelection *selection, GtkButton *configure_button)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    Extension *ext;

    gtk_widget_set_sensitive (GTK_WIDGET (configure_button), FALSE);

    if (extension_configure_id > 0)
    {
        g_signal_handler_disconnect (configure_button, extension_configure_id);
        extension_configure_id = 0;
    }

    if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

    gtk_tree_model_get (model, &iter, EXT_STRUCT_COLUMN, &ext, -1);
    if (ext != NULL) {
        // Unconfigurable extensions remain unconfigurable.
        if (extension_configure_check(ext)) {
            gtk_widget_set_sensitive (GTK_WIDGET (configure_button), TRUE);
            extension_configure_id = g_signal_connect (configure_button, "clicked", G_CALLBACK (extension_configure_clicked), ext);
        }
    }
}

static void
extension_state_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    gboolean new_state;
    Extension *ext;

    path = gtk_tree_path_new_from_string (path_str);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (data));

    g_object_get (G_OBJECT (cell), "active", &new_state, NULL);
    new_state ^= 1;

    if (gtk_tree_model_get_iter_from_string (model, &iter, path_str))
    {
        gtk_tree_model_get (model, &iter, EXT_STRUCT_COLUMN, &ext, -1);

        if (caja_extension_set_state (ext, new_state))
        {
            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                EXT_STATE_COLUMN, new_state, -1);
        }
    }
    gtk_tree_path_free (path);
}


static void
caja_file_management_properties_dialog_setup_media_page (GtkBuilder *builder)
{
    unsigned int n;
    GList *l;
    GList *content_types;
    GtkWidget *other_type_combo_box;
    GtkListStore *other_type_list_store;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    const char *s[] = {"media_audio_cdda_combobox",   "x-content/audio-cdda",
                       "media_video_dvd_combobox",    "x-content/video-dvd",
                       "media_music_player_combobox", "x-content/audio-player",
                       "media_dcf_combobox",          "x-content/image-dcf",
                       "media_software_combobox",     "x-content/software",
                       NULL
                      };

    for (n = 0; s[n*2] != NULL; n++)
    {
        caja_autorun_prepare_combo_box (GTK_WIDGET (gtk_builder_get_object (builder, s[n*2])), s[n*2 + 1],
                                        TRUE, TRUE, TRUE, NULL, NULL);
    }

    other_type_combo_box = GTK_WIDGET (gtk_builder_get_object (builder, "media_other_type_combobox"));

    other_type_list_store = gtk_list_store_new (3,
                            CAIRO_GOBJECT_TYPE_SURFACE,
                            G_TYPE_STRING,
                            G_TYPE_STRING);

    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (other_type_list_store),
                                          1, GTK_SORT_ASCENDING);


    content_types = g_content_types_get_registered ();

    for (l = content_types; l != NULL; l = l->next)
    {
        char *content_type = l->data;
        char *description;
        GIcon *icon;
        CajaIconInfo *icon_info;
        cairo_surface_t *surface;
        int icon_size, icon_scale;

        if (!g_str_has_prefix (content_type, "x-content/"))
            continue;
        for (n = 0; s[n*2] != NULL; n++)
        {
            if (strcmp (content_type, s[n*2 + 1]) == 0)
            {
                goto skip;
            }
        }

        icon_size = caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
        icon_scale = gtk_widget_get_scale_factor (other_type_combo_box);

        description = g_content_type_get_description (content_type);
        gtk_list_store_append (other_type_list_store, &iter);
        icon = g_content_type_get_icon (content_type);
        if (icon != NULL)
        {
            icon_info = caja_icon_info_lookup (icon, icon_size, icon_scale);
            g_object_unref (icon);
            surface = caja_icon_info_get_surface_nodefault_at_size (icon_info, icon_size);
            g_object_unref (icon_info);
        }
        else
        {
            surface = NULL;
        }

        gtk_list_store_set (other_type_list_store, &iter,
                            0, surface,
                            1, description,
                            2, content_type,
                            -1);
        if (surface != NULL)
            cairo_surface_destroy (surface);
        g_free (description);
skip:
        ;
    }
    g_list_foreach (content_types, (GFunc) g_free, NULL);
    g_list_free (content_types);

    gtk_combo_box_set_model (GTK_COMBO_BOX (other_type_combo_box), GTK_TREE_MODEL (other_type_list_store));

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                    "surface", 0,
                                    NULL);
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                    "text", 1,
                                    NULL);

    g_signal_connect (G_OBJECT (other_type_combo_box),
                      "changed",
                      G_CALLBACK (other_type_combo_box_changed),
                      gtk_builder_get_object (builder, "media_other_action_combobox"));

    gtk_combo_box_set_active (GTK_COMBO_BOX (other_type_combo_box), 0);

    caja_file_management_properties_dialog_update_media_sensitivity (builder);
}

static void
caja_file_management_properties_dialog_setup_extension_page (GtkBuilder *builder)
{
    GtkCellRendererToggle *toggle;
    GtkListStore *store;
    GtkTreeView *view;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkIconTheme *icon_theme;
    cairo_surface_t *ext_surface_icon;
    GtkButton *about_button, *configure_button;
    gchar *ext_text_info;

    GList *extensions;
    int i;

    extensions = caja_extensions_get_list ();

    view = GTK_TREE_VIEW (
                    gtk_builder_get_object (builder, "extension_view"));
    store = GTK_LIST_STORE (
                    gtk_builder_get_object (builder, "extension_store"));

    toggle = GTK_CELL_RENDERER_TOGGLE (
                    gtk_builder_get_object (builder, "extension_toggle"));
    g_object_set (toggle, "xpad", 6, NULL);

    g_signal_connect (toggle, "toggled",
                      G_CALLBACK (extension_state_toggled), view);

    icon_theme = gtk_icon_theme_get_default();

    for (i = 0; i < g_list_length (extensions); i++)
    {
        Extension* ext = EXTENSION (g_list_nth_data (extensions, i));

        if (ext->icon != NULL)
        {
            ext_surface_icon = gtk_icon_theme_load_surface (icon_theme, ext->icon,
                                                            24, gtk_widget_get_scale_factor (GTK_WIDGET (view)),
                                                            NULL, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
        }
        else
        {
            ext_surface_icon = gtk_icon_theme_load_surface (icon_theme, "system-run",
                                                            24, gtk_widget_get_scale_factor (GTK_WIDGET (view)),
                                                            NULL, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
        }

        if (ext->description != NULL)
        {
            ext_text_info = g_markup_printf_escaped ("<b>%s</b>\n%s",
                                                     ext->name ? ext->name : ext->filename,
                                                     ext->description);
        }
        else
        {
            ext_text_info = g_markup_printf_escaped ("<b>%s</b>",
                                                     ext->name ? ext->name : ext->filename);
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            EXT_STATE_COLUMN, ext->state,
                            EXT_ICON_COLUMN, ext_surface_icon,
                            EXT_INFO_COLUMN, ext_text_info,
                            EXT_STRUCT_COLUMN, ext, -1);

        g_free (ext_text_info);
        if (ext_surface_icon)
            cairo_surface_destroy (ext_surface_icon);
    }

    about_button = GTK_BUTTON (gtk_builder_get_object (builder, "about_extension_button"));
    configure_button = GTK_BUTTON (gtk_builder_get_object (builder, "configure_extension_button"));

    selection = gtk_tree_view_get_selection (view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (selection, "changed",
                      G_CALLBACK (extension_list_selection_changed_about),
                      about_button);
    g_signal_connect (selection, "changed",
                      G_CALLBACK (extension_list_selection_changed_configure),
                      configure_button);
}

static void
bind_builder_bool (GtkBuilder *builder,
                   GSettings *settings,
                   const char *widget_name,
                   const char *prefs)
{
    g_settings_bind (settings, prefs,
                     gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

static void
bind_builder_bool_inverted (GtkBuilder *builder,
                            GSettings *settings,
                            const char *widget_name,
                            const char *prefs)
{
    g_settings_bind (settings, prefs,
                      gtk_builder_get_object (builder, widget_name),
                      "active", G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static gboolean
enum_get_mapping (GValue             *value,
                  GVariant           *variant,
                  gpointer            user_data)
{
    const char **enum_values = user_data;
    const char *str;
    int i;

    str = g_variant_get_string (variant, NULL);
    for (i = 0; enum_values[i] != NULL; i++) {
        if (strcmp (enum_values[i], str) == 0) {
            g_value_set_int (value, i);
            return TRUE;
        }
    }

    return FALSE;
}

static GVariant *
enum_set_mapping (const GValue       *value,
                  const GVariantType *expected_type,
                  gpointer            user_data)
{
    const char **enum_values = user_data;

    return g_variant_new_string (enum_values[g_value_get_int (value)]);
}

static void
bind_builder_enum (GtkBuilder *builder,
                   GSettings *settings,
                   const char *widget_name,
                   const char *prefs,
                   const char **enum_values)
{
    g_settings_bind_with_mapping (settings, prefs,
                                  gtk_builder_get_object (builder, widget_name),
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  enum_get_mapping,
                                  enum_set_mapping,
                                  enum_values, NULL);
}

typedef struct {
    const guint64 *values;
    int n_values;
} UIntEnumBinding;

static gboolean
uint_enum_get_mapping (GValue             *value,
               GVariant           *variant,
               gpointer            user_data)
{
    UIntEnumBinding *binding = user_data;
    guint64 v;
    int i;

    v = g_variant_get_uint64 (variant);
    for (i = 0; i < binding->n_values; i++) {
        if (binding->values[i] >= v) {
            g_value_set_int (value, i);
            return TRUE;
        }
    }

    return FALSE;
}

static GVariant *
uint_enum_set_mapping (const GValue       *value,
               const GVariantType *expected_type,
               gpointer            user_data)
{
    UIntEnumBinding *binding = user_data;

    return g_variant_new_uint64 (binding->values[g_value_get_int (value)]);
}

static void
bind_builder_uint_enum (GtkBuilder *builder,
            GSettings *settings,
            const char *widget_name,
            const char *prefs,
            const guint64 *values,
            int n_values)
{
    UIntEnumBinding *binding;

    binding = g_new (UIntEnumBinding, 1);
    binding->values = values;
    binding->n_values = n_values;

    g_settings_bind_with_mapping (settings, prefs,
                      gtk_builder_get_object (builder, widget_name),
                      "active", G_SETTINGS_BIND_DEFAULT,
                      uint_enum_get_mapping,
                      uint_enum_set_mapping,
                      binding, g_free);
}

static GVariant *
radio_mapping_set (const GValue *gvalue,
                   const GVariantType *expected_type,
                   gpointer user_data)
{
    const gchar *widget_value = user_data;
    GVariant *retval = NULL;

    if (g_value_get_boolean (gvalue)) {
        retval = g_variant_new_string (widget_value);
    }
    return retval;
}

static gboolean
radio_mapping_get (GValue *gvalue,
                   GVariant *variant,
                   gpointer user_data)
{
    const gchar *widget_value = user_data;
    const gchar *value;
    value = g_variant_get_string (variant, NULL);

    if (g_strcmp0 (value, widget_value) == 0) {
        g_value_set_boolean (gvalue, TRUE);
    } else {
        g_value_set_boolean (gvalue, FALSE);
    }

    return TRUE;
 }

static void
bind_builder_radio (GtkBuilder *builder,
            GSettings *settings,
            const char **widget_names,
            const char *prefs,
            const char **values)
{
    int i;
    GtkWidget *button = NULL;

    for (i = 0; widget_names[i] != NULL; i++) {
        button = GTK_WIDGET (gtk_builder_get_object (builder, widget_names[i]));

        g_settings_bind_with_mapping (settings, prefs,
                                      button, "active",
                                      G_SETTINGS_BIND_DEFAULT,
                                      radio_mapping_get, radio_mapping_set,
                                      (gpointer) values[i], NULL);
    }
}

static  void
caja_file_management_properties_dialog_setup (GtkBuilder *builder, GtkWindow *window)
{
    GtkWidget *dialog;

    /* setup UI */
    create_date_format_menu (builder);

    /* setup preferences */
    bind_builder_bool (builder, caja_icon_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_COMPACT_LAYOUT_WIDGET,
                       CAJA_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT);
    bind_builder_bool (builder, caja_icon_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_LABELS_BESIDE_ICONS_WIDGET,
                       CAJA_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS);
    bind_builder_bool (builder, caja_compact_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_ALL_COLUMNS_SAME_WIDTH,
                       CAJA_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH);
    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET,
                       CAJA_PREFERENCES_SORT_DIRECTORIES_FIRST);
    bind_builder_bool_inverted (builder, caja_preferences,
                                CAJA_FILE_MANAGEMENT_PROPERTIES_ALWAYS_USE_BROWSER_WIDGET,
                                CAJA_PREFERENCES_ALWAYS_USE_BROWSER);

    bind_builder_bool (builder, caja_media_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTOMOUNT_OPEN,
                       CAJA_PREFERENCES_MEDIA_AUTOMOUNT_OPEN);
    bind_builder_bool (builder, caja_media_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_MEDIA_AUTORUN_NEVER,
                       CAJA_PREFERENCES_MEDIA_AUTORUN_NEVER);

    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET,
                       CAJA_PREFERENCES_CONFIRM_TRASH);
    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_TRASH_WIDGET,
                       CAJA_PREFERENCES_CONFIRM_MOVE_TO_TRASH);

    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET,
                       CAJA_PREFERENCES_ENABLE_DELETE);
    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
                       CAJA_PREFERENCES_SHOW_HIDDEN_FILES);
    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_BACKUP_WIDGET,
                       CAJA_PREFERENCES_SHOW_BACKUP_FILES);
    bind_builder_bool (builder, caja_tree_sidebar_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_TREE_VIEW_FOLDERS_WIDGET,
                       CAJA_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES);

    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_USE_IEC_UNITS_WIDGET,
                       CAJA_PREFERENCES_USE_IEC_UNITS);

    bind_builder_bool (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_SHOW_ICONS_IN_LIST_VIEW,
                       CAJA_PREFERENCES_SHOW_ICONS_IN_LIST_VIEW);

    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET,
                       CAJA_PREFERENCES_DEFAULT_FOLDER_VIEWER,
                       (const char **) default_view_values);
    bind_builder_enum (builder, caja_icon_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET,
                       CAJA_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                       (const char **) zoom_values);
    bind_builder_enum (builder, caja_compact_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_COMPACT_VIEW_ZOOM_WIDGET,
                       CAJA_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
                       (const char **) zoom_values);
    bind_builder_enum (builder, caja_list_view_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET,
                       CAJA_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
                       (const char **) zoom_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
                       CAJA_PREFERENCES_DEFAULT_SORT_ORDER,
                       (const char **) sort_order_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_TEXT_WIDGET,
                       CAJA_PREFERENCES_SHOW_TEXT_IN_ICONS,
                       (const char **) preview_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_IMAGE_WIDGET,
                       CAJA_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
                       (const char **) preview_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_SOUND_WIDGET,
                       CAJA_PREFERENCES_PREVIEW_SOUND,
                       (const char **) preview_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET,
                       CAJA_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                       (const char **) preview_values);
    bind_builder_enum (builder, caja_preferences,
                       CAJA_FILE_MANAGEMENT_PROPERTIES_DATE_FORMAT_WIDGET,
                       CAJA_PREFERENCES_DATE_FORMAT,
                       (const char **) date_format_values);

    bind_builder_radio (builder, caja_preferences,
                        (const char **) click_behavior_components,
                        CAJA_PREFERENCES_CLICK_POLICY,
                        (const char **) click_behavior_values);
    bind_builder_radio (builder, caja_preferences,
                        (const char **) executable_text_components,
                        CAJA_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
                        (const char **) executable_text_values);

    bind_builder_uint_enum (builder, caja_preferences,
                            CAJA_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET,
                            CAJA_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
                            thumbnail_limit_values,
                            G_N_ELEMENTS (thumbnail_limit_values));

    caja_file_management_properties_dialog_setup_icon_caption_page (builder);
    caja_file_management_properties_dialog_setup_list_column_page (builder);
    caja_file_management_properties_dialog_setup_media_page (builder);
    caja_file_management_properties_dialog_setup_extension_page (builder);

    g_signal_connect_swapped (caja_media_preferences,
                              "changed::" CAJA_PREFERENCES_MEDIA_AUTORUN_NEVER,
                              G_CALLBACK(caja_file_management_properties_dialog_update_media_sensitivity),
                              builder);


    /* UI callbacks */
    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "file_management_dialog"));
    g_signal_connect_data (G_OBJECT (dialog), "response",
                           G_CALLBACK (caja_file_management_properties_dialog_response_cb),
                           g_object_ref (builder),
                           (GClosureNotify)g_object_unref,
                           0);

    gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-file-manager");

    if (window)
    {
        gtk_window_set_screen (GTK_WINDOW (dialog), gtk_window_get_screen(window));
    }

    GtkWidget *notebook = GTK_WIDGET (gtk_builder_get_object (builder, "notebook1"));
    gtk_widget_add_events (notebook, GDK_SCROLL_MASK);
    g_signal_connect (notebook,
                      "scroll-event",
                      G_CALLBACK (eel_notebook_scroll_event_cb),
                      NULL);

    gtk_widget_show (dialog);
}

static gboolean
delete_event_callback (GtkWidget       *widget,
                       GdkEventAny     *event,
                       gpointer         data)
{
    void (*response_callback) (GtkDialog *dialog,
                               gint response_id);

    response_callback = data;

    response_callback (GTK_DIALOG (widget), GTK_RESPONSE_CLOSE);

    return TRUE;
}

void
caja_file_management_properties_dialog_show (GCallback close_callback, GtkWindow *window)
{
    GtkBuilder *builder;

    builder = gtk_builder_new ();

    gtk_builder_add_from_file (builder,
                               UIDIR "/caja-file-management-properties.ui",
                               NULL);

    g_signal_connect (G_OBJECT (gtk_builder_get_object (builder, "file_management_dialog")),
                      "response", close_callback, NULL);
    g_signal_connect (G_OBJECT (gtk_builder_get_object (builder, "file_management_dialog")),
                      "delete_event", G_CALLBACK (delete_event_callback), close_callback);

    caja_file_management_properties_dialog_setup (builder, window);

    g_object_unref (builder);
}
