/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef FM_DIRECTORY_VIEW_H
#define FM_DIRECTORY_VIEW_H

#include <gtk/gtk.h>
#include <eel/eel-background.h>
#include <libcaja-private/caja-directory.h>
#include <libcaja-private/caja-file.h>
#include <libcaja-private/caja-icon-container.h>
#include <libcaja-private/caja-link.h>
#include <libcaja-private/caja-view.h>
#include <libcaja-private/caja-window-info.h>
#include <libcaja-private/caja-window-slot-info.h>
#include <gio/gio.h>

typedef struct FMDirectoryView FMDirectoryView;
typedef struct FMDirectoryViewClass FMDirectoryViewClass;

#define FM_TYPE_DIRECTORY_VIEW fm_directory_view_get_type()
#define FM_DIRECTORY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FM_TYPE_DIRECTORY_VIEW, FMDirectoryView))
#define FM_DIRECTORY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), FM_TYPE_DIRECTORY_VIEW, FMDirectoryViewClass))
#define FM_IS_DIRECTORY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_DIRECTORY_VIEW))
#define FM_IS_DIRECTORY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), FM_TYPE_DIRECTORY_VIEW))
#define FM_DIRECTORY_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FM_TYPE_DIRECTORY_VIEW, FMDirectoryViewClass))

typedef struct FMDirectoryViewDetails FMDirectoryViewDetails;

struct FMDirectoryView
{
    GtkScrolledWindow parent;
    FMDirectoryViewDetails *details;
};

struct FMDirectoryViewClass
{
    GtkScrolledWindowClass parent_class;

    /* The 'clear' signal is emitted to empty the view of its contents.
     * It must be replaced by each subclass.
     */
    void 	(* clear) 		 (FMDirectoryView *view);

    /* The 'begin_file_changes' signal is emitted before a set of files
     * are added to the view. It can be replaced by a subclass to do any
     * necessary preparation for a set of new files. The default
     * implementation does nothing.
     */
    void 	(* begin_file_changes) (FMDirectoryView *view);

    /* The 'add_file' signal is emitted to add one file to the view.
     * It must be replaced by each subclass.
     */
    void    (* add_file) 		 (FMDirectoryView *view,
                                  CajaFile *file,
                                  CajaDirectory *directory);
    void    (* remove_file)		 (FMDirectoryView *view,
                                  CajaFile *file,
                                  CajaDirectory *directory);

    /* The 'file_changed' signal is emitted to signal a change in a file,
     * including the file being removed.
     * It must be replaced by each subclass.
     */
    void 	(* file_changed)         (FMDirectoryView *view,
                                      CajaFile *file,
                                      CajaDirectory *directory);

    /* The 'end_file_changes' signal is emitted after a set of files
     * are added to the view. It can be replaced by a subclass to do any
     * necessary cleanup (typically, cleanup for code in begin_file_changes).
     * The default implementation does nothing.
     */
    void 	(* end_file_changes)    (FMDirectoryView *view);

    void    (* flush_added_files)	 (FMDirectoryView *view);

    /* The 'begin_loading' signal is emitted before any of the contents
     * of a directory are added to the view. It can be replaced by a
     * subclass to do any necessary preparation to start dealing with a
     * new directory. The default implementation does nothing.
     */
    void 	(* begin_loading) 	 (FMDirectoryView *view);

    /* The 'end_loading' signal is emitted after all of the contents
     * of a directory are added to the view. It can be replaced by a
     * subclass to do any necessary clean-up. The default implementation
     * does nothing.
     *
     * If all_files_seen is true, the handler may assume that
     * no load error ocurred, and all files of the underlying
     * directory were loaded.
     *
     * Otherwise, end_loading was emitted due to cancellation,
     * which usually means that not all files are available.
     */
    void 	(* end_loading) 	 (FMDirectoryView *view,
                                  gboolean all_files_seen);

    /* The 'load_error' signal is emitted when the directory model
     * reports an error in the process of monitoring the directory's
     * contents.  The load error indicates that the process of
     * loading the contents has ended, but the directory is still
     * being monitored. The default implementation handles common
     * load failures like ACCESS_DENIED.
     */
    void    (* load_error)           (FMDirectoryView *view,
                                      GError *error);

    /* Function pointers that don't have corresponding signals */

    /* reset_to_defaults is a function pointer that subclasses must
     * override to set sort order, zoom level, etc to match default
     * values.
     */
    void     (* reset_to_defaults)	         (FMDirectoryView *view);

    /* get_selection is not a signal; it is just a function pointer for
     * subclasses to replace (override). Subclasses must replace it
     * with a function that returns a newly-allocated GList of
     * CajaFile pointers.
     */
    GList *	(* get_selection) 	 	(FMDirectoryView *view);

    /* get_selection_for_file_transfer  is a function pointer for
     * subclasses to replace (override). Subclasses must replace it
     * with a function that returns a newly-allocated GList of
     * CajaFile pointers. The difference from get_selection is
     * that any files in the selection that also has a parent folder
     * in the selection is not included.
     */
    GList *	(* get_selection_for_file_transfer)(FMDirectoryView *view);

    /* select_all is a function pointer that subclasses must override to
     * select all of the items in the view */
    void     (* select_all)	         	(FMDirectoryView *view);

    /* set_selection is a function pointer that subclasses must
     * override to select the specified items (and unselect all
     * others). The argument is a list of CajaFiles. */

    void     (* set_selection)	 	(FMDirectoryView *view,
                                     GList *selection);

    /* invert_selection is a function pointer that subclasses must
     * override to invert selection. */

    void     (* invert_selection)	 	(FMDirectoryView *view);

    /* Return an array of locations of selected icons in their view. */
    GArray * (* get_selected_icon_locations) (FMDirectoryView *view);

    guint    (* get_item_count)             (FMDirectoryView *view);

    /* bump_zoom_level is a function pointer that subclasses must override
     * to change the zoom level of an object. */
    void    (* bump_zoom_level)      	(FMDirectoryView *view,
                                         int zoom_increment);

    /* zoom_to_level is a function pointer that subclasses must override
     * to set the zoom level of an object to the specified level. */
    void    (* zoom_to_level) 		(FMDirectoryView *view,
                                     CajaZoomLevel level);

    CajaZoomLevel (* get_zoom_level)    (FMDirectoryView *view);

    /* restore_default_zoom_level is a function pointer that subclasses must override
         * to restore the zoom level of an object to a default setting. */
    void    (* restore_default_zoom_level) (FMDirectoryView *view);

    /* can_zoom_in is a function pointer that subclasses must override to
     * return whether the view is at maximum size (furthest-in zoom level) */
    gboolean (* can_zoom_in)	 	(FMDirectoryView *view);

    /* can_zoom_out is a function pointer that subclasses must override to
     * return whether the view is at minimum size (furthest-out zoom level) */
    gboolean (* can_zoom_out)	 	(FMDirectoryView *view);

    /* reveal_selection is a function pointer that subclasses may
     * override to make sure the selected items are sufficiently
     * apparent to the user (e.g., scrolled into view). By default,
     * this does nothing.
     */
    void     (* reveal_selection)	 	(FMDirectoryView *view);

    /* get_background is a function pointer that subclasses must
     * override to return the EelBackground for this view.
     */
    GtkWidget * (* get_background_widget)	(FMDirectoryView *view);

    /* merge_menus is a function pointer that subclasses can override to
     * add their own menu items to the window's menu bar.
     * If overridden, subclasses must call parent class's function.
     */
    void    (* merge_menus)         	(FMDirectoryView *view);
    void    (* unmerge_menus)         	(FMDirectoryView *view);

    /* update_menus is a function pointer that subclasses can override to
     * update the sensitivity or wording of menu items in the menu bar.
     * It is called (at least) whenever the selection changes. If overridden,
     * subclasses must call parent class's function.
     */
    void    (* update_menus)         	(FMDirectoryView *view);

    /* sort_files is a function pointer that subclasses can override
     * to provide a sorting order to determine which files should be
     * presented when only a partial list is provided.
     */
    int     (* compare_files)              (FMDirectoryView *view,
                                            CajaFile    *a,
                                            CajaFile    *b);

    /* get_emblem_names_to_exclude is a function pointer that subclasses
     * may override to specify a set of emblem names that should not
     * be displayed with each file. By default, all emblems returned by
     * CajaFile are displayed.
     */
    char ** (* get_emblem_names_to_exclude)	(FMDirectoryView *view);

    /* supports_properties is a function pointer that subclasses may
     * override to control whether the "Show Properties" menu item
     * should be enabled for selected items. The default implementation
     * returns TRUE.
     */
    gboolean (* supports_properties)	(FMDirectoryView *view);

    /* supports_zooming is a function pointer that subclasses may
     * override to control whether or not the zooming control and
     * menu items should be enabled. The default implementation
     * returns TRUE.
     */
    gboolean (* supports_zooming)		(FMDirectoryView *view);

    /* using_manual_layout is a function pointer that subclasses may
     * override to control whether or not items can be freely positioned
     * on the user-visible area.
     * Note that this value is not guaranteed to be constant within the
     * view's lifecycle. */
    gboolean (* using_manual_layout)     (FMDirectoryView *view);

    /* is_read_only is a function pointer that subclasses may
     * override to control whether or not the user is allowed to
     * change the contents of the currently viewed directory. The
     * default implementation checks the permissions of the
     * directory.
     */
    gboolean (* is_read_only)	        (FMDirectoryView *view);

    /* is_empty is a function pointer that subclasses must
     * override to report whether the view contains any items.
     */
    gboolean (* is_empty)                   (FMDirectoryView *view);

    /* supports_creating_files is a function pointer that subclasses may
     * override to control whether or not new items can be created.
     * be accepted. The default implementation checks whether the
     * user has write permissions for the viewed directory, and whether
     * the viewed directory is in the trash.
     */
    gboolean (* supports_creating_files)	(FMDirectoryView *view);

    /* accepts_dragged_files is a function pointer that subclasses may
     * override to control whether or not files can be dropped in this
     * location. The default implementation returns TRUE.
     */
    gboolean (* accepts_dragged_files)	(FMDirectoryView *view);

    gboolean (* can_rename_file)            (FMDirectoryView *view,
            CajaFile *file);
    /* select_all specifies whether the whole filename should be selected
     * or only its basename (i.e. everything except the extension)
     * */
    void	 (* start_renaming_file)        (FMDirectoryView *view,
            CajaFile *file,
            gboolean select_all);

    gboolean (* file_still_belongs)		(FMDirectoryView *view,
                                         CajaFile	 *file,
                                         CajaDirectory *directory);

    /* convert *point from widget's coordinate system to a coordinate
     * system used for specifying file operation positions, which is view-specific.
     *
     * This is used by the the icon view, which converts the screen position to a zoom
     * level-independent coordinate system.
     */
    void (* widget_to_file_operation_position) (FMDirectoryView *view,
            GdkPoint        *position);

    /* Preference change callbacks, overriden by icon and list views.
     * Icon and list views respond by synchronizing to the new preference
     * values and forcing an update if appropriate.
     */
    void	(* text_attribute_names_changed)   (FMDirectoryView *view);
    void	(* embedded_text_policy_changed)   (FMDirectoryView *view);
    void	(* image_display_policy_changed)   (FMDirectoryView *view);
    void	(* click_policy_changed)	   (FMDirectoryView *view);
    void	(* sort_directories_first_changed) (FMDirectoryView *view);

    void	(* emblems_changed)                (FMDirectoryView *view);

    void    (* set_is_active)                  (FMDirectoryView *view,
            gboolean         is_active);

    /* Signals used only for keybindings */
    gboolean (* trash)                         (FMDirectoryView *view);
    gboolean (* delete)                        (FMDirectoryView *view);
};

/* GObject support */
GType               fm_directory_view_get_type                         (void);

/* Functions callable from the user interface and elsewhere. */
CajaWindowInfo *fm_directory_view_get_caja_window              (FMDirectoryView  *view);
CajaWindowSlotInfo *fm_directory_view_get_caja_window_slot     (FMDirectoryView  *view);
char *              fm_directory_view_get_uri                          (FMDirectoryView  *view);
char *              fm_directory_view_get_backing_uri                  (FMDirectoryView  *view);
gboolean            fm_directory_view_can_accept_item                  (CajaFile     *target_item,
        const char       *item_uri,
        FMDirectoryView  *view);
void                fm_directory_view_display_selection_info           (FMDirectoryView  *view);
GList *             fm_directory_view_get_selection                    (FMDirectoryView  *view);
GList *             fm_directory_view_get_selection_for_file_transfer  (FMDirectoryView  *view);
void                fm_directory_view_invert_selection                 (FMDirectoryView  *view);
void                fm_directory_view_stop                             (FMDirectoryView  *view);
guint               fm_directory_view_get_item_count                   (FMDirectoryView  *view);
gboolean            fm_directory_view_can_zoom_in                      (FMDirectoryView  *view);
gboolean            fm_directory_view_can_zoom_out                     (FMDirectoryView  *view);
GtkWidget *         fm_directory_view_get_background_widget            (FMDirectoryView  *view);
void                fm_directory_view_bump_zoom_level                  (FMDirectoryView  *view,
        int               zoom_increment);
void                fm_directory_view_zoom_to_level                    (FMDirectoryView  *view,
        CajaZoomLevel zoom_level);
CajaZoomLevel   fm_directory_view_get_zoom_level                   (FMDirectoryView  *view);
void                fm_directory_view_restore_default_zoom_level       (FMDirectoryView  *view);
void                fm_directory_view_reset_to_defaults                (FMDirectoryView  *view);
void                fm_directory_view_select_all                       (FMDirectoryView  *view);
void                fm_directory_view_set_selection                    (FMDirectoryView  *view,
        GList            *selection);
GArray *            fm_directory_view_get_selected_icon_locations      (FMDirectoryView  *view);
void                fm_directory_view_reveal_selection                 (FMDirectoryView  *view);
gboolean            fm_directory_view_is_empty                         (FMDirectoryView  *view);
gboolean            fm_directory_view_is_read_only                     (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_creating_files          (FMDirectoryView  *view);
gboolean            fm_directory_view_accepts_dragged_files            (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_properties              (FMDirectoryView  *view);
gboolean            fm_directory_view_supports_zooming                 (FMDirectoryView  *view);
gboolean            fm_directory_view_using_manual_layout              (FMDirectoryView  *view);
void                fm_directory_view_move_copy_items                  (const GList      *item_uris,
        GArray           *relative_item_points,
        const char       *target_uri,
        int               copy_action,
        int               x,
        int               y,
        FMDirectoryView  *view);
GdkAtom	            fm_directory_view_get_copied_files_atom            (FMDirectoryView  *view);
gboolean            fm_directory_view_get_active                       (FMDirectoryView  *view);

/* Wrappers for signal emitters. These are normally called
 * only by FMDirectoryView itself. They have corresponding signals
 * that observers might want to connect with.
 */
void                fm_directory_view_clear                            (FMDirectoryView  *view);
void                fm_directory_view_begin_loading                    (FMDirectoryView  *view);
void                fm_directory_view_end_loading                      (FMDirectoryView  *view,
        gboolean          all_files_seen);

gboolean            fm_directory_view_get_loading                      (FMDirectoryView  *view);

/* Hooks for subclasses to call. These are normally called only by
 * FMDirectoryView and its subclasses
 */
void                fm_directory_view_activate_files                   (FMDirectoryView        *view,
        GList                  *files,
        CajaWindowOpenMode  mode,
        CajaWindowOpenFlags flags,
        gboolean                confirm_multiple);
void                fm_directory_view_activate_file                    (FMDirectoryView        *view,
        CajaFile           *file,
        CajaWindowOpenMode  mode,
        CajaWindowOpenFlags flags);
void                fm_directory_view_start_batching_selection_changes (FMDirectoryView  *view);
void                fm_directory_view_stop_batching_selection_changes  (FMDirectoryView  *view);
void                fm_directory_view_queue_file_change                (FMDirectoryView  *view,
        CajaFile     *file);
void                fm_directory_view_notify_selection_changed         (FMDirectoryView  *view);
GtkUIManager *      fm_directory_view_get_ui_manager                   (FMDirectoryView  *view);
char **             fm_directory_view_get_emblem_names_to_exclude      (FMDirectoryView  *view);
CajaDirectory  *fm_directory_view_get_model                        (FMDirectoryView  *view);
GtkWindow	   *fm_directory_view_get_containing_window	       (FMDirectoryView  *view);
CajaFile       *fm_directory_view_get_directory_as_file            (FMDirectoryView  *view);
EelBackground *     fm_directory_view_get_background                   (FMDirectoryView  *view);
gboolean            fm_directory_view_get_allow_moves                  (FMDirectoryView  *view);
void                fm_directory_view_pop_up_background_context_menu   (FMDirectoryView  *view,
        GdkEventButton   *event);
void                fm_directory_view_pop_up_selection_context_menu    (FMDirectoryView  *view,
        GdkEventButton   *event);
void                fm_directory_view_pop_up_location_context_menu     (FMDirectoryView  *view,
        GdkEventButton   *event,
        const char       *location);
void                fm_directory_view_send_selection_change            (FMDirectoryView *view);
gboolean            fm_directory_view_should_show_file                 (FMDirectoryView  *view,
        CajaFile     *file);
gboolean	    fm_directory_view_should_sort_directories_first    (FMDirectoryView  *view);
void                fm_directory_view_update_menus                     (FMDirectoryView  *view);
void                fm_directory_view_new_folder                       (FMDirectoryView  *view);
void                fm_directory_view_new_file                         (FMDirectoryView  *view,
        const char       *parent_uri,
        CajaFile     *source);
void                fm_directory_view_ignore_hidden_file_preferences   (FMDirectoryView  *view);
void                fm_directory_view_set_show_foreign                 (FMDirectoryView  *view,
        gboolean          show_foreign);
void                fm_directory_view_init_view_iface                  (CajaViewIface *iface);
gboolean            fm_directory_view_handle_scroll_event              (FMDirectoryView  *view,
        GdkEventScroll   *event);
void                fm_directory_view_handle_netscape_url_drop         (FMDirectoryView  *view,
        const char       *encoded_url,
        const char       *target_uri,
        GdkDragAction     action,
        int               x,
        int               y);
void                fm_directory_view_handle_uri_list_drop             (FMDirectoryView  *view,
        const char       *item_uris,
        const char       *target_uri,
        GdkDragAction     action,
        int               x,
        int               y);
void                fm_directory_view_handle_text_drop                 (FMDirectoryView  *view,
        const char       *text,
        const char       *target_uri,
        GdkDragAction     action,
        int               x,
        int               y);
void                fm_directory_view_handle_raw_drop                 (FMDirectoryView  *view,
        const char       *raw_data,
        int               length,
        const char       *target_uri,
        const char       *direct_save_uri,
        GdkDragAction     action,
        int               x,
        int               y);
void                fm_directory_view_freeze_updates                   (FMDirectoryView  *view);
void                fm_directory_view_unfreeze_updates                 (FMDirectoryView  *view);
void                fm_directory_view_add_subdirectory                (FMDirectoryView  *view,
        CajaDirectory*directory);
void                fm_directory_view_remove_subdirectory             (FMDirectoryView  *view,
        CajaDirectory*directory);

gboolean            fm_directory_view_is_editable                     (FMDirectoryView *view);
void		    fm_directory_view_set_initiated_unmount	      (FMDirectoryView *view,
        gboolean inititated_unmount);

/* operations affecting two directory views */
void                fm_directory_view_move_copy_items_between_views   (FMDirectoryView *source, FMDirectoryView *target, gboolean copy);

#endif /* FM_DIRECTORY_VIEW_H */
