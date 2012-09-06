/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* caja-bookmarks-window.h - interface for bookmark-editing window.
 */

#ifndef CAJA_BOOKMARKS_WINDOW_H
#define CAJA_BOOKMARKS_WINDOW_H

#include <gtk/gtk.h>
#include "caja-bookmark-list.h"
#include "caja-window.h"

GtkWindow *create_bookmarks_window                 (CajaBookmarkList *bookmarks,
                                                    CajaWindow       *window_source);
void       caja_bookmarks_window_save_geometry     (GtkWindow        *window);
void	   edit_bookmarks_dialog_set_signals	   (CajaWindow       *window);

#endif /* CAJA_BOOKMARKS_WINDOW_H */
