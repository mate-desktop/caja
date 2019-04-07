/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-mime-actions.h - uri-specific versions of mime action functions

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

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#ifndef CAJA_MIME_ACTIONS_H
#define CAJA_MIME_ACTIONS_H

#include <gio/gio.h>

#include "caja-file.h"
#include "caja-window-info.h"
#include "caja-window-slot-info.h"

CajaFileAttributes caja_mime_actions_get_required_file_attributes (void);

GAppInfo *             caja_mime_get_default_application_for_file     (CajaFile            *file);
GList *                caja_mime_get_applications_for_file            (CajaFile            *file);

GAppInfo *             caja_mime_get_default_application_for_files    (GList                   *files);
GList *                caja_mime_get_applications_for_files           (GList                   *file);

gboolean               caja_mime_has_any_applications_for_file        (CajaFile            *file);

gboolean               caja_mime_file_opens_in_view                   (CajaFile            *file);
gboolean               caja_mime_file_opens_in_external_app           (CajaFile            *file);
void                   caja_mime_activate_files                       (GtkWindow               *parent_window,
        CajaWindowSlotInfo  *slot_info,
        GList                   *files,
        const char              *launch_directory,
        CajaWindowOpenMode   mode,
        CajaWindowOpenFlags  flags,
        gboolean                 user_confirmation);
void                   caja_mime_activate_file                        (GtkWindow               *parent_window,
        CajaWindowSlotInfo  *slot_info,
        CajaFile            *file,
        const char              *launch_directory,
        CajaWindowOpenMode   mode,
        CajaWindowOpenFlags  flags);


#endif /* CAJA_MIME_ACTIONS_H */
