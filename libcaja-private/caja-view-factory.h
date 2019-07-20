/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-view-factory.h: register and create CajaViews

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

#ifndef CAJA_VIEW_FACTORY_H
#define CAJA_VIEW_FACTORY_H

#include <string.h>

#include <gio/gio.h>

#include "caja-view.h"
#include "caja-window-slot-info.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _CajaViewInfo CajaViewInfo;

    struct _CajaViewInfo
    {
        char *id;
        char *view_combo_label;               /* Foo View (used in preferences dialog and navigation combo) */
        char *view_menu_label_with_mnemonic;  /* View -> _Foo (this is the "_Foo" part) */
        char *error_label;                 /* The foo view encountered an error. */
        char *startup_error_label;         /* The foo view encountered an error while starting up. */
        char *display_location_label;      /* Display this location with the foo view. */
        gboolean single_view;
        CajaView * (*create) (CajaWindowSlotInfo *slot);
        /* MATECOMPONENTTODO: More args here */
        gboolean (*supports_uri) (const char *uri,
                                  GFileType file_type,
                                  const char *mime_type);
    };


    void                    caja_view_factory_register          (CajaViewInfo   *view_info);
    const CajaViewInfo *caja_view_factory_lookup            (const char         *id);
    CajaView *          caja_view_factory_create            (const char         *id,
            CajaWindowSlotInfo *slot);
    gboolean                caja_view_factory_view_supports_uri (const char         *id,
            GFile              *location,
            GFileType          file_type,
            const char         *mime_type);
    GList *                 caja_view_factory_get_views_for_uri (const char         *uri,
            GFileType          file_type,
            const char         *mime_type);




#ifdef __cplusplus
}
#endif

#endif /* CAJA_VIEW_FACTORY_H */
