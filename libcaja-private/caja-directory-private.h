/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   caja-directory-private.h: Caja directory model.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Author: Darin Adler <darin@bentspoon.com>
*/

#include <gio/gio.h>
#include <libxml/tree.h>

#include <eel/eel-vfs-extensions.h>

#include <libcaja-extension/caja-info-provider.h>

#include "caja-directory.h"
#include "caja-file-queue.h"
#include "caja-file.h"
#include "caja-monitor.h"

typedef struct LinkInfoReadState LinkInfoReadState;
typedef struct TopLeftTextReadState TopLeftTextReadState;
typedef struct FileMonitors FileMonitors;
typedef struct DirectoryLoadState DirectoryLoadState;
typedef struct DirectoryCountState DirectoryCountState;
typedef struct DeepCountState DeepCountState;
typedef struct GetInfoState GetInfoState;
typedef struct NewFilesState NewFilesState;
typedef struct MimeListState MimeListState;
typedef struct ThumbnailState ThumbnailState;
typedef struct MountState MountState;
typedef struct FilesystemInfoState FilesystemInfoState;

typedef enum
{
    REQUEST_LINK_INFO,
    REQUEST_DEEP_COUNT,
    REQUEST_DIRECTORY_COUNT,
    REQUEST_FILE_INFO,
    REQUEST_FILE_LIST, /* always FALSE if file != NULL */
    REQUEST_MIME_LIST,
    REQUEST_TOP_LEFT_TEXT,
    REQUEST_LARGE_TOP_LEFT_TEXT,
    REQUEST_EXTENSION_INFO,
    REQUEST_THUMBNAIL,
    REQUEST_MOUNT,
    REQUEST_FILESYSTEM_INFO,
    REQUEST_TYPE_LAST
} RequestType;

/* A request for information about one or more files. */
typedef guint32 Request;
typedef gint32 RequestCounter[REQUEST_TYPE_LAST];

#define REQUEST_WANTS_TYPE(request, type) ((request) & (1<<(type)))
#define REQUEST_SET_TYPE(request, type) (request) |= (1<<(type))

struct _CajaDirectoryPrivate
{
    /* The location. */
    GFile *location;

    /* The file objects. */
    CajaFile *as_file;
    GList *file_list;
    GHashTable *file_hash;

    /* Queues of files needing some I/O done. */
    CajaFileQueue *high_priority_queue;
    CajaFileQueue *low_priority_queue;
    CajaFileQueue *extension_queue;

    /* These lists are going to be pretty short.  If we think they
     * are going to get big, we can use hash tables instead.
     */
    GList *call_when_ready_list;
    RequestCounter call_when_ready_counters;
    GList *monitor_list;
    RequestCounter monitor_counters;
    guint call_ready_idle_id;

    CajaMonitor *monitor;
    gulong 		 mime_db_monitor;

    gboolean in_async_service_loop;
    gboolean state_changed;

    gboolean file_list_monitored;
    gboolean directory_loaded;
    gboolean directory_loaded_sent_notification;
    DirectoryLoadState *directory_load_in_progress;

    GList *pending_file_info; /* list of MateVFSFileInfo's that are pending */
    int confirmed_file_count;
    guint dequeue_pending_idle_id;

    GList *new_files_in_progress; /* list of NewFilesState * */

    DirectoryCountState *count_in_progress;

    CajaFile *deep_count_file;
    DeepCountState *deep_count_in_progress;

    MimeListState *mime_list_in_progress;

    CajaFile *get_info_file;
    GetInfoState *get_info_in_progress;

    CajaFile *extension_info_file;
    CajaInfoProvider *extension_info_provider;
    CajaOperationHandle *extension_info_in_progress;
    guint extension_info_idle;

    ThumbnailState *thumbnail_state;

    MountState *mount_state;

    FilesystemInfoState *filesystem_info_state;

    TopLeftTextReadState *top_left_read_state;

    LinkInfoReadState *link_info_read_state;

    GList *file_operations_in_progress; /* list of FileOperation * */

    guint64 free_space; /* (guint)-1 for unknown */
    time_t free_space_read; /* The time free_space was updated, or 0 for never */
};

CajaDirectory *caja_directory_get_existing                    (GFile                     *location);

/* async. interface */
void               caja_directory_async_state_changed             (CajaDirectory         *directory);
void               caja_directory_call_when_ready_internal        (CajaDirectory         *directory,
        CajaFile              *file,
        CajaFileAttributes     file_attributes,
        gboolean                   wait_for_file_list,
        CajaDirectoryCallback  directory_callback,
        CajaFileCallback       file_callback,
        gpointer                   callback_data);
gboolean           caja_directory_check_if_ready_internal         (CajaDirectory         *directory,
        CajaFile              *file,
        CajaFileAttributes     file_attributes);
void               caja_directory_cancel_callback_internal        (CajaDirectory         *directory,
        CajaFile              *file,
        CajaDirectoryCallback  directory_callback,
        CajaFileCallback       file_callback,
        gpointer                   callback_data);
void               caja_directory_monitor_add_internal            (CajaDirectory         *directory,
        CajaFile              *file,
        gconstpointer              client,
        gboolean                   monitor_hidden_files,
        CajaFileAttributes     attributes,
        CajaDirectoryCallback  callback,
        gpointer                   callback_data);
void               caja_directory_monitor_remove_internal         (CajaDirectory         *directory,
        CajaFile              *file,
        gconstpointer              client);
void               caja_directory_get_info_for_new_files          (CajaDirectory         *directory,
        GList                     *vfs_uris);
CajaFile *     caja_directory_get_existing_corresponding_file (CajaDirectory         *directory);
void               caja_directory_invalidate_count_and_mime_list  (CajaDirectory         *directory);
gboolean           caja_directory_is_file_list_monitored          (CajaDirectory         *directory);
gboolean           caja_directory_is_anyone_monitoring_file_list  (CajaDirectory         *directory);
gboolean           caja_directory_has_active_request_for_file     (CajaDirectory         *directory,
        CajaFile              *file);
void               caja_directory_remove_file_monitor_link        (CajaDirectory         *directory,
        GList                     *link);
void               caja_directory_schedule_dequeue_pending        (CajaDirectory         *directory);
void               caja_directory_stop_monitoring_file_list       (CajaDirectory         *directory);
void               caja_directory_cancel                          (CajaDirectory         *directory);
void               caja_async_destroying_file                     (CajaFile              *file);
void               caja_directory_force_reload_internal           (CajaDirectory         *directory,
        CajaFileAttributes     file_attributes);
void               caja_directory_cancel_loading_file_attributes  (CajaDirectory         *directory,
        CajaFile              *file,
        CajaFileAttributes     file_attributes);

/* Calls shared between directory, file, and async. code. */
void               caja_directory_emit_files_added                (CajaDirectory         *directory,
        GList                     *added_files);
void               caja_directory_emit_files_changed              (CajaDirectory         *directory,
        GList                     *changed_files);
void               caja_directory_emit_change_signals             (CajaDirectory         *directory,
        GList                     *changed_files);
void               emit_change_signals_for_all_files		      (CajaDirectory	 *directory);
void               emit_change_signals_for_all_files_in_all_directories (void);
void               caja_directory_emit_done_loading               (CajaDirectory         *directory);
void               caja_directory_emit_load_error                 (CajaDirectory         *directory,
        GError                    *error);
CajaDirectory *caja_directory_get_internal                    (GFile                     *location,
        gboolean                   create);
char *             caja_directory_get_name_for_self_as_new_file   (CajaDirectory         *directory);
Request            caja_directory_set_up_request                  (CajaFileAttributes     file_attributes);

/* Interface to the file list. */
CajaFile *     caja_directory_find_file_by_name               (CajaDirectory         *directory,
        const char                *filename);

void               caja_directory_add_file                        (CajaDirectory         *directory,
        CajaFile              *file);
void               caja_directory_remove_file                     (CajaDirectory         *directory,
        CajaFile              *file);
FileMonitors *     caja_directory_remove_file_monitors            (CajaDirectory         *directory,
        CajaFile              *file);
void               caja_directory_add_file_monitors               (CajaDirectory         *directory,
        CajaFile              *file,
        FileMonitors              *monitors);
GList *            caja_directory_begin_file_name_change          (CajaDirectory         *directory,
        CajaFile              *file);
void               caja_directory_end_file_name_change            (CajaDirectory         *directory,
        CajaFile              *file,
        GList                     *node);
void               caja_directory_moved                           (const char                *from_uri,
        const char                *to_uri);
/* Interface to the work queue. */

void               caja_directory_add_file_to_work_queue          (CajaDirectory *directory,
        CajaFile *file);
void               caja_directory_remove_file_from_work_queue     (CajaDirectory *directory,
        CajaFile *file);

/* debugging functions */
int                caja_directory_number_outstanding              (void);
