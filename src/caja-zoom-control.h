/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Caja is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the zoom control on the location bar
 *
 */

#ifndef CAJA_ZOOM_CONTROL_H
#define CAJA_ZOOM_CONTROL_H

#include <gtk/gtk.h>

#include <libcaja-private/caja-icon-info.h> /* For CajaZoomLevel */

#define CAJA_TYPE_ZOOM_CONTROL caja_zoom_control_get_type()
#define CAJA_ZOOM_CONTROL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_ZOOM_CONTROL, CajaZoomControl))
#define CAJA_ZOOM_CONTROL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_ZOOM_CONTROL, CajaZoomControlClass))
#define CAJA_IS_ZOOM_CONTROL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_ZOOM_CONTROL))
#define CAJA_IS_ZOOM_CONTROL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_ZOOM_CONTROL))
#define CAJA_ZOOM_CONTROL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_ZOOM_CONTROL, CajaZoomControlClass))

typedef struct CajaZoomControl CajaZoomControl;
typedef struct CajaZoomControlClass CajaZoomControlClass;
typedef struct _CajaZoomControlPrivate CajaZoomControlPrivate;

struct CajaZoomControl
{
    GtkBox parent;
    CajaZoomControlPrivate *details;
};

struct CajaZoomControlClass
{
    GtkBoxClass parent_class;

    void (*zoom_in)		(CajaZoomControl *control);
    void (*zoom_out) 	(CajaZoomControl *control);
    void (*zoom_to_level) 	(CajaZoomControl *control,
                             CajaZoomLevel zoom_level);
    void (*zoom_to_default)	(CajaZoomControl *control);

    /* Action signal for keybindings, do not connect to this */
    void (*change_value)    (CajaZoomControl *control,
                             GtkScrollType scroll);
};

GType             caja_zoom_control_get_type           (void);
GtkWidget *       caja_zoom_control_new                (void);
void              caja_zoom_control_set_zoom_level     (CajaZoomControl *zoom_control,
        CajaZoomLevel    zoom_level);
void              caja_zoom_control_set_parameters     (CajaZoomControl *zoom_control,
        CajaZoomLevel    min_zoom_level,
        CajaZoomLevel    max_zoom_level,
        gboolean             has_min_zoom_level,
        gboolean             has_max_zoom_level,
        GList               *zoom_levels);
CajaZoomLevel caja_zoom_control_get_zoom_level     (CajaZoomControl *zoom_control);
CajaZoomLevel caja_zoom_control_get_min_zoom_level (CajaZoomControl *zoom_control);
CajaZoomLevel caja_zoom_control_get_max_zoom_level (CajaZoomControl *zoom_control);
gboolean          caja_zoom_control_has_min_zoom_level (CajaZoomControl *zoom_control);
gboolean          caja_zoom_control_has_max_zoom_level (CajaZoomControl *zoom_control);
gboolean          caja_zoom_control_can_zoom_in        (CajaZoomControl *zoom_control);
gboolean          caja_zoom_control_can_zoom_out       (CajaZoomControl *zoom_control);

void              caja_zoom_control_set_active_appearance (CajaZoomControl *zoom_control, gboolean is_active);

#endif /* CAJA_ZOOM_CONTROL_H */
