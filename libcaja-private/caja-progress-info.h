/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-progress-info.h: file operation progress info.

   Copyright (C) 2007 Red Hat, Inc.

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

   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef CAJA_PROGRESS_INFO_H
#define CAJA_PROGRESS_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

#define CAJA_TYPE_PROGRESS_INFO         (caja_progress_info_get_type ())
#define CAJA_PROGRESS_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_PROGRESS_INFO, CajaProgressInfo))
#define CAJA_PROGRESS_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CAJA_TYPE_PROGRESS_INFO, CajaProgressInfoClass))
#define CAJA_IS_PROGRESS_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_PROGRESS_INFO))
#define CAJA_IS_PROGRESS_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CAJA_TYPE_PROGRESS_INFO))
#define CAJA_PROGRESS_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CAJA_TYPE_PROGRESS_INFO, CajaProgressInfoClass))

typedef struct _CajaProgressInfo      CajaProgressInfo;
typedef struct _CajaProgressInfoClass CajaProgressInfoClass;

GType caja_progress_info_get_type (void) G_GNUC_CONST;

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done

   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

CajaProgressInfo *caja_progress_info_new (gboolean should_start, gboolean can_pause);
void caja_progress_info_get_ready (CajaProgressInfo *info);
void caja_progress_info_disable_pause (CajaProgressInfo *info);

GList *       caja_get_all_progress_info (void);

char *        caja_progress_info_get_status      (CajaProgressInfo *info);
char *        caja_progress_info_get_details     (CajaProgressInfo *info);
double        caja_progress_info_get_progress    (CajaProgressInfo *info);
GCancellable *caja_progress_info_get_cancellable (CajaProgressInfo *info);
void          caja_progress_info_cancel          (CajaProgressInfo *info);
gboolean      caja_progress_info_get_is_started  (CajaProgressInfo *info);
gboolean      caja_progress_info_get_is_finished (CajaProgressInfo *info);
gboolean      caja_progress_info_get_is_paused   (CajaProgressInfo *info);

void          caja_progress_info_start           (CajaProgressInfo *info);
void          caja_progress_info_finish          (CajaProgressInfo *info);
void          caja_progress_info_pause           (CajaProgressInfo *info);
void          caja_progress_info_resume          (CajaProgressInfo *info);
void          caja_progress_info_set_status      (CajaProgressInfo *info,
        const char           *status);
void          caja_progress_info_take_status     (CajaProgressInfo *info,
        char                 *status);
void          caja_progress_info_set_details     (CajaProgressInfo *info,
        const char           *details);
void          caja_progress_info_take_details    (CajaProgressInfo *info,
        char                 *details);
void          caja_progress_info_set_progress    (CajaProgressInfo *info,
        double                current,
        double                total);
void          caja_progress_info_pulse_progress  (CajaProgressInfo *info);


#endif /* CAJA_PROGRESS_INFO_H */
