/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 *
 */

#ifndef __CAJA_X_CONTENT_BAR_H
#define __CAJA_X_CONTENT_BAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CAJA_TYPE_X_CONTENT_BAR         (caja_x_content_bar_get_type ())
#define CAJA_X_CONTENT_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_X_CONTENT_BAR, CajaXContentBar))
#define CAJA_X_CONTENT_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_X_CONTENT_BAR, CajaXContentBarClass))
#define CAJA_IS_X_CONTENT_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_X_CONTENT_BAR))
#define CAJA_IS_X_CONTENT_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_X_CONTENT_BAR))
#define CAJA_X_CONTENT_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_X_CONTENT_BAR, CajaXContentBarClass))

    typedef struct _CajaXContentBarPrivate CajaXContentBarPrivate;

    typedef struct
    {
        GtkBox	box;

        CajaXContentBarPrivate *priv;
    } CajaXContentBar;

    typedef struct
    {
        GtkBoxClass	    parent_class;
    } CajaXContentBarClass;

    GType		 caja_x_content_bar_get_type	(void) G_GNUC_CONST;

    GtkWidget	*caja_x_content_bar_new		   (GMount              *mount,
            const char          *x_content_type);
    const char      *caja_x_content_bar_get_x_content_type (CajaXContentBar *bar);
    void             caja_x_content_bar_set_x_content_type (CajaXContentBar *bar,
            const char          *x_content_type);
    void             caja_x_content_bar_set_mount          (CajaXContentBar *bar,
            GMount              *mount);
    GMount          *caja_x_content_bar_get_mount          (CajaXContentBar *bar);

G_END_DECLS

#endif /* __CAJA_X_CONTENT_BAR_H */
