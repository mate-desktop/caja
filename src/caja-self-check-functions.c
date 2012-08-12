/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

/* caja-self-check-functions.c: Wrapper for all self check functions
 * in Caja proper.
 */

#include <config.h>

#if ! defined (CAJA_OMIT_SELF_CHECK)

#include "caja-self-check-functions.h"

void caja_run_self_checks(void)
{
    CAJA_FOR_EACH_SELF_CHECK_FUNCTION (CAJA_CALL_SELF_CHECK_FUNCTION)
}

#endif /* ! CAJA_OMIT_SELF_CHECK */
