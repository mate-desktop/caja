/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-file-utilities.h - interface for file manipulation routines.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef CAJA_FILE_UTILITIES_H
#define CAJA_FILE_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#define CAJA_SAVED_SEARCH_EXTENSION ".savedSearch"
#define CAJA_SAVED_SEARCH_MIMETYPE "application/x-mate-saved-search"

/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   caja_get_xdg_dir                        (const char *type);
char *   caja_get_user_directory                 (void);
char *   caja_get_desktop_directory              (void);
GFile *  caja_get_desktop_location               (void);
char *   caja_get_desktop_directory_uri          (void);
char *   caja_get_home_directory_uri             (void);
gboolean caja_is_desktop_directory_file          (GFile *dir,
        const char *filename);
gboolean caja_is_root_directory                  (GFile *dir);
gboolean caja_is_desktop_directory               (GFile *dir);
gboolean caja_is_home_directory                  (GFile *dir);
gboolean caja_is_home_directory_file             (GFile *dir,
        const char *filename);
GMount * caja_get_mounted_mount_for_root         (GFile *location);
gboolean caja_is_in_system_dir                   (GFile *location);
char *   caja_get_pixmap_directory               (void);

gboolean caja_should_use_templates_directory     (void);
char *   caja_get_templates_directory            (void);
char *   caja_get_templates_directory_uri        (void);
void     caja_create_templates_directory         (void);

char *	 caja_compute_title_for_location	     (GFile *file);

/* A version of mate's mate_pixmap_file that works for the caja prefix.
 * Otherwise similar to mate_pixmap_file in that it checks to see if the file
 * exists and returns NULL if it doesn't.
 */
/* FIXME bugzilla.gnome.org 42425:
 * We might not need this once we get on mate-libs 2.0 which handles
 * mate_pixmap_file better, using MATE_PATH.
 */
char *   caja_pixmap_file                        (const char *partial_path);

/* Locate a file in either the uers directory or the datadir. */
char *   caja_get_data_file_path                 (const char *partial_path);

gboolean caja_is_engrampa_installed              (void);

/* Inhibit/Uninhibit MATE Power Manager */
int    caja_inhibit_power_manager                (const char *message) G_GNUC_WARN_UNUSED_RESULT;
void     caja_uninhibit_power_manager            (int cookie);

/* Return an allocated file name that is guranteed to be unique, but
 * tries to make the name readable to users.
 * This isn't race-free, so don't use for security-related things
 */
char *   caja_ensure_unique_file_name            (const char *directory_uri,
        const char *base_name,
        const char *extension);

GFile *  caja_find_existing_uri_in_hierarchy     (GFile *location);

char * caja_get_accel_map_file (void);

GHashTable * caja_trashed_files_get_original_directories (GList *files,
        GList **unhandled_files);
void caja_restore_files_from_trash (GList *files,
                                    GtkWindow *parent_window);

#endif /* CAJA_FILE_UTILITIES_H */
