/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-dialog-notebook.h - GTKNotebook management for dialog windows

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

   Authors: Laurent Napias <tamplan@free.fr>
*/

#ifndef CAJA_DIALOG_NOTEBOOK_H
#define CAJA_DIALOG_NOTEBOOK_H

#include <gtk/gtk.h>

gboolean
dialog_page_scroll_event_cb (GtkWidget *widget, GdkEventScroll *event, GtkWindow *window);

#endif /* CAJA_DIALOG_NOTEBOOK_H */
