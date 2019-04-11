/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* caja-bookmark-list.h - interface for centralized list of bookmarks.
 */

#ifndef CAJA_BOOKMARK_LIST_H
#define CAJA_BOOKMARK_LIST_H

#include <gio/gio.h>

#include <libcaja-private/caja-bookmark.h>

typedef struct CajaBookmarkList CajaBookmarkList;
typedef struct CajaBookmarkListClass CajaBookmarkListClass;

#define CAJA_TYPE_BOOKMARK_LIST caja_bookmark_list_get_type()
#define CAJA_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_BOOKMARK_LIST, CajaBookmarkList))
#define CAJA_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_BOOKMARK_LIST, CajaBookmarkListClass))
#define CAJA_IS_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_BOOKMARK_LIST))
#define CAJA_IS_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_BOOKMARK_LIST))
#define CAJA_BOOKMARK_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_BOOKMARK_LIST, CajaBookmarkListClass))

struct CajaBookmarkList
{
    GObject object;

    GList *list;
    GFileMonitor *monitor;
    GQueue *pending_ops;
};

struct CajaBookmarkListClass
{
    GObjectClass parent_class;
    void (* contents_changed) (CajaBookmarkList *bookmarks);
};

GType                   caja_bookmark_list_get_type            (void);
CajaBookmarkList *  caja_bookmark_list_new                 (void);
void                    caja_bookmark_list_append              (CajaBookmarkList   *bookmarks,
        CajaBookmark *bookmark);
gboolean                caja_bookmark_list_contains            (CajaBookmarkList   *bookmarks,
        CajaBookmark *bookmark);
void                    caja_bookmark_list_delete_item_at      (CajaBookmarkList   *bookmarks,
        guint                   index);
void                    caja_bookmark_list_delete_items_with_uri (CajaBookmarkList *bookmarks,
        const char		   *uri);
void                    caja_bookmark_list_insert_item         (CajaBookmarkList   *bookmarks,
        CajaBookmark *bookmark,
        guint                   index);
guint                   caja_bookmark_list_length              (CajaBookmarkList   *bookmarks);
CajaBookmark *      caja_bookmark_list_item_at             (CajaBookmarkList   *bookmarks,
        guint                   index);
void                    caja_bookmark_list_move_item           (CajaBookmarkList *bookmarks,
        guint                 index,
        guint                 destination);
void                    caja_bookmark_list_set_window_geometry (CajaBookmarkList   *bookmarks,
        const char             *geometry);
const char *            caja_bookmark_list_get_window_geometry (CajaBookmarkList   *bookmarks);

#endif /* CAJA_BOOKMARK_LIST_H */
