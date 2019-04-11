/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author:  Alexander Larsson <alexl@redhat.com>
 */

#ifndef CAJA_WINDOW_BOOKMARKS_H
#define CAJA_WINDOW_BOOKMARKS_H

#include <libcaja-private/caja-bookmark.h>

#include "caja-bookmark-list.h"
#include "caja-window.h"

void                  caja_bookmarks_exiting                        (void);
void                  caja_window_add_bookmark_for_current_location (CajaWindow *window);
void                  caja_window_edit_bookmarks                    (CajaWindow *window);
void                  caja_window_initialize_bookmarks_menu         (CajaWindow *window);

#endif
