/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CAJA_LOCATION_EXPANSION_H
#define CAJA_LOCATION_EXPANSION_H

#include <gio/gio.h>

gboolean caja_location_has_variable (const char *text);
char *caja_location_expand_environment (const char *text);
void caja_location_expand_async (const char *text,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data);
char *caja_location_expand_finish (GAsyncResult *result, GError **error);

#endif
