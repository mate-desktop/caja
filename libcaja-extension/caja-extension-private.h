/*
 *  caja-extension-private.h - Type definitions for Caja extensions
 *
 *  Copyright (C) 2009 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef CAJA_EXTENSION_PRIVATE_H
#define CAJA_EXTENSION_PRIVATE_H

#include "caja-file-info.h"

#ifdef __cplusplus
extern "C" {
#endif

    extern CajaFileInfo *(*caja_file_info_getter) (GFile *location, gboolean create);

#ifdef __cplusplus
}
#endif

#endif
