/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-bookmark.h - interface for individual bookmarks.

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

#ifndef CAJA_BOOKMARK_H
#define CAJA_BOOKMARK_H

#include <gtk/gtk.h>
#include <gio/gio.h>
typedef struct CajaBookmark CajaBookmark;

#define CAJA_TYPE_BOOKMARK caja_bookmark_get_type()
#define CAJA_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_BOOKMARK, CajaBookmark))
#define CAJA_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_BOOKMARK, CajaBookmarkClass))
#define CAJA_IS_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_BOOKMARK))
#define CAJA_IS_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_BOOKMARK))
#define CAJA_BOOKMARK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_BOOKMARK, CajaBookmarkClass))

typedef struct CajaBookmarkDetails CajaBookmarkDetails;

struct CajaBookmark
{
    GObject object;
    CajaBookmarkDetails *details;
};

struct CajaBookmarkClass
{
    GObjectClass parent_class;

    /* Signals that clients can connect to. */

    /* The appearance_changed signal is emitted when the bookmark's
     * name or icon has changed.
     */
    void	(* appearance_changed) (CajaBookmark *bookmark);

    /* The contents_changed signal is emitted when the bookmark's
     * URI has changed.
     */
    void	(* contents_changed) (CajaBookmark *bookmark);
};

typedef struct CajaBookmarkClass CajaBookmarkClass;

GType                 caja_bookmark_get_type               (void);
CajaBookmark *    caja_bookmark_new                    (GFile *location,
        const char *name,
        gboolean has_custom_name,
        GIcon *icon);
CajaBookmark *    caja_bookmark_copy                   (CajaBookmark      *bookmark);
char *                caja_bookmark_get_name               (CajaBookmark      *bookmark);
GFile *               caja_bookmark_get_location           (CajaBookmark      *bookmark);
char *                caja_bookmark_get_uri                (CajaBookmark      *bookmark);
GIcon *               caja_bookmark_get_icon               (CajaBookmark      *bookmark);
gboolean	      caja_bookmark_get_has_custom_name    (CajaBookmark      *bookmark);
gboolean              caja_bookmark_set_name               (CajaBookmark      *bookmark,
        const char            *new_name);
gboolean              caja_bookmark_uri_known_not_to_exist (CajaBookmark      *bookmark);
int                   caja_bookmark_compare_with           (gconstpointer          a,
        gconstpointer          b);
int                   caja_bookmark_compare_uris           (gconstpointer          a,
        gconstpointer          b);

void                  caja_bookmark_set_scroll_pos         (CajaBookmark      *bookmark,
        const char            *uri);
char *                caja_bookmark_get_scroll_pos         (CajaBookmark      *bookmark);


/* Helper functions for displaying bookmarks */
cairo_surface_t *     caja_bookmark_get_surface            (CajaBookmark      *bookmark,
        GtkIconSize            icon_size);
GtkWidget *           caja_bookmark_menu_item_new          (CajaBookmark      *bookmark);

#endif /* CAJA_BOOKMARK_H */
