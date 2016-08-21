/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-mate-extensions.h - interface for new functions that operate on
                                 mate classes. Perhaps some of these should be
  			         rolled into mate someday.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef EEL_MATE_EXTENSIONS_H
#define EEL_MATE_EXTENSIONS_H

#include <gtk/gtk.h>

/* Return a command string containing the path to a terminal on this system. */
char *        eel_mate_make_terminal_command                         (const char               *command);

/* Open up a new terminal, optionally passing in a command to execute */
void          eel_mate_open_terminal_on_screen                       (const char               *command,
        GdkScreen                *screen);

#endif /* EEL_MATE_EXTENSIONS_H */
