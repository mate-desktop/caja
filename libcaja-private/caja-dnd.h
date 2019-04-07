/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-dnd.h - Common Drag & drop handling code shared by the icon container
   and the list view.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
	    Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef CAJA_DND_H
#define CAJA_DND_H

#include <gtk/gtk.h>

#include "caja-window-slot-info.h"

/* Drag & Drop target names. */
#define CAJA_ICON_DND_MATE_ICON_LIST_TYPE	"x-special/mate-icon-list"
#define CAJA_ICON_DND_URI_LIST_TYPE		"text/uri-list"
#define CAJA_ICON_DND_NETSCAPE_URL_TYPE	"_NETSCAPE_URL"
#define CAJA_ICON_DND_COLOR_TYPE		"application/x-color"
#define CAJA_ICON_DND_BGIMAGE_TYPE		"property/bgimage"
#define CAJA_ICON_DND_KEYWORD_TYPE		"property/keyword"
#define CAJA_ICON_DND_RESET_BACKGROUND_TYPE "x-special/mate-reset-background"
#define CAJA_ICON_DND_ROOTWINDOW_DROP_TYPE	"application/x-rootwindow-drop"
#define CAJA_ICON_DND_XDNDDIRECTSAVE_TYPE	"XdndDirectSave0" /* XDS Protocol Type */
#define CAJA_ICON_DND_RAW_TYPE	"application/octet-stream"

/* Item of the drag selection list */
typedef struct
{
    char *uri;
    gboolean got_icon_position;
    int icon_x, icon_y;
    int icon_width, icon_height;
} CajaDragSelectionItem;

/* Standard Drag & Drop types. */
typedef enum
{
    CAJA_ICON_DND_MATE_ICON_LIST,
    CAJA_ICON_DND_URI_LIST,
    CAJA_ICON_DND_NETSCAPE_URL,
    CAJA_ICON_DND_COLOR,
    CAJA_ICON_DND_BGIMAGE,
    CAJA_ICON_DND_KEYWORD,
    CAJA_ICON_DND_TEXT,
    CAJA_ICON_DND_RESET_BACKGROUND,
    CAJA_ICON_DND_XDNDDIRECTSAVE,
    CAJA_ICON_DND_RAW,
    CAJA_ICON_DND_ROOTWINDOW_DROP
} CajaIconDndTargetType;

typedef enum
{
    CAJA_DND_ACTION_FIRST = GDK_ACTION_ASK << 1,
    CAJA_DND_ACTION_SET_AS_BACKGROUND = CAJA_DND_ACTION_FIRST << 0,
    CAJA_DND_ACTION_SET_AS_FOLDER_BACKGROUND = CAJA_DND_ACTION_FIRST << 1,
    CAJA_DND_ACTION_SET_AS_GLOBAL_BACKGROUND = CAJA_DND_ACTION_FIRST << 2
} CajaDndAction;

/* drag&drop-related information. */
typedef struct
{
    GtkTargetList *target_list;

    /* Stuff saved at "receive data" time needed later in the drag. */
    gboolean got_drop_data_type;
    CajaIconDndTargetType data_type;
    GtkSelectionData *selection_data;
    char *direct_save_uri;

    /* Start of the drag, in window coordinates. */
    int start_x, start_y;

    /* List of CajaDragSelectionItems, representing items being dragged, or NULL
     * if data about them has not been received from the source yet.
     */
    GList *selection_list;

    /* has the drop occured ? */
    gboolean drop_occured;

    /* whether or not need to clean up the previous dnd data */
    gboolean need_to_destroy;

    /* autoscrolling during dragging */
    int auto_scroll_timeout_id;
    gboolean waiting_to_autoscroll;
    gint64 start_auto_scroll_in;

} CajaDragInfo;

typedef struct
{
    /* NB: the following elements are managed by us */
    gboolean have_data;
    gboolean have_valid_data;

    gboolean drop_occured;

    unsigned int info;
    union
    {
        GList *selection_list;
        GList *uri_list;
        char *netscape_url;
    } data;

    /* NB: the following elements are managed by the caller of
     *   caja_drag_slot_proxy_init() */

    /* a fixed location, or NULL to use slot's location */
    GFile *target_location;
    /* a fixed slot, or NULL to use the window's active slot */
    CajaWindowSlotInfo *target_slot;
} CajaDragSlotProxyInfo;

typedef void		(* CajaDragEachSelectedItemDataGet)	(const char *url,
        int x, int y, int w, int h,
        gpointer data);
typedef void		(* CajaDragEachSelectedItemIterator)	(CajaDragEachSelectedItemDataGet iteratee,
        gpointer iterator_context,
        gpointer data);

void			    caja_drag_init				(CajaDragInfo		      *drag_info,
        const GtkTargetEntry		      *drag_types,
        int				       drag_type_count,
        gboolean			       add_text_targets);
void			    caja_drag_finalize			(CajaDragInfo		      *drag_info);
CajaDragSelectionItem  *caja_drag_selection_item_new		(void);
void			    caja_drag_destroy_selection_list	(GList				      *selection_list);
GList			   *caja_drag_build_selection_list		(GtkSelectionData		      *data);

GList *			    caja_drag_uri_list_from_selection_list	(const GList			      *selection_list);

GList *			    caja_drag_uri_list_from_array		(const char			     **uris);

gboolean		    caja_drag_items_local			(const char			      *target_uri,
        const GList			      *selection_list);
gboolean		    caja_drag_uris_local			(const char			      *target_uri,
        const GList			      *source_uri_list);
gboolean		    caja_drag_items_on_desktop		(const GList			      *selection_list);
void			    caja_drag_default_drop_action_for_icons (GdkDragContext			      *context,
        const char			      *target_uri,
        const GList			      *items,
        int				      *action);
GdkDragAction		    caja_drag_default_drop_action_for_netscape_url (GdkDragContext			     *context);
GdkDragAction		    caja_drag_default_drop_action_for_uri_list     (GdkDragContext			     *context,
        const char			     *target_uri_string);
gboolean		    caja_drag_drag_data_get			(GtkWidget			      *widget,
        GdkDragContext			      *context,
        GtkSelectionData		      *selection_data,
        guint				       info,
        guint32			       time,
        gpointer			       container_context,
        CajaDragEachSelectedItemIterator  each_selected_item_iterator);
int			    caja_drag_modifier_based_action		(int				       default_action,
        int				       non_default_action);

GdkDragAction		    caja_drag_drop_action_ask		(GtkWidget			      *widget,
        GdkDragAction			       possible_actions);
GdkDragAction		    caja_drag_drop_background_ask		(GtkWidget			      *widget,
        GdkDragAction			       possible_actions);

gboolean		    caja_drag_autoscroll_in_scroll_region	(GtkWidget			      *widget);
void			    caja_drag_autoscroll_calculate_delta	(GtkWidget			      *widget,
        float				      *x_scroll_delta,
        float				      *y_scroll_delta);
void			    caja_drag_autoscroll_start		(CajaDragInfo		      *drag_info,
        GtkWidget			      *widget,
        GSourceFunc			       callback,
        gpointer			       user_data);
void			    caja_drag_autoscroll_stop		(CajaDragInfo		      *drag_info);

gboolean		    caja_drag_selection_includes_special_link (GList			      *selection_list);

void                        caja_drag_slot_proxy_init               (GtkWidget *widget,
        CajaDragSlotProxyInfo *drag_info);

#endif
