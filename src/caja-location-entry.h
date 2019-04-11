/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef CAJA_LOCATION_ENTRY_H
#define CAJA_LOCATION_ENTRY_H

#include <libcaja-private/caja-entry.h>

#define CAJA_TYPE_LOCATION_ENTRY caja_location_entry_get_type()
#define CAJA_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAJA_TYPE_LOCATION_ENTRY, CajaLocationEntry))
#define CAJA_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), CAJA_TYPE_LOCATION_ENTRY, CajaLocationEntryClass))
#define CAJA_IS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAJA_TYPE_LOCATION_ENTRY))
#define CAJA_IS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), CAJA_TYPE_LOCATION_ENTRY))
#define CAJA_LOCATION_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), CAJA_TYPE_LOCATION_ENTRY, CajaLocationEntryClass))

typedef struct CajaLocationEntryDetails CajaLocationEntryDetails;

typedef struct CajaLocationEntry
{
    CajaEntry parent;
    CajaLocationEntryDetails *details;
} CajaLocationEntry;

typedef struct
{
    CajaEntryClass parent_class;
} CajaLocationEntryClass;

typedef enum
{
    CAJA_LOCATION_ENTRY_ACTION_GOTO,
    CAJA_LOCATION_ENTRY_ACTION_CLEAR
} CajaLocationEntryAction;

GType      caja_location_entry_get_type     	(void);
GtkWidget* caja_location_entry_new          	(void);
void       caja_location_entry_set_special_text     (CajaLocationEntry *entry,
        const char            *special_text);
void       caja_location_entry_set_secondary_action (CajaLocationEntry *entry,
        CajaLocationEntryAction secondary_action);
void       caja_location_entry_update_current_location (CajaLocationEntry *entry,
        const char *path);

#endif /* CAJA_LOCATION_ENTRY_H */
