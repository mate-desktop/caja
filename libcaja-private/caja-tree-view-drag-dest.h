/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Dave Camp <dave@ximian.com>
 */

/* caja-tree-view-drag-dest.h: Handles drag and drop for treeviews which
 *                                 contain a hierarchy of files
 */

#ifndef CAJA_TREE_VIEW_DRAG_DEST_H
#define CAJA_TREE_VIEW_DRAG_DEST_H

#include <gtk/gtk.h>

#include "caja-file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_TREE_VIEW_DRAG_DEST	(caja_tree_view_drag_dest_get_type ())
#define CAJA_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_TREE_VIEW_DRAG_DEST, CajaTreeViewDragDest))
#define CAJA_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_TREE_VIEW_DRAG_DEST, CajaTreeViewDragDestClass))
#define CAJA_IS_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_INSTANCE_CHECK_TYPE ((obj), CAJA_TYPE_TREE_VIEW_DRAG_DEST))
#define CAJA_IS_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_TREE_VIEW_DRAG_DEST))

    typedef struct _CajaTreeViewDragDest        CajaTreeViewDragDest;
    typedef struct _CajaTreeViewDragDestClass   CajaTreeViewDragDestClass;
    typedef struct _CajaTreeViewDragDestDetails CajaTreeViewDragDestDetails;

    struct _CajaTreeViewDragDest
    {
        GObject parent;

        CajaTreeViewDragDestDetails *details;
    };

    struct _CajaTreeViewDragDestClass
    {
        GObjectClass parent;

        char *(*get_root_uri) (CajaTreeViewDragDest *dest);
        CajaFile *(*get_file_for_path) (CajaTreeViewDragDest *dest,
                                        GtkTreePath *path);
        void (*move_copy_items) (CajaTreeViewDragDest *dest,
                                 const GList *item_uris,
                                 const char *target_uri,
                                 GdkDragAction action,
                                 int x,
                                 int y);
        void (* handle_netscape_url) (CajaTreeViewDragDest *dest,
                                      const char *url,
                                      const char *target_uri,
                                      GdkDragAction action,
                                      int x,
                                      int y);
        void (* handle_uri_list) (CajaTreeViewDragDest *dest,
                                  const char *uri_list,
                                  const char *target_uri,
                                  GdkDragAction action,
                                  int x,
                                  int y);
        void (* handle_text)    (CajaTreeViewDragDest *dest,
                                 const char *text,
                                 const char *target_uri,
                                 GdkDragAction action,
                                 int x,
                                 int y);
        void (* handle_raw)    (CajaTreeViewDragDest *dest,
                                char *raw_data,
                                int length,
                                const char *target_uri,
                                const char *direct_save_uri,
                                GdkDragAction action,
                                int x,
                                int y);
    };

    GType                     caja_tree_view_drag_dest_get_type (void);
    CajaTreeViewDragDest *caja_tree_view_drag_dest_new      (GtkTreeView *tree_view);

#ifdef __cplusplus
}
#endif

#endif
