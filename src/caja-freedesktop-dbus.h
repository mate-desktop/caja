/*
 * caja-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 *          Stefano Karapetsas <stefano@karapetsas.com>
 */


#ifndef __CAJA_FREEDESKTOP_DBUS_H__
#define __CAJA_FREEDESKTOP_DBUS_H__

#include <glib-object.h>

#include "caja-application.h"

#define CAJA_FDO_DBUS_IFACE "org.freedesktop.FileManager1"
#define CAJA_FDO_DBUS_NAME  "org.freedesktop.FileManager1"
#define CAJA_FDO_DBUS_PATH  "/org/freedesktop/FileManager1"

typedef struct _CajaFreedesktopDBus CajaFreedesktopDBus;
typedef struct _CajaFreedesktopDBusClass CajaFreedesktopDBusClass;

GType caja_freedesktop_dbus_get_type (void);
CajaFreedesktopDBus * caja_freedesktop_dbus_new (CajaApplication *application);

#endif /* __CAJA_FREEDESKTOP_DBUS_H__ */
