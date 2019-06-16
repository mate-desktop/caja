/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-bookmark.c - implementation of individual bookmarks.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>

#include "caja-bookmark.h"
#include "caja-file.h"
#include "caja-icon-names.h"

enum
{
    APPEARANCE_CHANGED,
    CONTENTS_CHANGED,
    LAST_SIGNAL
};

#define ELLIPSISED_MENU_ITEM_MIN_CHARS  32

static guint signals[LAST_SIGNAL] = { 0 };

struct CajaBookmarkDetails
{
    char *name;
    gboolean has_custom_name;
    GFile *location;
    GIcon *icon;
    CajaFile *file;

    char *scroll_file;
};

static void	  caja_bookmark_connect_file	  (CajaBookmark	 *file);
static void	  caja_bookmark_disconnect_file	  (CajaBookmark	 *file);

G_DEFINE_TYPE (CajaBookmark, caja_bookmark, G_TYPE_OBJECT);

/* GObject methods.  */

static void
caja_bookmark_finalize (GObject *object)
{
    CajaBookmark *bookmark;

    g_assert (CAJA_IS_BOOKMARK (object));

    bookmark = CAJA_BOOKMARK (object);

    caja_bookmark_disconnect_file (bookmark);

    g_free (bookmark->details->name);
    g_object_unref (bookmark->details->location);
    if (bookmark->details->icon)
    {
        g_object_unref (bookmark->details->icon);
    }
    g_free (bookmark->details->scroll_file);
    g_free (bookmark->details);

    G_OBJECT_CLASS (caja_bookmark_parent_class)->finalize (object);
}

/* Initialization.  */

static void
caja_bookmark_class_init (CajaBookmarkClass *class)
{
    G_OBJECT_CLASS (class)->finalize = caja_bookmark_finalize;

    signals[APPEARANCE_CHANGED] =
        g_signal_new ("appearance_changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaBookmarkClass, appearance_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[CONTENTS_CHANGED] =
        g_signal_new ("contents_changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (CajaBookmarkClass, contents_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

}

static void
caja_bookmark_init (CajaBookmark *bookmark)
{
    bookmark->details = g_new0 (CajaBookmarkDetails, 1);
}

/**
 * caja_bookmark_compare_with:
 *
 * Check whether two bookmarks are considered identical.
 * @a: first CajaBookmark*.
 * @b: second CajaBookmark*.
 *
 * Return value: 0 if @a and @b have same name and uri, 1 otherwise
 * (GCompareFunc style)
 **/
int
caja_bookmark_compare_with (gconstpointer a, gconstpointer b)
{
    CajaBookmark *bookmark_a;
    CajaBookmark *bookmark_b;

    g_return_val_if_fail (CAJA_IS_BOOKMARK (a), 1);
    g_return_val_if_fail (CAJA_IS_BOOKMARK (b), 1);

    bookmark_a = CAJA_BOOKMARK (a);
    bookmark_b = CAJA_BOOKMARK (b);

    if (g_strcmp0 (bookmark_a->details->name,
                    bookmark_b->details->name) != 0)
    {
        return 1;
    }

    if (!g_file_equal (bookmark_a->details->location,
                       bookmark_b->details->location))
    {
        return 1;
    }

    return 0;
}

/**
 * caja_bookmark_compare_uris:
 *
 * Check whether the uris of two bookmarks are for the same location.
 * @a: first CajaBookmark*.
 * @b: second CajaBookmark*.
 *
 * Return value: 0 if @a and @b have matching uri, 1 otherwise
 * (GCompareFunc style)
 **/
int
caja_bookmark_compare_uris (gconstpointer a, gconstpointer b)
{
    CajaBookmark *bookmark_a;
    CajaBookmark *bookmark_b;

    g_return_val_if_fail (CAJA_IS_BOOKMARK (a), 1);
    g_return_val_if_fail (CAJA_IS_BOOKMARK (b), 1);

    bookmark_a = CAJA_BOOKMARK (a);
    bookmark_b = CAJA_BOOKMARK (b);

    return !g_file_equal (bookmark_a->details->location,
                          bookmark_b->details->location);
}

CajaBookmark *
caja_bookmark_copy (CajaBookmark *bookmark)
{
    g_return_val_if_fail (CAJA_IS_BOOKMARK (bookmark), NULL);

    return caja_bookmark_new (
               bookmark->details->location,
               bookmark->details->name,
               bookmark->details->has_custom_name,
               bookmark->details->icon);
}

char *
caja_bookmark_get_name (CajaBookmark *bookmark)
{
    g_return_val_if_fail(CAJA_IS_BOOKMARK (bookmark), NULL);

    return g_strdup (bookmark->details->name);
}


gboolean
caja_bookmark_get_has_custom_name (CajaBookmark *bookmark)
{
    g_return_val_if_fail(CAJA_IS_BOOKMARK (bookmark), FALSE);

    return (bookmark->details->has_custom_name);
}

cairo_surface_t *
caja_bookmark_get_surface (CajaBookmark *bookmark,
                           GtkIconSize stock_size)
{
    cairo_surface_t *result;
    GIcon *icon;
    CajaIconInfo *info;
    int pixel_size, pixel_scale;

    g_return_val_if_fail (CAJA_IS_BOOKMARK (bookmark), NULL);

    icon = caja_bookmark_get_icon (bookmark);
    if (icon == NULL)
    {
        return NULL;
    }

    pixel_size = caja_get_icon_size_for_stock_size (stock_size);
    pixel_scale = gdk_window_get_scale_factor (gdk_get_default_root_window ());
    info = caja_icon_info_lookup (icon, pixel_size, pixel_scale);
    result = caja_icon_info_get_surface_at_size (info, pixel_size);
    g_object_unref (info);

    g_object_unref (icon);

    return result;
}

GIcon *
caja_bookmark_get_icon (CajaBookmark *bookmark)
{
    g_return_val_if_fail (CAJA_IS_BOOKMARK (bookmark), NULL);

    /* Try to connect a file in case file exists now but didn't earlier. */
    caja_bookmark_connect_file (bookmark);

    if (bookmark->details->icon)
    {
        return g_object_ref (bookmark->details->icon);
    }
    return NULL;
}

GFile *
caja_bookmark_get_location (CajaBookmark *bookmark)
{
    g_return_val_if_fail(CAJA_IS_BOOKMARK (bookmark), NULL);

    /* Try to connect a file in case file exists now but didn't earlier.
     * This allows a bookmark to update its image properly in the case
     * where a new file appears with the same URI as a previously-deleted
     * file. Calling connect_file here means that attempts to activate the
     * bookmark will update its image if possible.
     */
    caja_bookmark_connect_file (bookmark);

    return g_object_ref (bookmark->details->location);
}

char *
caja_bookmark_get_uri (CajaBookmark *bookmark)
{
    GFile *file;
    char *uri;

    file = caja_bookmark_get_location (bookmark);
    uri = g_file_get_uri (file);
    g_object_unref (file);
    return uri;
}


/**
 * caja_bookmark_set_name:
 *
 * Change the user-displayed name of a bookmark.
 * @new_name: The new user-displayed name for this bookmark, mustn't be NULL.
 *
 * Returns: TRUE if the name changed else FALSE.
 **/
gboolean
caja_bookmark_set_name (CajaBookmark *bookmark, const char *new_name)
{
    g_return_val_if_fail (new_name != NULL, FALSE);
    g_return_val_if_fail (CAJA_IS_BOOKMARK (bookmark), FALSE);

    if (g_strcmp0 (new_name, bookmark->details->name) == 0)
    {
        return FALSE;
    }
    else if (!bookmark->details->has_custom_name)
    {
        bookmark->details->has_custom_name = TRUE;
    }

    g_free (bookmark->details->name);
    bookmark->details->name = g_strdup (new_name);

    g_signal_emit (bookmark, signals[APPEARANCE_CHANGED], 0);

    if (bookmark->details->has_custom_name)
    {
        g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
    }

    return TRUE;
}

static gboolean
caja_bookmark_icon_is_different (CajaBookmark *bookmark,
                                 GIcon *new_icon)
{
    g_assert (CAJA_IS_BOOKMARK (bookmark));
    g_assert (new_icon != NULL);

    if (bookmark->details->icon == NULL)
    {
        return TRUE;
    }

    return !g_icon_equal (bookmark->details->icon, new_icon) != 0;
}

/**
 * Update icon if there's a better one available.
 * Return TRUE if the icon changed.
 */
static gboolean
caja_bookmark_update_icon (CajaBookmark *bookmark)
{
    GIcon *new_icon;

    g_assert (CAJA_IS_BOOKMARK (bookmark));

    if (bookmark->details->file == NULL)
    {
        return FALSE;
    }

    if (!caja_file_is_local (bookmark->details->file))
    {
        /* never update icons for remote bookmarks */
        return FALSE;
    }

    if (!caja_file_is_not_yet_confirmed (bookmark->details->file) &&
            caja_file_check_if_ready (bookmark->details->file,
                                      CAJA_FILE_ATTRIBUTES_FOR_ICON))
    {
        new_icon = caja_file_get_gicon (bookmark->details->file, 0);
        if (caja_bookmark_icon_is_different (bookmark, new_icon))
        {
            if (bookmark->details->icon)
            {
                g_object_unref (bookmark->details->icon);
            }
            bookmark->details->icon = new_icon;
            return TRUE;
        }
        g_object_unref (new_icon);
    }

    return FALSE;
}

static void
bookmark_file_changed_callback (CajaFile *file, CajaBookmark *bookmark)
{
    GFile *location;
    gboolean should_emit_appearance_changed_signal;
    gboolean should_emit_contents_changed_signal;
    char *display_name;

    g_assert (CAJA_IS_FILE (file));
    g_assert (CAJA_IS_BOOKMARK (bookmark));
    g_assert (file == bookmark->details->file);

    should_emit_appearance_changed_signal = FALSE;
    should_emit_contents_changed_signal = FALSE;
    location = caja_file_get_location (file);

    if (!g_file_equal (bookmark->details->location, location) &&
            !caja_file_is_in_trash (file))
    {
        g_object_unref (bookmark->details->location);
        bookmark->details->location = location;
        should_emit_contents_changed_signal = TRUE;
    }
    else
    {
        g_object_unref (location);
    }

    if (caja_file_is_gone (file) ||
            caja_file_is_in_trash (file))
    {
        /* The file we were monitoring has been trashed, deleted,
         * or moved in a way that we didn't notice. We should make
         * a spanking new CajaFile object for this
         * location so if a new file appears in this place
         * we will notice. However, we can't immediately do so
         * because creating a new CajaFile directly as a result
         * of noticing a file goes away may trigger i/o on that file
         * again, noticeing it is gone, leading to a loop.
         * So, the new CajaFile is created when the bookmark
         * is used again. However, this is not really a problem, as
         * we don't want to change the icon or anything about the
         * bookmark just because its not there anymore.
         */
        caja_bookmark_disconnect_file (bookmark);
    }
    else if (caja_bookmark_update_icon (bookmark))
    {
        /* File hasn't gone away, but it has changed
         * in a way that affected its icon.
         */
        should_emit_appearance_changed_signal = TRUE;
    }

    if (!bookmark->details->has_custom_name)
    {
        display_name = caja_file_get_display_name (file);

        if (g_strcmp0 (bookmark->details->name, display_name) != 0)
        {
            g_free (bookmark->details->name);
            bookmark->details->name = display_name;
            should_emit_appearance_changed_signal = TRUE;
        }
        else
        {
            g_free (display_name);
        }
    }

    if (should_emit_appearance_changed_signal)
    {
        g_signal_emit (bookmark, signals[APPEARANCE_CHANGED], 0);
    }

    if (should_emit_contents_changed_signal)
    {
        g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
    }
}

/**
 * caja_bookmark_set_icon_to_default:
 *
 * Reset the icon to either the missing bookmark icon or the generic
 * bookmark icon, depending on whether the file still exists.
 */
static void
caja_bookmark_set_icon_to_default (CajaBookmark *bookmark)
{
    GIcon *emblemed_icon, *folder;

    if (bookmark->details->icon)
    {
        g_object_unref (bookmark->details->icon);
    }

    folder = g_themed_icon_new (CAJA_ICON_FOLDER);

    if (caja_bookmark_uri_known_not_to_exist (bookmark))
    {
        GIcon *icon;
        GEmblem *emblem;

        icon = g_themed_icon_new ("dialog-warning");
        emblem = g_emblem_new (icon);

        emblemed_icon = g_emblemed_icon_new (folder, emblem);

        g_object_unref (emblem);
        g_object_unref (icon);
        g_object_unref (folder);

        folder = emblemed_icon;
    }

    bookmark->details->icon = folder;
}

static void
caja_bookmark_disconnect_file (CajaBookmark *bookmark)
{
    g_assert (CAJA_IS_BOOKMARK (bookmark));

    if (bookmark->details->file != NULL)
    {
        g_signal_handlers_disconnect_by_func (bookmark->details->file,
                                              G_CALLBACK (bookmark_file_changed_callback),
                                              bookmark);
        caja_file_unref (bookmark->details->file);
        bookmark->details->file = NULL;
    }

    if (bookmark->details->icon != NULL)
    {
        g_object_unref (bookmark->details->icon);
        bookmark->details->icon = NULL;
    }
}

static void
caja_bookmark_connect_file (CajaBookmark *bookmark)
{
    char *display_name;

    g_assert (CAJA_IS_BOOKMARK (bookmark));

    if (bookmark->details->file != NULL)
    {
        return;
    }

    if (!caja_bookmark_uri_known_not_to_exist (bookmark))
    {
        bookmark->details->file = caja_file_get (bookmark->details->location);
        g_assert (!caja_file_is_gone (bookmark->details->file));

        g_signal_connect_object (bookmark->details->file, "changed",
                                 G_CALLBACK (bookmark_file_changed_callback), bookmark, 0);
    }

    /* Set icon based on available information; don't force network i/o
     * to get any currently unknown information.
     */
    if (!caja_bookmark_update_icon (bookmark))
    {
        if (bookmark->details->icon == NULL || bookmark->details->file == NULL)
        {
            caja_bookmark_set_icon_to_default (bookmark);
        }
    }

    if (!bookmark->details->has_custom_name &&
            bookmark->details->file &&
            caja_file_check_if_ready (bookmark->details->file, CAJA_FILE_ATTRIBUTE_INFO))
    {
        display_name = caja_file_get_display_name (bookmark->details->file);
        if (g_strcmp0 (bookmark->details->name, display_name) != 0)
        {
            g_free (bookmark->details->name);
            bookmark->details->name = display_name;
        }
        else
        {
            g_free (display_name);
        }
    }
}

CajaBookmark *
caja_bookmark_new (GFile *location, const char *name, gboolean has_custom_name,
                   GIcon *icon)
{
    CajaBookmark *new_bookmark;

    new_bookmark = CAJA_BOOKMARK (g_object_new (CAJA_TYPE_BOOKMARK, NULL));

    new_bookmark->details->name = g_strdup (name);
    new_bookmark->details->location = g_object_ref (location);
    new_bookmark->details->has_custom_name = has_custom_name;
    if (icon)
    {
        new_bookmark->details->icon = g_object_ref (icon);
    }

    caja_bookmark_connect_file (new_bookmark);

    return new_bookmark;
}

static cairo_surface_t *
create_image_cairo_for_bookmark (CajaBookmark *bookmark)
{
    cairo_surface_t *surface;

    surface = caja_bookmark_get_surface (bookmark, GTK_ICON_SIZE_MENU);
    if (surface == NULL)
    {
        return NULL;
    }

    return surface;
}

static GtkWidget *
bookmark_image_menu_item_new_from_surface (cairo_surface_t   *icon_surface,
                                           const gchar       *label_name)
{
    GtkWidget *icon;
    GtkLabel *label;
    gchar *concat;

    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

    if (icon_surface)
        icon = gtk_image_new_from_surface (icon_surface);
    else
        icon = gtk_image_new ();

    concat = g_strconcat (label_name, "     ", NULL);
    GtkWidget *label_menu = gtk_label_new (concat);
    g_free (concat);

    label = GTK_LABEL (label_menu);
    gtk_label_set_use_underline (label, FALSE);
    gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars (label, (ELLIPSISED_MENU_ITEM_MIN_CHARS + 2));

    GtkWidget *menuitem = gtk_menu_item_new ();

    gtk_container_add (GTK_CONTAINER (box), icon);
    gtk_container_add (GTK_CONTAINER (box), label_menu);

    gtk_container_add (GTK_CONTAINER (menuitem), box);
    gtk_widget_show_all (menuitem);

    return menuitem;
}

/**
 * caja_bookmark_menu_item_new:
 *
 * Return a menu item representing a bookmark.
 * @bookmark: The bookmark the menu item represents.
 * Return value: A newly-created bookmark, not yet shown.
 **/
GtkWidget *
caja_bookmark_menu_item_new (CajaBookmark *bookmark)
{
    cairo_surface_t *image_cairo;

    image_cairo = create_image_cairo_for_bookmark (bookmark);

    if (strlen (bookmark->details->name) > 0)
    {
        GtkWidget *menu_item;

        menu_item = bookmark_image_menu_item_new_from_surface (image_cairo, bookmark->details->name);

        return menu_item;
    }
    else
        return NULL;
}

gboolean
caja_bookmark_uri_known_not_to_exist (CajaBookmark *bookmark)
{
    char *path_name;
    gboolean exists;

    /* Convert to a path, returning FALSE if not local. */
    if (!g_file_is_native (bookmark->details->location))
    {
        return FALSE;
    }
    path_name = g_file_get_path (bookmark->details->location);

    /* Now check if the file exists (sync. call OK because it is local). */
    exists = g_file_test (path_name, G_FILE_TEST_EXISTS);
    g_free (path_name);

    return !exists;
}

void
caja_bookmark_set_scroll_pos (CajaBookmark      *bookmark,
                              const char            *uri)
{
    g_free (bookmark->details->scroll_file);
    bookmark->details->scroll_file = g_strdup (uri);
}

char *
caja_bookmark_get_scroll_pos (CajaBookmark      *bookmark)
{
    return g_strdup (bookmark->details->scroll_file);
}
