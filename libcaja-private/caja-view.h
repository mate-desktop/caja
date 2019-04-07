/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-view.h: Interface for caja views

   Copyright (C) 2004 Red Hat Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef CAJA_VIEW_H
#define CAJA_VIEW_H

#include <glib-object.h>
#include <gtk/gtk.h>

/* For CajaZoomLevel */
#include "caja-icon-info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_VIEW           (caja_view_get_type ())
#define CAJA_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_VIEW, CajaView))
#define CAJA_IS_VIEW(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_VIEW))
#define CAJA_VIEW_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_VIEW, CajaViewIface))


    typedef struct _CajaView CajaView; /* dummy typedef */
    typedef struct _CajaViewIface CajaViewIface;

    struct _CajaViewIface
    {
        GTypeInterface g_iface;

        /* Signals: */

        /* emitted when the view-specific title as returned by get_title changes */
        void           (* title_changed)          (CajaView          *view);

        void           (* zoom_level_changed)     (CajaView          *view);

        /* VTable: */

        /* Get the id string for this view. Its a constant string, not memory managed */
        const char *   (* get_view_id)            (CajaView          *view);

        /* Get the widget for this view, can be the same object or a different
           object owned by the view. Doesn't ref the widget. */
        GtkWidget *    (* get_widget)             (CajaView          *view);

        /* Called to tell the view to start loading a location, or to reload it.
           The view responds with a load_underway as soon as it starts loading,
           and a load_complete when the location is completely read. */
        void           (* load_location)          (CajaView          *view,
                const char            *location_uri);

        /* Called to tell the view to stop loading the location its currently loading */
        void           (* stop_loading)           (CajaView          *view);

        /* Returns the number of selected items in the view */
        int            (* get_selection_count)    (CajaView          *view);

        /* Returns a list of uris for th selected items in the view, caller frees it */
        GList *        (* get_selection)          (CajaView          *view);

        /* This is called when the window wants to change the selection in the view */
        void           (* set_selection)          (CajaView          *view,
                GList                 *list);

        /* Inverts the selection in the view */
        void           (* invert_selection)       (CajaView          *view);

        /* Return the uri of the first visible file */
        char *         (* get_first_visible_file) (CajaView          *view);
        /* Scroll the view so that the file specified by the uri is at the top
           of the view */
        void           (* scroll_to_file)	  (CajaView          *view,
                                               const char            *uri);

        /* This function can supply a special window title, if you don't want one
           have this function return NULL, or just don't supply a function  */
        char *         (* get_title)              (CajaView          *view);


        /* Zoom support */
        gboolean       (* supports_zooming)       (CajaView          *view);
        void           (* bump_zoom_level)     	  (CajaView          *view,
                int                    zoom_increment);
        void           (* zoom_to_level) 	  (CajaView          *view,
                                               CajaZoomLevel     level);
        CajaZoomLevel (* get_zoom_level) 	  (CajaView          *view);
        void           (* restore_default_zoom_level) (CajaView          *view);
        gboolean       (* can_zoom_in)	 	  (CajaView          *view);
        gboolean       (* can_zoom_out)	 	  (CajaView          *view);

        void           (* grab_focus)             (CajaView          *view);
        void           (* update_menus)           (CajaView          *view);

        /* Request popup of context menu referring to the open location.
         * This is triggered in spatial windows by right-clicking the location button,
         * in navigational windows by right-clicking the "Location:" label in the
         * navigation bar or any of the buttons in the pathbar.
         * The location parameter specifies the location this popup should be displayed for.
         * If it is NULL, the currently displayed location should be used.
         * The view may display the popup synchronously, asynchronously
         * or not react to the popup request at all. */
        void           (* pop_up_location_context_menu) (CajaView   *view,
                GdkEventButton *event,
                const char     *location);

        void           (* drop_proxy_received_uris)     (CajaView         *view,
                GList                *uris,
                const char           *target_location,
                GdkDragAction         action);
        void           (* drop_proxy_received_netscape_url) (CajaView         *view,
                const char           *source_url,
                const char           *target_location,
                GdkDragAction         action);
        void           (* set_is_active)                    (CajaView         *view,
                gboolean              is_active);

        /* Padding for future expansion */
        void (*_reserved1) (void);
        void (*_reserved2) (void);
        void (*_reserved3) (void);
        void (*_reserved4) (void);
        void (*_reserved5) (void);
        void (*_reserved6) (void);
        void (*_reserved7) (void);
    };

    GType             caja_view_get_type             (void);

    const char *      caja_view_get_view_id                (CajaView      *view);
    GtkWidget *       caja_view_get_widget                 (CajaView      *view);
    void              caja_view_load_location              (CajaView      *view,
            const char        *location_uri);
    void              caja_view_stop_loading               (CajaView      *view);
    int               caja_view_get_selection_count        (CajaView      *view);
    GList *           caja_view_get_selection              (CajaView      *view);
    void              caja_view_set_selection              (CajaView      *view,
            GList             *list);
    void              caja_view_invert_selection           (CajaView      *view);
    char *            caja_view_get_first_visible_file     (CajaView      *view);
    void              caja_view_scroll_to_file             (CajaView      *view,
            const char        *uri);
    char *            caja_view_get_title                  (CajaView      *view);
    gboolean          caja_view_supports_zooming           (CajaView      *view);
    void              caja_view_bump_zoom_level            (CajaView      *view,
            int                zoom_increment);
    void              caja_view_zoom_to_level              (CajaView      *view,
            CajaZoomLevel  level);
    void              caja_view_restore_default_zoom_level (CajaView      *view);
    gboolean          caja_view_can_zoom_in                (CajaView      *view);
    gboolean          caja_view_can_zoom_out               (CajaView      *view);
    CajaZoomLevel caja_view_get_zoom_level             (CajaView      *view);
    void              caja_view_pop_up_location_context_menu (CajaView    *view,
            GdkEventButton  *event,
            const char      *location);
    void              caja_view_grab_focus                 (CajaView      *view);
    void              caja_view_update_menus               (CajaView      *view);
    void              caja_view_drop_proxy_received_uris   (CajaView         *view,
            GList                *uris,
            const char           *target_location,
            GdkDragAction         action);
    void              caja_view_drop_proxy_received_netscape_url (CajaView         *view,
            const char           *source_url,
            const char           *target_location,
            GdkDragAction         action);
    void              caja_view_set_is_active              (CajaView      *view,
            gboolean           is_active);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_VIEW_H */
