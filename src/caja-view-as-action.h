/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Caja
 *
 *  Copyright (C) 2009 Red Hat, Inc.
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
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef CAJA_VIEW_AS_ACTION_H
#define CAJA_VIEW_AS_ACTION_H

#include <gtk/gtk.h>

#define CAJA_TYPE_VIEW_AS_ACTION            (caja_view_as_action_get_type ())
#define CAJA_VIEW_AS_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_VIEW_AS_ACTION, CajaViewAsAction))
#define CAJA_VIEW_AS_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_VIEW_AS_ACTION, CajaViewAsActionClass))
#define CAJA_IS_VIEW_AS_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_VIEW_AS_ACTION))
#define CAJA_IS_VIEW_AS_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), CAJA_TYPE_VIEW_AS_ACTION))
#define CAJA_VIEW_AS_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CAJA_TYPE_VIEW_AS_ACTION, CajaViewAsActionClass))

typedef struct _CajaViewAsAction       CajaViewAsAction;
typedef struct _CajaViewAsActionClass  CajaViewAsActionClass;
typedef struct _CajaViewAsActionPrivate CajaViewAsActionPrivate;

struct _CajaViewAsAction
{
    GtkAction parent;

    /*< private >*/
    CajaViewAsActionPrivate *priv;
};

struct _CajaViewAsActionClass
{
    GtkActionClass parent_class;
};

GType    caja_view_as_action_get_type   (void);

#endif
