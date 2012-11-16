/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-vfs-extensions.h - mate-vfs extensions.  Its likely some of these will
                          be part of mate-vfs in the future.

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

   Authors: Darin Adler <darin@eazel.com>
	    Pavel Cisler <pavel@eazel.com>
	    Mike Fleming  <mfleming@eazel.com>
            John Sullivan <sullivan@eazel.com>
*/

#ifndef EEL_VFS_EXTENSIONS_H
#define EEL_VFS_EXTENSIONS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	EEL_TRASH_URI "trash:"
#define EEL_DESKTOP_URI "x-caja-desktop:"
#define EEL_SEARCH_URI "x-caja-search:"

    gboolean eel_uri_is_trash(const char* uri);
    gboolean eel_uri_is_trash_folder(const char* uri);
    gboolean eel_uri_is_in_trash(const char* uri);
    gboolean eel_uri_is_desktop(const char* uri);
    gboolean eel_uri_is_search(const char* uri);


    char* eel_make_valid_utf8(const char* name);

    char* eel_filename_strip_extension(const char* filename);
    void eel_filename_get_rename_region(const char* filename, int* start_offset, int* end_offset);

#ifdef __cplusplus
}
#endif

#endif /* EEL_VFS_EXTENSIONS_H */
