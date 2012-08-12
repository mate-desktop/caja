/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-link.h: .

   Copyright (C) 2001 Red Hat, Inc.

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

   Authors: Jonathan Blandford <jrb@redhat.com>
*/

#ifndef CAJA_LINK_H
#define CAJA_LINK_H

#include <gdk/gdk.h>

gboolean         caja_link_local_create                      (const char        *directory_uri,
        const char        *base_name,
        const char        *display_name,
        const char        *image,
        const char        *target_uri,
        const GdkPoint    *point,
        int                screen,
        gboolean           unique_filename);
gboolean         caja_link_local_set_text                    (const char        *uri,
        const char        *text);
gboolean         caja_link_local_set_icon                    (const char        *uri,
        const char        *icon);
char *           caja_link_local_get_text                    (const char        *uri);
char *           caja_link_local_get_additional_text         (const char        *uri);
char *           caja_link_local_get_link_uri                (const char        *uri);
void             caja_link_get_link_info_given_file_contents (const char        *file_contents,
        int                link_file_size,
        const char        *file_uri,
        char             **uri,
        char             **name,
        char             **icon,
        gboolean          *is_launcher,
        gboolean          *is_foreign);

#endif /* CAJA_LINK_H */
