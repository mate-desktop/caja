/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-desktop-link.h: Class that handles the links on the desktop

   Copyright (C) 2003 Red Hat, Inc.

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

#ifndef CAJA_DESKTOP_LINK_H
#define CAJA_DESKTOP_LINK_H

#include <gio/gio.h>

#include "caja-file.h"

#define CAJA_TYPE_DESKTOP_LINK caja_desktop_link_get_type()
#define CAJA_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_DESKTOP_LINK, CajaDesktopLink))
#define CAJA_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_DESKTOP_LINK, CajaDesktopLinkClass))
#define CAJA_IS_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_DESKTOP_LINK))
#define CAJA_IS_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_DESKTOP_LINK))
#define CAJA_DESKTOP_LINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_DESKTOP_LINK, CajaDesktopLinkClass))

typedef struct _CajaDesktopLinkPrivate CajaDesktopLinkPrivate;

typedef struct
{
    GObject parent_slot;
    CajaDesktopLinkPrivate *details;
} CajaDesktopLink;

typedef struct
{
    GObjectClass parent_slot;
} CajaDesktopLinkClass;

typedef enum
{
    CAJA_DESKTOP_LINK_HOME,
    CAJA_DESKTOP_LINK_COMPUTER,
    CAJA_DESKTOP_LINK_TRASH,
    CAJA_DESKTOP_LINK_MOUNT,
    CAJA_DESKTOP_LINK_NETWORK
} CajaDesktopLinkType;

GType   caja_desktop_link_get_type (void);

CajaDesktopLink *   caja_desktop_link_new                     (CajaDesktopLinkType  type);
CajaDesktopLink *   caja_desktop_link_new_from_mount          (GMount                 *mount);
CajaDesktopLinkType caja_desktop_link_get_link_type           (CajaDesktopLink     *link);
char *                  caja_desktop_link_get_file_name           (CajaDesktopLink     *link);
char *                  caja_desktop_link_get_display_name        (CajaDesktopLink     *link);
GIcon *                 caja_desktop_link_get_icon                (CajaDesktopLink     *link);
GFile *                 caja_desktop_link_get_activation_location (CajaDesktopLink     *link);
char *                  caja_desktop_link_get_activation_uri      (CajaDesktopLink     *link);
gboolean                caja_desktop_link_get_date                (CajaDesktopLink     *link,
        CajaDateType         date_type,
        time_t                  *date);
GMount *                caja_desktop_link_get_mount               (CajaDesktopLink     *link);
gboolean                caja_desktop_link_can_rename              (CajaDesktopLink     *link);
gboolean                caja_desktop_link_rename                  (CajaDesktopLink     *link,
        const char              *name);


#endif /* CAJA_DESKTOP_LINK_H */
