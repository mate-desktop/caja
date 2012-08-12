/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-horizontal-splitter.h - A horizontal splitter with a semi gradient look

   Copyright (C) 1999, 2000 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef CAJA_HORIZONTAL_SPLITTER_H
#define CAJA_HORIZONTAL_SPLITTER_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_HORIZONTAL_SPLITTER caja_horizontal_splitter_get_type()
#define CAJA_HORIZONTAL_SPLITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_HORIZONTAL_SPLITTER, CajaHorizontalSplitter))
#define CAJA_HORIZONTAL_SPLITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_HORIZONTAL_SPLITTER, CajaHorizontalSplitterClass))
#define CAJA_IS_HORIZONTAL_SPLITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_HORIZONTAL_SPLITTER))
#define CAJA_IS_HORIZONTAL_SPLITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_HORIZONTAL_SPLITTER))
#define CAJA_HORIZONTAL_SPLITTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_HORIZONTAL_SPLITTER, CajaHorizontalSplitterClass))

    typedef struct CajaHorizontalSplitterDetails CajaHorizontalSplitterDetails;

    typedef struct
    {
        GtkHPaned				parent_slot;
        CajaHorizontalSplitterDetails	*details;
    } CajaHorizontalSplitter;

    typedef struct
    {
        GtkHPanedClass				parent_slot;
    } CajaHorizontalSplitterClass;

    /* CajaHorizontalSplitter public methods */
    GType      caja_horizontal_splitter_get_type (void);
    GtkWidget *caja_horizontal_splitter_new      (void);

    gboolean   caja_horizontal_splitter_is_hidden	(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_collapse	(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_hide		(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_show		(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_expand		(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_toggle_position	(CajaHorizontalSplitter *splitter);
    void	   caja_horizontal_splitter_pack2           (CajaHorizontalSplitter *splitter,
            GtkWidget                  *child2);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_HORIZONTAL_SPLITTER_H */
