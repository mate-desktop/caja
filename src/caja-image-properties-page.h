/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Copyright (C) 2004 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef CAJA_IMAGE_PROPERTIES_PAGE_H
#define CAJA_IMAGE_PROPERTIES_PAGE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CAJA_TYPE_IMAGE_PROPERTIES_PAGE caja_image_properties_page_get_type()
#define CAJA_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_IMAGE_PROPERTIES_PAGE, CajaImagePropertiesPage))
#define CAJA_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_IMAGE_PROPERTIES_PAGE, CajaImagePropertiesPageClass))
#define CAJA_IS_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_IMAGE_PROPERTIES_PAGE))
#define CAJA_IS_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_IMAGE_PROPERTIES_PAGE))
#define CAJA_IMAGE_PROPERTIES_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_IMAGE_PROPERTIES_PAGE, CajaImagePropertiesPageClass))

typedef struct _CajaImagePropertiesPagePrivate CajaImagePropertiesPagePrivate;

typedef struct
{
    GtkBox parent;
    CajaImagePropertiesPagePrivate *details;
} CajaImagePropertiesPage;

typedef struct
{
    GtkBoxClass parent;
} CajaImagePropertiesPageClass;

GType caja_image_properties_page_get_type (void);
void  caja_image_properties_page_register (void);

G_END_DECLS

#endif /* CAJA_IMAGE_PROPERTIES_PAGE_H */
