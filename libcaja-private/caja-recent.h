#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "caja-file.h"

void caja_recent_add_file (CajaFile *file,
                           GAppInfo *application);
