/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* caja-icon-info.c
 * Copyright (C) 2007  Red Hat, Inc.,  Alexander Larsson <alexl@redhat.com>
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
#include "caja-icon-info.h"
#include "caja-icon-names.h"
#include "caja-default-file-icon.h"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <eel/eel-gdk-pixbuf-extensions.h>

struct _CajaIconInfo
{
    GObject parent;

    gboolean sole_owner;
    guint64 last_use_time;
    GdkPixbuf *pixbuf;

    gboolean got_embedded_rect;
    GdkRectangle embedded_rect;
    gint n_attach_points;
    GdkPoint *attach_points;
    char *display_name;
    char *icon_name;
};

struct _CajaIconInfoClass
{
    GObjectClass parent_class;
};

static void schedule_reap_cache (void);

G_DEFINE_TYPE (CajaIconInfo,
               caja_icon_info,
               G_TYPE_OBJECT);

static void
caja_icon_info_init (CajaIconInfo *icon)
{
    icon->last_use_time = g_thread_gettime ();
    icon->sole_owner = TRUE;
}

gboolean
caja_icon_info_is_fallback (CajaIconInfo  *icon)
{
    return icon->pixbuf == NULL;
}

static void
pixbuf_toggle_notify (gpointer      info,
                      GObject      *object,
                      gboolean      is_last_ref)
{
    CajaIconInfo  *icon = info;

    if (is_last_ref)
    {
        icon->sole_owner = TRUE;
        g_object_remove_toggle_ref (object,
                                    pixbuf_toggle_notify,
                                    info);
        icon->last_use_time = g_thread_gettime ();
        schedule_reap_cache ();
    }
}

static void
caja_icon_info_finalize (GObject *object)
{
    CajaIconInfo *icon;

    icon = CAJA_ICON_INFO (object);

    if (!icon->sole_owner && icon->pixbuf)
    {
        g_object_remove_toggle_ref (G_OBJECT (icon->pixbuf),
                                    pixbuf_toggle_notify,
                                    icon);
    }

    if (icon->pixbuf)
    {
        g_object_unref (icon->pixbuf);
    }
    g_free (icon->attach_points);
    g_free (icon->display_name);
    g_free (icon->icon_name);

    G_OBJECT_CLASS (caja_icon_info_parent_class)->finalize (object);
}

static void
caja_icon_info_class_init (CajaIconInfoClass *icon_info_class)
{
    GObjectClass *gobject_class;

    gobject_class = (GObjectClass *) icon_info_class;

    gobject_class->finalize = caja_icon_info_finalize;

}

CajaIconInfo *
caja_icon_info_new_for_pixbuf (GdkPixbuf *pixbuf)
{
    CajaIconInfo *icon;

    icon = g_object_new (CAJA_TYPE_ICON_INFO, NULL);

    if (pixbuf)
    {
        icon->pixbuf = g_object_ref (pixbuf);
    }

    return icon;
}

static CajaIconInfo *
caja_icon_info_new_for_icon_info (GtkIconInfo *icon_info)
{
    CajaIconInfo *icon;
    GdkPoint *points;
    gint n_points;
    const char *filename;
    char *basename, *p;

    icon = g_object_new (CAJA_TYPE_ICON_INFO, NULL);

    icon->pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

    icon->got_embedded_rect = gtk_icon_info_get_embedded_rect (icon_info,
                              &icon->embedded_rect);

    if (gtk_icon_info_get_attach_points (icon_info, &points, &n_points))
    {
        icon->n_attach_points = n_points;
        icon->attach_points = points;
    }

    icon->display_name = g_strdup (gtk_icon_info_get_display_name (icon_info));

    filename = gtk_icon_info_get_filename (icon_info);
    if (filename != NULL)
    {
        basename = g_path_get_basename (filename);
        p = strrchr (basename, '.');
        if (p)
        {
            *p = 0;
        }
        icon->icon_name = basename;
    }

    return icon;
}


typedef struct
{
    GIcon *icon;
    int size;
} LoadableIconKey;

typedef struct
{
    char *filename;
    int size;
} ThemedIconKey;

static GHashTable *loadable_icon_cache = NULL;
static GHashTable *themed_icon_cache = NULL;
static guint reap_cache_timeout = 0;

#define NSEC_PER_SEC ((guint64)1000000000L)

static guint time_now;

static gboolean
reap_old_icon (gpointer  key,
               gpointer  value,
               gpointer  user_info)
{
    CajaIconInfo *icon = value;
    gboolean *reapable_icons_left = user_info;

    if (icon->sole_owner)
    {
        if (time_now - icon->last_use_time > 30 * NSEC_PER_SEC)
        {
            /* This went unused 30 secs ago. reap */
            return TRUE;
        }
        else
        {
            /* We can reap this soon */
            *reapable_icons_left = TRUE;
        }
    }

    return FALSE;
}

static gboolean
reap_cache (gpointer data)
{
    gboolean reapable_icons_left;

    reapable_icons_left = TRUE;

    time_now = g_thread_gettime ();

    if (loadable_icon_cache)
    {
        g_hash_table_foreach_remove (loadable_icon_cache,
                                     reap_old_icon,
                                     &reapable_icons_left);
    }

    if (themed_icon_cache)
    {
        g_hash_table_foreach_remove (themed_icon_cache,
                                     reap_old_icon,
                                     &reapable_icons_left);
    }

    if (reapable_icons_left)
    {
        return TRUE;
    }
    else
    {
        reap_cache_timeout = 0;
        return FALSE;
    }
}

static void
schedule_reap_cache (void)
{
    if (reap_cache_timeout == 0)
    {
        reap_cache_timeout = g_timeout_add_seconds_full (0, 5,
                             reap_cache,
                             NULL, NULL);
    }
}

void
caja_icon_info_clear_caches (void)
{
    if (loadable_icon_cache)
    {
        g_hash_table_remove_all (loadable_icon_cache);
    }

    if (themed_icon_cache)
    {
        g_hash_table_remove_all (themed_icon_cache);
    }
}

static guint
loadable_icon_key_hash (LoadableIconKey *key)
{
    return g_icon_hash (key->icon) ^ key->size;
}

static gboolean
loadable_icon_key_equal (const LoadableIconKey *a,
                         const LoadableIconKey *b)
{
    return a->size == b->size &&
           g_icon_equal (a->icon, b->icon);
}

static LoadableIconKey *
loadable_icon_key_new (GIcon *icon, int size)
{
    LoadableIconKey *key;

    key = g_slice_new (LoadableIconKey);
    key->icon = g_object_ref (icon);
    key->size = size;

    return key;
}

static void
loadable_icon_key_free (LoadableIconKey *key)
{
    g_object_unref (key->icon);
    g_slice_free (LoadableIconKey, key);
}

static guint
themed_icon_key_hash (ThemedIconKey *key)
{
    return g_str_hash (key->filename) ^ key->size;
}

static gboolean
themed_icon_key_equal (const ThemedIconKey *a,
                       const ThemedIconKey *b)
{
    return a->size == b->size &&
           g_str_equal (a->filename, b->filename);
}

static ThemedIconKey *
themed_icon_key_new (const char *filename, int size)
{
    ThemedIconKey *key;

    key = g_slice_new (ThemedIconKey);
    key->filename = g_strdup (filename);
    key->size = size;

    return key;
}

static void
themed_icon_key_free (ThemedIconKey *key)
{
    g_free (key->filename);
    g_slice_free (ThemedIconKey, key);
}

CajaIconInfo *
caja_icon_info_lookup (GIcon *icon,
                       int size)
{
    CajaIconInfo *icon_info;
    GdkPixbuf *pixbuf;

    if (G_IS_LOADABLE_ICON (icon))
    {
        LoadableIconKey lookup_key;
        LoadableIconKey *key;
        GInputStream *stream;

        if (loadable_icon_cache == NULL)
        {
            loadable_icon_cache =
                g_hash_table_new_full ((GHashFunc)loadable_icon_key_hash,
                                       (GEqualFunc)loadable_icon_key_equal,
                                       (GDestroyNotify) loadable_icon_key_free,
                                       (GDestroyNotify) g_object_unref);
        }

        lookup_key.icon = icon;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (loadable_icon_cache, &lookup_key);
        if (icon_info)
        {
            return g_object_ref (icon_info);
        }

        pixbuf = NULL;
        stream = g_loadable_icon_load (G_LOADABLE_ICON (icon),
                                       size,
                                       NULL, NULL, NULL);
        if (stream)
        {
            pixbuf = eel_gdk_pixbuf_load_from_stream_at_size (stream, size);
            g_object_unref (stream);
        }

        icon_info = caja_icon_info_new_for_pixbuf (pixbuf);

        key = loadable_icon_key_new (icon, size);
        g_hash_table_insert (loadable_icon_cache, key, icon_info);

        return g_object_ref (icon_info);
    }
    else if (G_IS_THEMED_ICON (icon))
    {
        const char * const *names;
        ThemedIconKey lookup_key;
        ThemedIconKey *key;
        GtkIconTheme *icon_theme;
        GtkIconInfo *gtkicon_info;
        const char *filename;

        if (themed_icon_cache == NULL)
        {
            themed_icon_cache =
                g_hash_table_new_full ((GHashFunc)themed_icon_key_hash,
                                       (GEqualFunc)themed_icon_key_equal,
                                       (GDestroyNotify) themed_icon_key_free,
                                       (GDestroyNotify) g_object_unref);
        }

        names = g_themed_icon_get_names (G_THEMED_ICON (icon));

        icon_theme = gtk_icon_theme_get_default ();
        gtkicon_info = gtk_icon_theme_choose_icon (icon_theme, (const char **)names, size, 0);

        if (gtkicon_info == NULL)
        {
            return caja_icon_info_new_for_pixbuf (NULL);
        }

        filename = gtk_icon_info_get_filename (gtkicon_info);

        /* 96_no-null-in-g-str-hash.patch from ubuntu natty nautilus
        https://bugs.launchpad.net/ubuntu/+source/nautilus/+bug/718098 */
        if (filename == NULL) {
            gtk_icon_info_free (gtkicon_info);
            return caja_icon_info_new_for_pixbuf (NULL);
        }
        /* patch end */

        lookup_key.filename = (char *)filename;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (themed_icon_cache, &lookup_key);
        if (icon_info)
        {
            gtk_icon_info_free (gtkicon_info);
            return g_object_ref (icon_info);
        }

        icon_info = caja_icon_info_new_for_icon_info (gtkicon_info);

        key = themed_icon_key_new (filename, size);
        g_hash_table_insert (themed_icon_cache, key, icon_info);

        gtk_icon_info_free (gtkicon_info);

        return g_object_ref (icon_info);
    }
    else
    {
        GdkPixbuf *pixbuf;
        GtkIconInfo *gtk_icon_info;

        gtk_icon_info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                        icon,
                        size,
                        GTK_ICON_LOOKUP_GENERIC_FALLBACK);
        if (gtk_icon_info != NULL)
        {
            pixbuf = gtk_icon_info_load_icon (gtk_icon_info, NULL);
            gtk_icon_info_free (gtk_icon_info);
        }
        else
        {
            pixbuf = NULL;
        }

        return caja_icon_info_new_for_pixbuf (pixbuf);
    }
}

CajaIconInfo *
caja_icon_info_lookup_from_name (const char *name,
                                 int size)
{
    GIcon *icon;
    CajaIconInfo *info;

    icon = g_themed_icon_new (name);
    info = caja_icon_info_lookup (icon, size);
    g_object_unref (icon);
    return info;
}

CajaIconInfo *
caja_icon_info_lookup_from_path (const char *path,
                                 int size)
{
    GFile *icon_file;
    GIcon *icon;
    CajaIconInfo *info;

    icon_file = g_file_new_for_path (path);
    icon = g_file_icon_new (icon_file);
    info = caja_icon_info_lookup (icon, size);
    g_object_unref (icon);
    g_object_unref (icon_file);
    return info;
}

GdkPixbuf *
caja_icon_info_get_pixbuf_nodefault (CajaIconInfo  *icon)
{
    GdkPixbuf *res;

    if (icon->pixbuf == NULL)
    {
        res = NULL;
    }
    else
    {
        res = g_object_ref (icon->pixbuf);

        if (icon->sole_owner)
        {
            icon->sole_owner = FALSE;
            g_object_add_toggle_ref (G_OBJECT (res),
                                     pixbuf_toggle_notify,
                                     icon);
        }
    }

    return res;
}


GdkPixbuf *
caja_icon_info_get_pixbuf (CajaIconInfo *icon)
{
    GdkPixbuf *res;

    res = caja_icon_info_get_pixbuf_nodefault (icon);
    if (res == NULL)
    {
        res = gdk_pixbuf_new_from_data (caja_default_file_icon,
                                        GDK_COLORSPACE_RGB,
                                        TRUE,
                                        8,
                                        caja_default_file_icon_width,
                                        caja_default_file_icon_height,
                                        caja_default_file_icon_width * 4, /* stride */
                                        NULL, /* don't destroy info */
                                        NULL);
    }

    return res;
}

GdkPixbuf *
caja_icon_info_get_pixbuf_nodefault_at_size (CajaIconInfo  *icon,
        gsize              forced_size)
{
    GdkPixbuf *pixbuf, *scaled_pixbuf;
    int w, h, s;
    double scale;

    pixbuf = caja_icon_info_get_pixbuf_nodefault (icon);

    if (pixbuf == NULL)
        return NULL;

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);
    s = MAX (w, h);
    if (s == forced_size)
    {
        return pixbuf;
    }

    scale = (double)forced_size / s;
    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                    w * scale, h * scale,
                    GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    return scaled_pixbuf;
}


GdkPixbuf *
caja_icon_info_get_pixbuf_at_size (CajaIconInfo  *icon,
                                   gsize              forced_size)
{
    GdkPixbuf *pixbuf, *scaled_pixbuf;
    int w, h, s;
    double scale;

    pixbuf = caja_icon_info_get_pixbuf (icon);

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);
    s = MAX (w, h);
    if (s == forced_size)
    {
        return pixbuf;
    }

    scale = (double)forced_size / s;
    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                    w * scale, h * scale,
                    GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    return scaled_pixbuf;
}

gboolean
caja_icon_info_get_embedded_rect (CajaIconInfo  *icon,
                                  GdkRectangle      *rectangle)
{
    *rectangle = icon->embedded_rect;
    return icon->got_embedded_rect;
}

gboolean
caja_icon_info_get_attach_points (CajaIconInfo  *icon,
                                  GdkPoint         **points,
                                  gint              *n_points)
{
    *n_points = icon->n_attach_points;
    *points = icon->attach_points;
    return icon->n_attach_points != 0;
}

const char* caja_icon_info_get_display_name(CajaIconInfo* icon)
{
    return icon->display_name;
}

const char* caja_icon_info_get_used_name(CajaIconInfo* icon)
{
    return icon->icon_name;
}

/* Return nominal icon size for given zoom level.
 * @zoom_level: zoom level for which to find matching icon size.
 *
 * Return value: icon size between CAJA_ICON_SIZE_SMALLEST and
 * CAJA_ICON_SIZE_LARGEST, inclusive.
 */
guint
caja_get_icon_size_for_zoom_level (CajaZoomLevel zoom_level)
{
    switch (zoom_level)
    {
    case CAJA_ZOOM_LEVEL_SMALLEST:
        return CAJA_ICON_SIZE_SMALLEST;
    case CAJA_ZOOM_LEVEL_SMALLER:
        return CAJA_ICON_SIZE_SMALLER;
    case CAJA_ZOOM_LEVEL_SMALL:
        return CAJA_ICON_SIZE_SMALL;
    case CAJA_ZOOM_LEVEL_STANDARD:
        return CAJA_ICON_SIZE_STANDARD;
    case CAJA_ZOOM_LEVEL_LARGE:
        return CAJA_ICON_SIZE_LARGE;
    case CAJA_ZOOM_LEVEL_LARGER:
        return CAJA_ICON_SIZE_LARGER;
    case CAJA_ZOOM_LEVEL_LARGEST:
        return CAJA_ICON_SIZE_LARGEST;
    }
    g_return_val_if_reached (CAJA_ICON_SIZE_STANDARD);
}

float
caja_get_relative_icon_size_for_zoom_level (CajaZoomLevel zoom_level)
{
    return (float)caja_get_icon_size_for_zoom_level (zoom_level) / CAJA_ICON_SIZE_STANDARD;
}

guint
caja_icon_get_larger_icon_size (guint size)
{
    if (size < CAJA_ICON_SIZE_SMALLEST)
    {
        return CAJA_ICON_SIZE_SMALLEST;
    }
    if (size < CAJA_ICON_SIZE_SMALLER)
    {
        return CAJA_ICON_SIZE_SMALLER;
    }
    if (size < CAJA_ICON_SIZE_SMALL)
    {
        return CAJA_ICON_SIZE_SMALL;
    }
    if (size < CAJA_ICON_SIZE_STANDARD)
    {
        return CAJA_ICON_SIZE_STANDARD;
    }
    if (size < CAJA_ICON_SIZE_LARGE)
    {
        return CAJA_ICON_SIZE_LARGE;
    }
    if (size < CAJA_ICON_SIZE_LARGER)
    {
        return CAJA_ICON_SIZE_LARGER;
    }
    return CAJA_ICON_SIZE_LARGEST;
}

guint
caja_icon_get_smaller_icon_size (guint size)
{
    if (size > CAJA_ICON_SIZE_LARGEST)
    {
        return CAJA_ICON_SIZE_LARGEST;
    }
    if (size > CAJA_ICON_SIZE_LARGER)
    {
        return CAJA_ICON_SIZE_LARGER;
    }
    if (size > CAJA_ICON_SIZE_LARGE)
    {
        return CAJA_ICON_SIZE_LARGE;
    }
    if (size > CAJA_ICON_SIZE_STANDARD)
    {
        return CAJA_ICON_SIZE_STANDARD;
    }
    if (size > CAJA_ICON_SIZE_SMALL)
    {
        return CAJA_ICON_SIZE_SMALL;
    }
    if (size > CAJA_ICON_SIZE_SMALLER)
    {
        return CAJA_ICON_SIZE_SMALLER;
    }
    return CAJA_ICON_SIZE_SMALLEST;
}

gint
caja_get_icon_size_for_stock_size (GtkIconSize size)
{
    gint w, h;

    if (gtk_icon_size_lookup (size, &w, &h))
    {
        return MAX (w, h);
    }
    return CAJA_ZOOM_LEVEL_STANDARD;
}


guint
caja_icon_get_emblem_size_for_icon_size (guint size)
{
    if (size >= 96)
        return 48;
    if (size >= 64)
        return 32;
    if (size >= 48)
        return 24;
    if (size >= 24)
        return 16;
    if (size >= 16)
        return 12;

    return 0; /* no emblems for smaller sizes */
}

gboolean
caja_icon_theme_can_render (GThemedIcon *icon)
{
	GtkIconTheme *icon_theme;
	const gchar * const *names;
	gint idx;

	names = g_themed_icon_get_names (icon);

	icon_theme = gtk_icon_theme_get_default ();

	for (idx = 0; names[idx] != NULL; idx++) {
		if (gtk_icon_theme_has_icon (icon_theme, names[idx])) {
			return TRUE;
		}
	}

	return FALSE;
}

GIcon *
caja_user_special_directory_get_gicon (GUserDirectory directory)
{

	#define ICON_CASE(x) \
		case G_USER_DIRECTORY_ ## x:\
			return g_themed_icon_new (CAJA_ICON_FOLDER_ ## x);

	switch (directory) {

		ICON_CASE (DESKTOP);
		ICON_CASE (DOCUMENTS);
		ICON_CASE (DOWNLOAD);
		ICON_CASE (MUSIC);
		ICON_CASE (PICTURES);
		ICON_CASE (PUBLIC_SHARE);
		ICON_CASE (TEMPLATES);
		ICON_CASE (VIDEOS);

	default:
		return g_themed_icon_new ("folder");
	}

	#undef ICON_CASE
}
