/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 *  This is the header file for the index panel widget, which displays overview information
 *  in a vertical panel and hosts the meta-views.
 */

#ifndef CAJA_INFORMATION_PANEL_H
#define CAJA_INFORMATION_PANEL_H

#include <eel/eel-background-box.h>

#define CAJA_TYPE_INFORMATION_PANEL caja_information_panel_get_type()
#define CAJA_INFORMATION_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_INFORMATION_PANEL, CajaInformationPanel))
#define CAJA_INFORMATION_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_INFORMATION_PANEL, CajaInformationPanelClass))
#define CAJA_IS_INFORMATION_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_INFORMATION_PANEL))
#define CAJA_IS_INFORMATION_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_INFORMATION_PANEL))
#define CAJA_INFORMATION_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_INFORMATION_PANEL, CajaInformationPanelClass))

typedef struct _CajaInformationPanelPrivate CajaInformationPanelPrivate;

#define CAJA_INFORMATION_PANEL_ID "information"

typedef struct
{
    EelBackgroundBox parent_slot;
    CajaInformationPanelPrivate *details;
} CajaInformationPanel;

typedef struct
{
    EelBackgroundBoxClass parent_slot;

    void (*location_changed) (CajaInformationPanel *information_panel,
                              const char *location);
} CajaInformationPanelClass;

GType            caja_information_panel_get_type     (void);
void             caja_information_panel_register     (void);

#endif /* CAJA_INFORMATION_PANEL_H */
