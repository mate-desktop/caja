/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 *  libcaja: A library for caja view implementations.
 *
 *  Copyright (C) 2001 Eazel, Inc.
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
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef CAJA_IDLE_QUEUE_H
#define CAJA_IDLE_QUEUE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct CajaIdleQueue CajaIdleQueue;

    CajaIdleQueue *caja_idle_queue_new     (void);
    void               caja_idle_queue_add     (CajaIdleQueue *queue,
            GFunc              callback,
            gpointer           data,
            gpointer           callback_data,
            GFreeFunc          free_callback_data);
    void               caja_idle_queue_destroy (CajaIdleQueue *queue);

#ifdef __cplusplus
}
#endif

#endif /* CAJA_IDLE_QUEUE_H */
