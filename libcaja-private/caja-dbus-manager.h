/*
 * caja-dbus-manager: caja DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __CAJA_DBUS_MANAGER_H__
#define __CAJA_DBUS_MANAGER_H__

#include <glib-object.h>

void caja_dbus_manager_start (void);
void caja_dbus_manager_stop (void);

#endif /* __CAJA_DBUS_MANAGER_H__ */
