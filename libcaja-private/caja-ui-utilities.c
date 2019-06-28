/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-ui-utilities.c - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

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

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

#include <eel/eel-debug.h>
#include <eel/eel-graphic-effects.h>

#include "caja-ui-utilities.h"
#include "caja-file-utilities.h"
#include "caja-icon-info.h"

void
caja_ui_unmerge_ui (GtkUIManager *ui_manager,
                    guint *merge_id,
                    GtkActionGroup **action_group)
{
    if (*merge_id != 0)
    {
        gtk_ui_manager_remove_ui (ui_manager,
                                  *merge_id);
        *merge_id = 0;
    }
    if (*action_group != NULL)
    {
        gtk_ui_manager_remove_action_group (ui_manager,
                                            *action_group);
        *action_group = NULL;
    }
}

void
caja_ui_prepare_merge_ui (GtkUIManager *ui_manager,
                          const char *name,
                          guint *merge_id,
                          GtkActionGroup **action_group)
{
    *merge_id = gtk_ui_manager_new_merge_id (ui_manager);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    *action_group = gtk_action_group_new (name);
    gtk_action_group_set_translation_domain (*action_group, GETTEXT_PACKAGE);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    gtk_ui_manager_insert_action_group (ui_manager, *action_group, 0);
    g_object_unref (*action_group); /* owned by ui manager */
}


char *
caja_get_ui_directory (void)
{
    return g_strdup (DATADIR "/caja/ui");
}

char *
caja_ui_file (const char *partial_path)
{
    char *path;

    path = g_build_filename (DATADIR "/caja/ui", partial_path, NULL);
    if (g_file_test (path, G_FILE_TEST_EXISTS))
    {
        return path;
    }
    g_free (path);
    return NULL;
}

const char *
caja_ui_string_get (const char *filename)
{
    static GHashTable *ui_cache = NULL;
    char *ui;

    if (ui_cache == NULL)
    {
        ui_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
        eel_debug_call_at_shutdown_with_data ((GFreeFunc)g_hash_table_destroy, ui_cache);
    }

    ui = g_hash_table_lookup (ui_cache, filename);
    if (ui == NULL)
    {
        char *path;

        path = caja_ui_file (filename);
        if (path == NULL || !g_file_get_contents (path, &ui, NULL, NULL))
        {
            g_warning ("Unable to load ui file %s\n", filename);
        }
        g_free (path);
        g_hash_table_insert (ui_cache,
                             g_strdup (filename),
                             ui);
    }

    return ui;
}

static void
extension_action_callback (GtkAction *action,
                           gpointer callback_data)
{
    caja_menu_item_activate (CAJA_MENU_ITEM (callback_data));
}

static void
extension_action_sensitive_callback (CajaMenuItem *item,
                                     GParamSpec *arg1,
                                     gpointer user_data)
{
    gboolean value;

    g_object_get (G_OBJECT (item),
                  "sensitive", &value,
                  NULL);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_action_set_sensitive (GTK_ACTION (user_data), value);
    G_GNUC_END_IGNORE_DEPRECATIONS;
}

static cairo_surface_t *
get_action_icon (const char *icon_name,
                 int         size,
                 GtkWidget  *parent_widget)
{
    CajaIconInfo *info;
    cairo_surface_t *surface;
    int scale;

    scale = gtk_widget_get_scale_factor (parent_widget);

    if (g_path_is_absolute (icon_name))
    {
        info = caja_icon_info_lookup_from_path (icon_name, size, scale);
    }
    else
    {
        info = caja_icon_info_lookup_from_name (icon_name, size, scale);
    }
    surface = caja_icon_info_get_surface_nodefault_at_size (info, size);
    g_object_unref (info);

    return surface;
}

GtkAction *
caja_action_from_menu_item (CajaMenuItem *item,
                            GtkWidget    *parent_widget)
{
    char *name, *label, *tip, *icon_name;
    gboolean sensitive, priority;
    GtkAction *action;

    g_object_get (G_OBJECT (item),
                  "name", &name, "label", &label,
                  "tip", &tip, "icon", &icon_name,
                  "sensitive", &sensitive,
                  "priority", &priority,
                  NULL);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_new (name,
                             label,
                             tip,
                             icon_name);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    if (icon_name != NULL)
    {
        cairo_surface_t *surface;

        surface = get_action_icon (icon_name,
                                   caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU),
                                   parent_widget);
        if (surface != NULL)
        {
            g_object_set_data_full (G_OBJECT (action), "menu-icon",
                                    surface,
                                    (GDestroyNotify)cairo_surface_destroy);
        }
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_action_set_sensitive (action, sensitive);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_object_set (action, "is-important", priority, NULL);

    g_signal_connect_data (action, "activate",
                           G_CALLBACK (extension_action_callback),
                           g_object_ref (item),
                           (GClosureNotify)g_object_unref, 0);

    g_free (name);
    g_free (label);
    g_free (tip);
    g_free (icon_name);

    return action;
}

GtkAction *
caja_toolbar_action_from_menu_item (CajaMenuItem *item, GtkWidget *parent_widget)
{
    char *name, *label, *tip, *icon_name;
    gboolean sensitive, priority;
    GtkAction *action;

    g_object_get (G_OBJECT (item),
                  "name", &name, "label", &label,
                  "tip", &tip, "icon", &icon_name,
                  "sensitive", &sensitive,
                  "priority", &priority,
                  NULL);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    action = gtk_action_new (name,
                             label,
                             tip,
                             icon_name);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    if (icon_name != NULL)
    {
        cairo_surface_t *surface;

        surface = get_action_icon (icon_name,
                                   caja_get_icon_size_for_stock_size (GTK_ICON_SIZE_LARGE_TOOLBAR),
                                   parent_widget);
        if (surface != NULL)
        {
            g_object_set_data_full (G_OBJECT (action), "toolbar-icon",
                                    surface,
                                    (GDestroyNotify)cairo_surface_destroy);
        }
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    gtk_action_set_sensitive (action, sensitive);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_object_set (action, "is-important", priority, NULL);

    g_signal_connect_data (action, "activate",
                           G_CALLBACK (extension_action_callback),
                           g_object_ref (item),
                           (GClosureNotify)g_object_unref, 0);

    g_signal_connect_object (item, "notify::sensitive",
                             G_CALLBACK (extension_action_sensitive_callback),
                             action,
                             0);

    g_free (name);
    g_free (label);
    g_free (tip);
    g_free (icon_name);

    return action;
}

static GdkPixbuf *
caja_get_thumbnail_frame (void)
{
    static GdkPixbuf *thumbnail_frame = NULL;

    if (thumbnail_frame == NULL)
    {
        char *image_path;

        image_path = caja_pixmap_file ("thumbnail_frame.png");
        if (image_path != NULL)
        {
            thumbnail_frame = gdk_pixbuf_new_from_file (image_path, NULL);
        }
        g_free (image_path);
    }

    return thumbnail_frame;
}

#define CAJA_THUMBNAIL_FRAME_LEFT 3
#define CAJA_THUMBNAIL_FRAME_TOP 3
#define CAJA_THUMBNAIL_FRAME_RIGHT 3
#define CAJA_THUMBNAIL_FRAME_BOTTOM 3

void
caja_ui_frame_image (GdkPixbuf **pixbuf)
{
    GdkPixbuf *pixbuf_with_frame, *frame;
    int left_offset, top_offset, right_offset, bottom_offset;

    frame = caja_get_thumbnail_frame ();
    if (frame == NULL) {
        return;
    }

    left_offset = CAJA_THUMBNAIL_FRAME_LEFT;
    top_offset = CAJA_THUMBNAIL_FRAME_TOP;
    right_offset = CAJA_THUMBNAIL_FRAME_RIGHT;
    bottom_offset = CAJA_THUMBNAIL_FRAME_BOTTOM;

    pixbuf_with_frame = eel_embed_image_in_frame
        (*pixbuf, frame,
         left_offset, top_offset, right_offset, bottom_offset);
    g_object_unref (*pixbuf);

    *pixbuf = pixbuf_with_frame;
}
