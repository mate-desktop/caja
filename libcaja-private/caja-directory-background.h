/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   caja-directory-background.h: Helper for the background of a widget
                                    that is viewing a particular directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>

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

   Authors: Darin Adler <darin@bentspoon.com>
            Jasmine Hassan <jasmine.aura@gmail.com>
*/

#include <eel/eel-background.h>

#include "caja-file.h"
#include "caja-icon-container.h"

void     caja_connect_background_to_file_metadata    (GtkWidget         *widget,
                                                      CajaFile          *file,
                                                      GdkDragAction      default_drag_action);

void     caja_connect_desktop_background_to_settings (CajaIconContainer *icon_container);
