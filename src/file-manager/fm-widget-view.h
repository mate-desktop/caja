/* vi: set sw=4 ts=4 wrap ai: */
/*
 * fm-widget-view.h: This file is part of caja.
 *
 * Copyright (C) 2019 Wu Xiaotian <yetist@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * */

#ifndef __FM_WIDGET_VIEW_H__
#define __FM_WIDGET_VIEW_H__  1

#include "fm-directory-view.h"

G_BEGIN_DECLS

#define FM_WIDGET_VIEW_ID                "OAFIID:Caja_File_Manager_Widget_View"
#define FM_TYPE_WIDGET_VIEW              (fm_widget_view_get_type ())
G_DECLARE_FINAL_TYPE (FMWidgetView, fm_widget_view, FM, WIDGET_VIEW, FMDirectoryView)

FMWidgetView* fm_widget_view_new      (void);
void          fm_widget_view_register (void);

G_END_DECLS

#endif /* __FM_WIDGET_VIEW_H__ */
