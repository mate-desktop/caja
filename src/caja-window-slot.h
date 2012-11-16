/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-slot.h: Caja window slot

   Copyright (C) 2008 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef CAJA_WINDOW_SLOT_H
#define CAJA_WINDOW_SLOT_H

#include "caja-window-pane.h"
#include "caja-query-editor.h"
#include <glib/gi18n.h>

#define CAJA_TYPE_WINDOW_SLOT	 (caja_window_slot_get_type())
#define CAJA_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_WINDOW_SLOT, CajaWindowSlotClass))
#define CAJA_WINDOW_SLOT(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WINDOW_SLOT, CajaWindowSlot))
#define CAJA_IS_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WINDOW_SLOT))
#define CAJA_IS_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_WINDOW_SLOT))
#define CAJA_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_WINDOW_SLOT, CajaWindowSlotClass))

typedef enum
{
    CAJA_LOCATION_CHANGE_STANDARD,
    CAJA_LOCATION_CHANGE_BACK,
    CAJA_LOCATION_CHANGE_FORWARD,
    CAJA_LOCATION_CHANGE_RELOAD,
    CAJA_LOCATION_CHANGE_REDIRECT,
    CAJA_LOCATION_CHANGE_FALLBACK
} CajaLocationChangeType;

struct CajaWindowSlotClass
{
    GObjectClass parent_class;

    /* wrapped CajaWindowInfo signals, for overloading */
    void (* active)   (CajaWindowSlot *slot);
    void (* inactive) (CajaWindowSlot *slot);

    void (* update_query_editor) (CajaWindowSlot *slot);
};

/* Each CajaWindowSlot corresponds to
 * a location in the window for displaying
 * a CajaView.
 *
 * For navigation windows, this would be a
 * tab, while spatial windows only have one slot.
 */
struct CajaWindowSlot
{
    GObject parent;

    CajaWindowPane *pane;

    /* content_box contains
     *  1) an event box containing extra_location_widgets
     *  2) the view box for the content view
     */
    GtkWidget *content_box;
    GtkWidget *extra_location_frame;
    GtkWidget *extra_location_widgets;
    GtkWidget *view_box;

    CajaView *content_view;
    CajaView *new_content_view;

    /* Information about bookmarks */
    CajaBookmark *current_location_bookmark;
    CajaBookmark *last_location_bookmark;

    /* Current location. */
    GFile *location;
    char *title;
    char *status_text;

    CajaFile *viewed_file;
    gboolean viewed_file_seen;
    gboolean viewed_file_in_trash;

    gboolean allow_stop;

    CajaQueryEditor *query_editor;

    /* New location. */
    CajaLocationChangeType location_change_type;
    guint location_change_distance;
    GFile *pending_location;
    char *pending_scroll_to;
    GList *pending_selection;
    CajaFile *determine_view_file;
    GCancellable *mount_cancellable;
    GError *mount_error;
    gboolean tried_mount;
    CajaWindowGoToCallback open_callback;
    gpointer open_callback_user_data;

    GCancellable *find_mount_cancellable;

    gboolean visible;
};

GType   caja_window_slot_get_type (void);

char *  caja_window_slot_get_title			   (CajaWindowSlot *slot);
void    caja_window_slot_update_title		   (CajaWindowSlot *slot);
void    caja_window_slot_update_icon		   (CajaWindowSlot *slot);
void    caja_window_slot_update_query_editor	   (CajaWindowSlot *slot);

GFile * caja_window_slot_get_location		   (CajaWindowSlot *slot);
char *  caja_window_slot_get_location_uri		   (CajaWindowSlot *slot);

void    caja_window_slot_close			   (CajaWindowSlot *slot);
void    caja_window_slot_reload			   (CajaWindowSlot *slot);

void			caja_window_slot_open_location	      (CajaWindowSlot	*slot,
        GFile			*location,
        gboolean			 close_behind);
void			caja_window_slot_open_location_with_selection (CajaWindowSlot	    *slot,
        GFile		    *location,
        GList		    *selection,
        gboolean		     close_behind);
void			caja_window_slot_open_location_full       (CajaWindowSlot	*slot,
        GFile			*location,
        CajaWindowOpenMode	 mode,
        CajaWindowOpenFlags	 flags,
        GList			*new_selection,
        CajaWindowGoToCallback   callback,
        gpointer		 user_data);
void			caja_window_slot_stop_loading	      (CajaWindowSlot	*slot);

void			caja_window_slot_set_content_view	      (CajaWindowSlot	*slot,
        const char		*id);
const char	       *caja_window_slot_get_content_view_id      (CajaWindowSlot	*slot);
gboolean		caja_window_slot_content_view_matches_iid (CajaWindowSlot	*slot,
        const char		*iid);

void                    caja_window_slot_connect_content_view     (CajaWindowSlot       *slot,
        CajaView             *view);
void                    caja_window_slot_disconnect_content_view  (CajaWindowSlot       *slot,
        CajaView             *view);

#define caja_window_slot_go_to(slot,location, new_tab) \
	caja_window_slot_open_location_full(slot, location, CAJA_WINDOW_OPEN_ACCORDING_TO_MODE, \
						(new_tab ? CAJA_WINDOW_OPEN_FLAG_NEW_TAB : 0), \
						NULL, NULL, NULL)

#define caja_window_slot_go_to_full(slot, location, new_tab, callback, user_data) \
	caja_window_slot_open_location_full(slot, location, CAJA_WINDOW_OPEN_ACCORDING_TO_MODE, \
						(new_tab ? CAJA_WINDOW_OPEN_FLAG_NEW_TAB : 0), \
						NULL, callback, user_data)

#define caja_window_slot_go_to_with_selection(slot,location,new_selection) \
	caja_window_slot_open_location_with_selection(slot, location, new_selection, FALSE)

void    caja_window_slot_go_home			   (CajaWindowSlot *slot,
        gboolean            new_tab);
void    caja_window_slot_go_up			   (CajaWindowSlot *slot,
        gboolean           close_behind);

void    caja_window_slot_set_content_view_widget	   (CajaWindowSlot *slot,
        CajaView       *content_view);
void    caja_window_slot_set_viewed_file		   (CajaWindowSlot *slot,
        CajaFile      *file);
void    caja_window_slot_set_allow_stop		   (CajaWindowSlot *slot,
        gboolean	    allow_stop);
void    caja_window_slot_set_status			   (CajaWindowSlot *slot,
        const char	 *status);

void    caja_window_slot_add_extra_location_widget     (CajaWindowSlot *slot,
        GtkWidget       *widget);
void    caja_window_slot_remove_extra_location_widgets (CajaWindowSlot *slot);

void    caja_window_slot_add_current_location_to_history_list (CajaWindowSlot *slot);

void    caja_window_slot_is_in_active_pane (CajaWindowSlot *slot, gboolean is_active);

#endif /* CAJA_WINDOW_SLOT_H */
