/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * caja-application-smclient: a little module for session handling.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Caja is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Caja is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "caja-application-smclient.h"

#include "caja-navigation-window.h"
#include "caja-window-private.h"
#include "caja-window-slot.h"

#include <eel/eel-gtk-extensions.h>
#include <libxml/xmlsave.h>

static char *
caja_application_get_session_data (CajaApplication *self)
{
	xmlDocPtr doc;
	xmlNodePtr root_node, history_node;
	GList *l, *window_list;
	char *data;
	unsigned n_processed;
	xmlSaveCtxtPtr ctx;
	xmlBufferPtr buffer;

	doc = xmlNewDoc ("1.0");

	root_node = xmlNewNode (NULL, "session");
	xmlDocSetRootElement (doc, root_node);

	history_node = xmlNewChild (root_node, NULL, "history", NULL);

	n_processed = 0;
	for (l = caja_get_history_list (); l != NULL; l = l->next) {
		CajaBookmark *bookmark;
		xmlNodePtr bookmark_node;
		GIcon *icon;
		char *tmp;

		bookmark = l->data;

		bookmark_node = xmlNewChild (history_node, NULL, "bookmark", NULL);

		tmp = caja_bookmark_get_name (bookmark);
		xmlNewProp (bookmark_node, "name", tmp);
		g_free (tmp);

		icon = caja_bookmark_get_icon (bookmark);
		tmp = g_icon_to_string (icon);
		g_object_unref (icon);
		if (tmp) {
			xmlNewProp (bookmark_node, "icon", tmp);
			g_free (tmp);
		}

		tmp = caja_bookmark_get_uri (bookmark);
		xmlNewProp (bookmark_node, "uri", tmp);
		g_free (tmp);

		if (caja_bookmark_get_has_custom_name (bookmark)) {
			xmlNewProp (bookmark_node, "has_custom_name", "TRUE");
		}

		if (++n_processed > 50) { /* prevent history list from growing arbitrarily large. */
			break;
		}
	}

	window_list = caja_application_get_window_list (self);

	for (l = window_list; l != NULL; l = l->next) {
		xmlNodePtr win_node, slot_node;
		CajaWindow *window;
		CajaWindowSlot *slot, *active_slot;
		GList *slots, *m;
		char *tmp;

		window = l->data;

		win_node = xmlNewChild (root_node, NULL, "window", NULL);

		xmlNewProp (win_node, "type", CAJA_IS_NAVIGATION_WINDOW (window) ? "navigation" : "spatial");

		if (CAJA_IS_NAVIGATION_WINDOW (window)) { /* spatial windows store their state as file metadata */
			GdkWindow *gdk_window;

			tmp = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
			xmlNewProp (win_node, "geometry", tmp);
			g_free (tmp);

			gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

			if (gdk_window &&
			    gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED) {
				xmlNewProp (win_node, "maximized", "TRUE");
			}

			if (gdk_window &&
			    gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_STICKY) {
				xmlNewProp (win_node, "sticky", "TRUE");
			}

			if (gdk_window &&
			    gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_ABOVE) {
				xmlNewProp (win_node, "keep-above", "TRUE");
			}
		}

		slots = caja_window_get_slots (window);
		active_slot = caja_window_get_active_slot (window);

		/* store one slot as window location. Otherwise
		 * older Caja versions will bail when reading the file. */
		tmp = caja_window_slot_get_location_uri (active_slot);
		xmlNewProp (win_node, "location", tmp);
		g_free (tmp);

		for (m = slots; m != NULL; m = m->next) {
			slot = CAJA_WINDOW_SLOT (m->data);

			slot_node = xmlNewChild (win_node, NULL, "slot", NULL);

			tmp = caja_window_slot_get_location_uri (slot);
			xmlNewProp (slot_node, "location", tmp);
			g_free (tmp);

			if (slot == active_slot) {
				xmlNewProp (slot_node, "active", "TRUE");
			}
		}

		g_list_free (slots);
	}

	buffer = xmlBufferCreate ();
	xmlIndentTreeOutput = 1;
	ctx = xmlSaveToBuffer (buffer, "UTF-8", XML_SAVE_FORMAT);
	if (xmlSaveDoc (ctx, doc) < 0 ||
	    xmlSaveFlush (ctx) < 0) {
		g_message ("failed to save session");
	}
	
	xmlSaveClose(ctx);
	data = g_strndup (buffer->content, buffer->use);
	xmlBufferFree (buffer);

	xmlFreeDoc (doc);

	return data;
}

void
caja_application_smclient_load (CajaApplication *application)
{
	xmlDocPtr doc;
	gboolean bail;
	xmlNodePtr root_node;
	GKeyFile *state_file;
	char *data;

	if (!egg_sm_client_is_resumed (application->smclient)) {
		return;
	}

	state_file = egg_sm_client_get_state_file (application->smclient);
	if (!state_file) {
		return;
	}

	data = g_key_file_get_string (state_file,
				      "Caja",
				      "documents",
				      NULL);
	if (data == NULL) {
		return;
	}
	
	bail = TRUE;

	doc = xmlReadMemory (data, strlen (data), NULL, "UTF-8", 0);
	if (doc != NULL && (root_node = xmlDocGetRootElement (doc)) != NULL) {
		xmlNodePtr node;
		
		bail = FALSE;
		
		for (node = root_node->children; node != NULL; node = node->next) {
			
			if (!strcmp (node->name, "text")) {
				continue;
			} else if (!strcmp (node->name, "history")) {
				xmlNodePtr bookmark_node;
				gboolean emit_change;
				
				emit_change = FALSE;
				
				for (bookmark_node = node->children; bookmark_node != NULL; bookmark_node = bookmark_node->next) {
					if (!strcmp (bookmark_node->name, "text")) {
						continue;
					} else if (!strcmp (bookmark_node->name, "bookmark")) {
						xmlChar *name, *icon_str, *uri;
						gboolean has_custom_name;
						GIcon *icon;
						GFile *location;
						
						uri = xmlGetProp (bookmark_node, "uri");
						name = xmlGetProp (bookmark_node, "name");
						has_custom_name = xmlHasProp (bookmark_node, "has_custom_name") ? TRUE : FALSE;
						icon_str = xmlGetProp (bookmark_node, "icon");
						icon = NULL;
						if (icon_str) {
							icon = g_icon_new_for_string (icon_str, NULL);
						}
						location = g_file_new_for_uri (uri);
						
						emit_change |= caja_add_to_history_list_no_notify (location, name, has_custom_name, icon);
						
						g_object_unref (location);
						
						if (icon) {
							g_object_unref (icon);
						}
						xmlFree (name);
						xmlFree (uri);
						xmlFree (icon_str);
					} else {
						g_message ("unexpected bookmark node %s while parsing session data", bookmark_node->name);
						bail = TRUE;
						continue;
					}
				}
				
				if (emit_change) {
					caja_send_history_list_changed ();
				}
			} else if (!strcmp (node->name, "window")) {
				CajaWindow *window;
				xmlChar *type, *location_uri, *slot_uri;
				xmlNodePtr slot_node;
				GFile *location;
				int i;
				
				type = xmlGetProp (node, "type");
				if (type == NULL) {
					g_message ("empty type node while parsing session data");
					bail = TRUE;
					continue;
				}
				
				location_uri = xmlGetProp (node, "location");
				if (location_uri == NULL) {
					g_message ("empty location node while parsing session data");
					bail = TRUE;
					xmlFree (type);
					continue;
				}
				
				if (!strcmp (type, "navigation")) {
					xmlChar *geometry;
					
					window = caja_application_create_navigation_window (application, NULL, gdk_screen_get_default ());
					
					geometry = xmlGetProp (node, "geometry");
					if (geometry != NULL) {
						eel_gtk_window_set_initial_geometry_from_string 
							(GTK_WINDOW (window), 
							 geometry,
							 CAJA_NAVIGATION_WINDOW_MIN_WIDTH,
							 CAJA_NAVIGATION_WINDOW_MIN_HEIGHT,
							 FALSE);
					}
					xmlFree (geometry);
					
					if (xmlHasProp (node, "maximized")) {
						gtk_window_maximize (GTK_WINDOW (window));
					} else {
						gtk_window_unmaximize (GTK_WINDOW (window));
					}
					
					if (xmlHasProp (node, "sticky")) {
						gtk_window_stick (GTK_WINDOW (window));
					} else {
						gtk_window_unstick (GTK_WINDOW (window));
					}
					
					if (xmlHasProp (node, "keep-above")) {
						gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
					} else {
						gtk_window_set_keep_above (GTK_WINDOW (window), FALSE);
					}
					
					for (i = 0, slot_node = node->children; slot_node != NULL; slot_node = slot_node->next) {
						if (!strcmp (slot_node->name, "slot")) {
							slot_uri = xmlGetProp (slot_node, "location");
							if (slot_uri != NULL) {
								CajaWindowSlot *slot;
								
								if (i == 0) {
									slot = window->details->active_pane->active_slot;
								} else {
									slot = caja_window_open_slot (window->details->active_pane, CAJA_WINDOW_OPEN_SLOT_APPEND);
								}
								
								location = g_file_new_for_uri (slot_uri);
								caja_window_slot_open_location (slot, location, FALSE);
								
								if (xmlHasProp (slot_node, "active")) {
									caja_window_set_active_slot (slot->pane->window, slot);
								}
								
								i++;
							}
							xmlFree (slot_uri);
						}
					}
					
					if (i == 0) {
						/* This may be an old session file */
						location = g_file_new_for_uri (location_uri);
						caja_window_slot_open_location (window->details->active_pane->active_slot, location, FALSE);
						g_object_unref (location);
					}
				} else if (!strcmp (type, "spatial")) {
					location = g_file_new_for_uri (location_uri);
					window = caja_application_get_spatial_window (application, NULL, NULL,
											  location, gdk_screen_get_default (),
											  NULL);

					caja_window_go_to (window, location);

					g_object_unref (location);
				} else {
					g_message ("unknown window type \"%s\" while parsing session data", type);
					bail = TRUE;
				}
				
				xmlFree (type);
				xmlFree (location_uri);
			} else {
				g_message ("unexpected node %s while parsing session data", node->name);
				bail = TRUE;
				continue;
			}
		}
	}
	
	if (doc != NULL) {
		xmlFreeDoc (doc);
	}

	g_free (data);

	if (bail) {
		g_message ("failed to load session");
	} 
}

static void
smclient_save_state_cb (EggSMClient *client,
			GKeyFile *state_file,
			CajaApplication *application)
{
	char *data;

	data = caja_application_get_session_data (application);

	if (data != NULL) {
		g_key_file_set_string (state_file,
				       "Caja",
				       "documents", 
				       data);
	}

	g_free (data);
}

static void
smclient_quit_cb (EggSMClient   *client,
		  CajaApplication *application)
{
	g_application_release (G_APPLICATION (application));
}

void
caja_application_smclient_init (CajaApplication *self)
{
	g_assert (self->smclient == NULL);

        self->smclient = egg_sm_client_get ();
        g_signal_connect (self->smclient, "save_state",
                          G_CALLBACK (smclient_save_state_cb),
                          self);
	g_signal_connect (self->smclient, "quit",
			  G_CALLBACK (smclient_quit_cb),
			  self);
	/* TODO: Should connect to quit_requested and block logout on active transfer? */
}
