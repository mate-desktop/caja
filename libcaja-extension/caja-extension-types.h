/*
 *  caja-info-provider.h - Type definitions for Caja extensions
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author: Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Caja extensions that want to
 * provide information about files.  Extensions are called when Caja
 * needs information about a file.  They are passed a CajaFileInfo
 * object which should be filled with relevant information */

#ifndef CAJA_EXTENSION_TYPES_H
#define CAJA_EXTENSION_TYPES_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAJA_TYPE_OPERATION_RESULT (caja_operation_result_get_type ())

    /* Handle for asynchronous interfaces.  These are opaque handles that must
     * be unique within an extension object.  These are returned by operations
     * that return CAJA_OPERATION_IN_PROGRESS */
    typedef struct _CajaOperationHandle CajaOperationHandle;

    typedef enum
    {
        /* Returned if the call succeeded, and the extension is done
         * with the request */
        CAJA_OPERATION_COMPLETE,

        /* Returned if the call failed */
        CAJA_OPERATION_FAILED,

        /* Returned if the extension has begun an async operation.
         * If this is returned, the extension must set the handle
         * parameter and call the callback closure when the
         * operation is complete. */
        CAJA_OPERATION_IN_PROGRESS
    } CajaOperationResult;

    GType caja_operation_result_get_type (void);

    void caja_module_initialize (GTypeModule  *module);
    void caja_module_shutdown   (void);
    void caja_module_list_types (const GType **types,
                                 int          *num_types);
    void caja_module_list_pyfiles (GList     **pyfiles);

#ifdef __cplusplus
}
#endif

#endif
