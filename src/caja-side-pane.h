/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*  caja-side-pane.c
 *
 *  Copyright (C) 2002 Ximian, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author: Dave Camp <dave@ximian.com>
 */

#ifndef CAJA_SIDE_PANE_H
#define CAJA_SIDE_PANE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CAJA_TYPE_SIDE_PANE caja_side_pane_get_type()
#define CAJA_SIDE_PANE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_SIDE_PANE, CajaSidePane))
#define CAJA_SIDE_PANE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_SIDE_PANE, CajaSidePaneClass))
#define CAJA_IS_SIDE_PANE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_SIDE_PANE))
#define CAJA_IS_SIDE_PANE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_SIDE_PANE))
#define CAJA_SIDE_PANE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_SIDE_PANE, CajaSidePaneClass))

    typedef struct _CajaSidePanePrivate CajaSidePanePrivate;

    typedef struct
    {
        GtkBox parent;
        CajaSidePanePrivate *details;
    } CajaSidePane;

    typedef struct
    {
        GtkBoxClass parent_slot;

        void (*close_requested) (CajaSidePane *side_pane);
        void (*switch_page) (CajaSidePane *side_pane,
                             GtkWidget *child);
    } CajaSidePaneClass;

    GType                  caja_side_pane_get_type        (void);
    CajaSidePane      *caja_side_pane_new             (void);
    void                   caja_side_pane_add_panel       (CajaSidePane *side_pane,
            GtkWidget        *widget,
            const char       *title,
            const char       *tooltip);
    void                   caja_side_pane_remove_panel    (CajaSidePane *side_pane,
            GtkWidget        *widget);
    void                   caja_side_pane_show_panel      (CajaSidePane *side_pane,
            GtkWidget        *widget);
    void                   caja_side_pane_set_panel_image (CajaSidePane *side_pane,
            GtkWidget        *widget,
            GdkPixbuf        *pixbuf);
    GtkWidget             *caja_side_pane_get_current_panel (CajaSidePane *side_pane);
    GtkWidget             *caja_side_pane_get_title        (CajaSidePane *side_pane);

G_END_DECLS

#endif /* CAJA_SIDE_PANE_H */
