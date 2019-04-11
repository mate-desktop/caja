/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* mate-icon-container.h - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#ifndef CAJA_ICON_CONTAINER_H
#define CAJA_ICON_CONTAINER_H

#include <eel/eel-canvas.h>

#include "caja-icon-info.h"

#define CAJA_TYPE_ICON_CONTAINER caja_icon_container_get_type()
#define CAJA_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_ICON_CONTAINER, CajaIconContainer))
#define CAJA_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_ICON_CONTAINER, CajaIconContainerClass))
#define CAJA_IS_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_ICON_CONTAINER))
#define CAJA_IS_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_ICON_CONTAINER))
#define CAJA_ICON_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_ICON_CONTAINER, CajaIconContainerClass))


#define CAJA_ICON_CONTAINER_ICON_DATA(pointer) \
	((CajaIconData *) (pointer))

typedef struct CajaIconData CajaIconData;

typedef void (* CajaIconCallback) (CajaIconData *icon_data,
                                   gpointer callback_data);

typedef struct
{
    int x;
    int y;
    double scale;
} CajaIconPosition;

typedef enum
{
    CAJA_ICON_LAYOUT_L_R_T_B,
    CAJA_ICON_LAYOUT_R_L_T_B,
    CAJA_ICON_LAYOUT_T_B_L_R,
    CAJA_ICON_LAYOUT_T_B_R_L
} CajaIconLayoutMode;

typedef enum
{
    CAJA_ICON_LABEL_POSITION_UNDER,
    CAJA_ICON_LABEL_POSITION_BESIDE
} CajaIconLabelPosition;

#define	CAJA_ICON_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct CajaIconContainerDetails CajaIconContainerDetails;

typedef struct
{
    EelCanvas canvas;
    CajaIconContainerDetails *details;
} CajaIconContainer;

typedef struct
{
    EelCanvasClass parent_slot;

    /* Operations on the container. */
    int          (* button_press) 	          (CajaIconContainer *container,
            GdkEventButton *event);
    void         (* context_click_background) (CajaIconContainer *container,
            GdkEventButton *event);
    void         (* middle_click) 		  (CajaIconContainer *container,
                                           GdkEventButton *event);

    /* Operations on icons. */
    void         (* activate)	  	  (CajaIconContainer *container,
                                       CajaIconData *data);
    void         (* activate_alternate)       (CajaIconContainer *container,
            CajaIconData *data);
    void         (* context_click_selection)  (CajaIconContainer *container,
            GdkEventButton *event);
    void	     (* move_copy_items)	  (CajaIconContainer *container,
                                           const GList *item_uris,
                                           GdkPoint *relative_item_points,
                                           const char *target_uri,
                                           GdkDragAction action,
                                           int x,
                                           int y);
    void	     (* handle_netscape_url)	  (CajaIconContainer *container,
            const char *url,
            const char *target_uri,
            GdkDragAction action,
            int x,
            int y);
    void	     (* handle_uri_list)    	  (CajaIconContainer *container,
            const char *uri_list,
            const char *target_uri,
            GdkDragAction action,
            int x,
            int y);
    void	     (* handle_text)		  (CajaIconContainer *container,
                                           const char *text,
                                           const char *target_uri,
                                           GdkDragAction action,
                                           int x,
                                           int y);
    void	     (* handle_raw)		  (CajaIconContainer *container,
                                       char *raw_data,
                                       int length,
                                       const char *target_uri,
                                       const char *direct_save_uri,
                                       GdkDragAction action,
                                       int x,
                                       int y);

    /* Queries on the container for subclass/client.
     * These must be implemented. The default "do nothing" is not good enough.
     */
    char *	     (* get_container_uri)	  (CajaIconContainer *container);

    /* Queries on icons for subclass/client.
     * These must be implemented. The default "do nothing" is not
     * good enough, these are _not_ signals.
     */
    CajaIconInfo *(* get_icon_images)     (CajaIconContainer *container,
                                           CajaIconData *data,
                                           int icon_size,
                                           GList **emblem_pixbufs,
                                           char **embedded_text,
                                           gboolean for_drag_accept,
                                           gboolean need_large_embeddded_text,
                                           gboolean *embedded_text_needs_loading,
                                           gboolean *has_window_open);
    void         (* get_icon_text)            (CajaIconContainer *container,
            CajaIconData *data,
            char **editable_text,
            char **additional_text,
            gboolean include_invisible);
    char *       (* get_icon_description)     (CajaIconContainer *container,
            CajaIconData *data);
    int          (* compare_icons)            (CajaIconContainer *container,
            CajaIconData *icon_a,
            CajaIconData *icon_b);
    int          (* compare_icons_by_name)    (CajaIconContainer *container,
            CajaIconData *icon_a,
            CajaIconData *icon_b);
    void         (* freeze_updates)           (CajaIconContainer *container);
    void         (* unfreeze_updates)         (CajaIconContainer *container);
    void         (* start_monitor_top_left)   (CajaIconContainer *container,
            CajaIconData *data,
            gconstpointer client,
            gboolean large_text);
    void         (* stop_monitor_top_left)    (CajaIconContainer *container,
            CajaIconData *data,
            gconstpointer client);
    void         (* prioritize_thumbnailing)  (CajaIconContainer *container,
            CajaIconData *data);

    /* Queries on icons for subclass/client.
     * These must be implemented => These are signals !
     * The default "do nothing" is not good enough.
     */
    gboolean     (* can_accept_item)	  (CajaIconContainer *container,
                                           CajaIconData *target,
                                           const char *item_uri);
    gboolean     (* get_stored_icon_position) (CajaIconContainer *container,
            CajaIconData *data,
            CajaIconPosition *position);
    char *       (* get_icon_uri)             (CajaIconContainer *container,
            CajaIconData *data);
    char *       (* get_icon_drop_target_uri) (CajaIconContainer *container,
            CajaIconData *data);

    /* If icon data is NULL, the layout timestamp of the container should be retrieved.
     * That is the time when the container displayed a fully loaded directory with
     * all icon positions assigned.
     *
     * If icon data is not NULL, the position timestamp of the icon should be retrieved.
     * That is the time when the file (i.e. icon data payload) was last displayed in a
     * fully loaded directory with all icon positions assigned.
     */
    gboolean     (* get_stored_layout_timestamp) (CajaIconContainer *container,
            CajaIconData *data,
            time_t *time);
    /* If icon data is NULL, the layout timestamp of the container should be stored.
     * If icon data is not NULL, the position timestamp of the container should be stored.
     */
    gboolean     (* store_layout_timestamp) (CajaIconContainer *container,
            CajaIconData *data,
            const time_t *time);

    /* Notifications for the whole container. */
    void	     (* band_select_started)	  (CajaIconContainer *container);
    void	     (* band_select_ended)	  (CajaIconContainer *container);
    void         (* selection_changed) 	  (CajaIconContainer *container);
    void         (* layout_changed)           (CajaIconContainer *container);

    /* Notifications for icons. */
    void         (* icon_position_changed)    (CajaIconContainer *container,
            CajaIconData *data,
            const CajaIconPosition *position);
    void         (* icon_text_changed)        (CajaIconContainer *container,
            CajaIconData *data,
            const char *text);
    void         (* renaming_icon)            (CajaIconContainer *container,
            GtkWidget *renaming_widget);
    void	     (* icon_stretch_started)     (CajaIconContainer *container,
            CajaIconData *data);
    void	     (* icon_stretch_ended)       (CajaIconContainer *container,
            CajaIconData *data);
    int	     (* preview)		  (CajaIconContainer *container,
                                   CajaIconData *data,
                                   gboolean start_flag);
    void         (* icon_added)               (CajaIconContainer *container,
            CajaIconData *data);
    void         (* icon_removed)             (CajaIconContainer *container,
            CajaIconData *data);
    void         (* cleared)                  (CajaIconContainer *container);
    gboolean     (* start_interactive_search) (CajaIconContainer *container);
} CajaIconContainerClass;

/* GtkObject */
GType             caja_icon_container_get_type                      (void);
GtkWidget *       caja_icon_container_new                           (void);


/* adding, removing, and managing icons */
void              caja_icon_container_clear                         (CajaIconContainer  *view);
gboolean          caja_icon_container_add                           (CajaIconContainer  *view,
        CajaIconData       *data);
void              caja_icon_container_layout_now                    (CajaIconContainer *container);
gboolean          caja_icon_container_remove                        (CajaIconContainer  *view,
        CajaIconData       *data);
void              caja_icon_container_for_each                      (CajaIconContainer  *view,
        CajaIconCallback    callback,
        gpointer                callback_data);
void              caja_icon_container_request_update                (CajaIconContainer  *view,
        CajaIconData       *data);
void              caja_icon_container_request_update_all            (CajaIconContainer  *container);
void              caja_icon_container_reveal                        (CajaIconContainer  *container,
        CajaIconData       *data);
gboolean          caja_icon_container_is_empty                      (CajaIconContainer  *container);
CajaIconData *caja_icon_container_get_first_visible_icon        (CajaIconContainer  *container);
void              caja_icon_container_scroll_to_icon                (CajaIconContainer  *container,
        CajaIconData       *data);

void              caja_icon_container_begin_loading                 (CajaIconContainer  *container);
void              caja_icon_container_end_loading                   (CajaIconContainer  *container,
        gboolean                all_icons_added);

/* control the layout */
gboolean          caja_icon_container_is_auto_layout                (CajaIconContainer  *container);
void              caja_icon_container_set_auto_layout               (CajaIconContainer  *container,
        gboolean                auto_layout);
gboolean          caja_icon_container_is_tighter_layout             (CajaIconContainer  *container);
void              caja_icon_container_set_tighter_layout            (CajaIconContainer  *container,
        gboolean                tighter_layout);

gboolean          caja_icon_container_is_keep_aligned               (CajaIconContainer  *container);
void              caja_icon_container_set_keep_aligned              (CajaIconContainer  *container,
        gboolean                keep_aligned);
void              caja_icon_container_set_layout_mode               (CajaIconContainer  *container,
        CajaIconLayoutMode  mode);
void              caja_icon_container_set_label_position            (CajaIconContainer  *container,
        CajaIconLabelPosition pos);
void              caja_icon_container_sort                          (CajaIconContainer  *container);
void              caja_icon_container_freeze_icon_positions         (CajaIconContainer  *container);

int               caja_icon_container_get_max_layout_lines           (CajaIconContainer  *container);
int               caja_icon_container_get_max_layout_lines_for_pango (CajaIconContainer  *container);

void              caja_icon_container_set_highlighted_for_clipboard (CajaIconContainer  *container,
        GList                  *clipboard_icon_data);

/* operations on all icons */
void              caja_icon_container_unselect_all                  (CajaIconContainer  *view);
void              caja_icon_container_select_all                    (CajaIconContainer  *view);


/* operations on the selection */
GList     *       caja_icon_container_get_selection                 (CajaIconContainer  *view);
void			  caja_icon_container_invert_selection				(CajaIconContainer  *view);
void              caja_icon_container_set_selection                 (CajaIconContainer  *view,
        GList                  *selection);
GArray    *       caja_icon_container_get_selected_icon_locations   (CajaIconContainer  *view);
gboolean          caja_icon_container_has_stretch_handles           (CajaIconContainer  *container);
gboolean          caja_icon_container_is_stretched                  (CajaIconContainer  *container);
void              caja_icon_container_show_stretch_handles          (CajaIconContainer  *container);
void              caja_icon_container_unstretch                     (CajaIconContainer  *container);
void              caja_icon_container_start_renaming_selected_item  (CajaIconContainer  *container,
        gboolean                select_all);

/* options */
CajaZoomLevel caja_icon_container_get_zoom_level                (CajaIconContainer  *view);
void              caja_icon_container_set_zoom_level                (CajaIconContainer  *view,
        int                     new_zoom_level);
void              caja_icon_container_set_single_click_mode         (CajaIconContainer  *container,
        gboolean                single_click_mode);
void              caja_icon_container_enable_linger_selection       (CajaIconContainer  *view,
        gboolean                enable);
gboolean          caja_icon_container_get_is_fixed_size             (CajaIconContainer  *container);
void              caja_icon_container_set_is_fixed_size             (CajaIconContainer  *container,
        gboolean                is_fixed_size);
gboolean          caja_icon_container_get_is_desktop                (CajaIconContainer  *container);
void              caja_icon_container_set_is_desktop                (CajaIconContainer  *container,
        gboolean                is_desktop);
void              caja_icon_container_reset_scroll_region           (CajaIconContainer  *container);
void              caja_icon_container_set_font                      (CajaIconContainer  *container,
        const char             *font);
void              caja_icon_container_set_font_size_table           (CajaIconContainer  *container,
        const int               font_size_table[CAJA_ZOOM_LEVEL_LARGEST + 1]);
void              caja_icon_container_set_margins                   (CajaIconContainer  *container,
        int                     left_margin,
        int                     right_margin,
        int                     top_margin,
        int                     bottom_margin);
void              caja_icon_container_set_use_drop_shadows          (CajaIconContainer  *container,
        gboolean                use_drop_shadows);
char*             caja_icon_container_get_icon_description          (CajaIconContainer  *container,
        CajaIconData       *data);
gboolean          caja_icon_container_get_allow_moves               (CajaIconContainer  *container);
void              caja_icon_container_set_allow_moves               (CajaIconContainer  *container,
        gboolean                allow_moves);
void		  caja_icon_container_set_forced_icon_size		(CajaIconContainer  *container,
        int                     forced_icon_size);
void		  caja_icon_container_set_all_columns_same_width	(CajaIconContainer  *container,
        gboolean                all_columns_same_width);

gboolean	  caja_icon_container_is_layout_rtl			(CajaIconContainer  *container);
gboolean	  caja_icon_container_is_layout_vertical		(CajaIconContainer  *container);

gboolean          caja_icon_container_get_store_layout_timestamps   (CajaIconContainer  *container);
void              caja_icon_container_set_store_layout_timestamps   (CajaIconContainer  *container,
        gboolean                store_layout);

void              caja_icon_container_widget_to_file_operation_position (CajaIconContainer *container,
        GdkPoint              *position);


#define CANVAS_WIDTH(container,allocation) ((allocation.width	  \
				- container->details->left_margin \
				- container->details->right_margin) \
				/  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) ((allocation.height \
			 - container->details->top_margin \
			 - container->details->bottom_margin) \
			 / EEL_CANVAS (container)->pixels_per_unit)

#endif /* CAJA_ICON_CONTAINER_H */
