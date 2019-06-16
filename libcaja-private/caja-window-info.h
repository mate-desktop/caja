/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-window-info.h: Interface for caja windows

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

#ifndef CAJA_WINDOW_INFO_H
#define CAJA_WINDOW_INFO_H

#include <glib-object.h>

#include <gtk/gtk.h>

#include "caja-view.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum
    {
        CAJA_WINDOW_SHOW_HIDDEN_FILES_DEFAULT,
        CAJA_WINDOW_SHOW_HIDDEN_FILES_ENABLE,
        CAJA_WINDOW_SHOW_HIDDEN_FILES_DISABLE
    }
    CajaWindowShowHiddenFilesMode;

    typedef enum
    {
        CAJA_WINDOW_SHOW_BACKUP_FILES_DEFAULT,
        CAJA_WINDOW_SHOW_BACKUP_FILES_ENABLE,
        CAJA_WINDOW_SHOW_BACKUP_FILES_DISABLE
    }
    CajaWindowShowBackupFilesMode;

    typedef enum
    {
        CAJA_WINDOW_OPEN_ACCORDING_TO_MODE,
        CAJA_WINDOW_OPEN_IN_SPATIAL,
        CAJA_WINDOW_OPEN_IN_NAVIGATION
    } CajaWindowOpenMode;

    typedef enum
    {
        /* used in spatial mode */
        CAJA_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1<<0,
        /* used in navigation mode */
        CAJA_WINDOW_OPEN_FLAG_NEW_WINDOW = 1<<1,
        CAJA_WINDOW_OPEN_FLAG_NEW_TAB = 1<<2
    } CajaWindowOpenFlags;

    typedef	enum
    {
        CAJA_WINDOW_SPATIAL,
        CAJA_WINDOW_NAVIGATION,
        CAJA_WINDOW_DESKTOP
    } CajaWindowType;

#define CAJA_TYPE_WINDOW_INFO           (caja_window_info_get_type ())
#define CAJA_WINDOW_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_WINDOW_INFO, CajaWindowInfo))
#define CAJA_IS_WINDOW_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_WINDOW_INFO))
#define CAJA_WINDOW_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CAJA_TYPE_WINDOW_INFO, CajaWindowInfoIface))

#ifndef CAJA_WINDOW_DEFINED
#define CAJA_WINDOW_DEFINED
    /* Using CajaWindow for the vtable to make implementing this in
     * CajaWindow easier */
    typedef struct CajaWindow          CajaWindow;
#endif

#ifndef CAJA_WINDOW_SLOT_DEFINED
#define CAJA_WINDOW_SLOT_DEFINED
    typedef struct CajaWindowSlot      CajaWindowSlot;
#endif


    typedef CajaWindowSlot              CajaWindowSlotInfo;
    typedef CajaWindow                  CajaWindowInfo;

    typedef struct _CajaWindowInfoIface CajaWindowInfoIface;

    typedef void (* CajaWindowGoToCallback) (CajaWindow *window,
    					     GError *error,
    					     gpointer user_data);

    struct _CajaWindowInfoIface
    {
        GTypeInterface g_iface;

        /* signals: */

        void           (* loading_uri)              (CajaWindowInfo *window,
                const char        *uri);
        /* Emitted when the view in the window changes the selection */
        void           (* selection_changed)        (CajaWindowInfo *window);
        void           (* title_changed)            (CajaWindowInfo *window,
                const char         *title);
        void           (* hidden_files_mode_changed)(CajaWindowInfo *window);
	void           (* backup_files_mode_changed)(CajaWindowInfo *window);

        /* VTable: */
        /* A view calls this once after a load_location, once it starts loading the
         * directory. Might be called directly, or later on the mainloop.
         * This can also be called at any other time if the view needs to
         * re-load the location. But the view needs to call load_complete first if
         * its currently loading. */
        void (* report_load_underway) (CajaWindowInfo *window,
                                       CajaView *view);
        /* A view calls this once after reporting load_underway, when the location
           has been fully loaded, or when the load was stopped
           (by an error or by the user). */
        void (* report_load_complete) (CajaWindowInfo *window,
                                       CajaView *view);
        /* This can be called at any time when there has been a catastrophic failure of
           the view. It will result in the view being removed. */
        void (* report_view_failed)   (CajaWindowInfo *window,
                                       CajaView *view);
        void (* report_selection_changed) (CajaWindowInfo *window);

        /* Returns the number of selected items in the view */
        int  (* get_selection_count)  (CajaWindowInfo    *window);

        /* Returns a list of uris for th selected items in the view, caller frees it */
        GList *(* get_selection)      (CajaWindowInfo    *window);

        char * (* get_current_location)  (CajaWindowInfo *window);
        void   (* push_status)           (CajaWindowInfo *window,
                                          const char *status);
        char * (* get_title)             (CajaWindowInfo *window);
        GList *(* get_history)           (CajaWindowInfo *window);
        CajaWindowType
        (* get_window_type)       (CajaWindowInfo *window);
        CajaWindowShowHiddenFilesMode
        (* get_hidden_files_mode) (CajaWindowInfo *window);
        void   (* set_hidden_files_mode) (CajaWindowInfo *window,
                                          CajaWindowShowHiddenFilesMode mode);
        CajaWindowShowBackupFilesMode
        (* get_backup_files_mode) (CajaWindowInfo *window);
        void   (* set_backup_files_mode) (CajaWindowInfo *window,
                                          CajaWindowShowBackupFilesMode mode);

        CajaWindowSlotInfo * (* get_active_slot) (CajaWindowInfo *window);
        CajaWindowSlotInfo * (* get_extra_slot)  (CajaWindowInfo *window);

        gboolean (* get_initiated_unmount) (CajaWindowInfo *window);
        void   (* set_initiated_unmount) (CajaWindowInfo *window,
                                          gboolean initiated_unmount);

        void   (* view_visible)        (CajaWindowInfo *window,
                                        CajaView *view);
        void   (* close_window)       (CajaWindowInfo *window);
        GtkUIManager *     (* get_ui_manager)   (CajaWindowInfo *window);
    };

    GType                             caja_window_info_get_type                 (void);
    void                              caja_window_info_report_load_underway     (CajaWindowInfo                *window,
            CajaView                      *view);
    void                              caja_window_info_report_load_complete     (CajaWindowInfo                *window,
            CajaView                      *view);
    void                              caja_window_info_report_view_failed       (CajaWindowInfo                *window,
            CajaView                      *view);
    void                              caja_window_info_report_selection_changed (CajaWindowInfo                *window);
    CajaWindowSlotInfo *          caja_window_info_get_active_slot          (CajaWindowInfo                *window);
    CajaWindowSlotInfo *          caja_window_info_get_extra_slot           (CajaWindowInfo                *window);
    void                              caja_window_info_view_visible             (CajaWindowInfo                *window,
            CajaView                      *view);
    void                              caja_window_info_close                    (CajaWindowInfo                *window);
    void                              caja_window_info_push_status              (CajaWindowInfo                *window,
            const char                        *status);
    CajaWindowType                caja_window_info_get_window_type          (CajaWindowInfo                *window);
    char *                            caja_window_info_get_title                (CajaWindowInfo                *window);
    GList *                           caja_window_info_get_history              (CajaWindowInfo                *window);
    char *                            caja_window_info_get_current_location     (CajaWindowInfo                *window);
    int                               caja_window_info_get_selection_count      (CajaWindowInfo                *window);
    GList *                           caja_window_info_get_selection            (CajaWindowInfo                *window);
    CajaWindowShowHiddenFilesMode caja_window_info_get_hidden_files_mode    (CajaWindowInfo                *window);
    void                              caja_window_info_set_hidden_files_mode    (CajaWindowInfo                *window,
            CajaWindowShowHiddenFilesMode  mode);
    CajaWindowShowBackupFilesMode     caja_window_info_get_backup_files_mode    (CajaWindowInfo                *window);
    void                              caja_window_info_set_backup_files_mode    (CajaWindowInfo                *window,
            CajaWindowShowBackupFilesMode  mode);

    gboolean                          caja_window_info_get_initiated_unmount    (CajaWindowInfo                *window);
    void                              caja_window_info_set_initiated_unmount    (CajaWindowInfo                *window,
            gboolean initiated_unmount);
    GtkUIManager *                    caja_window_info_get_ui_manager           (CajaWindowInfo                *window);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_WINDOW_INFO_H */
