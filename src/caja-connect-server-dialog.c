/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Caja
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
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
 */

#include <config.h>

#include "caja-connect-server-dialog.h"

#include <string.h>
#include <eel/eel-stock-dialogs.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "caja-application.h"
#include "caja-bookmark-list.h"
#include "caja-connect-server-operation.h"
#include "caja-window.h"

#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-icon-names.h>

/* TODO:
 * - name entry + pre-fill
 * - NetworkManager integration
 */

struct _CajaConnectServerDialogDetails
{
    CajaApplication *application;

	GtkWidget *primary_table;
    GtkWidget *user_details;
    GtkWidget *port_spinbutton;

    GtkWidget *info_bar;
    GtkWidget *info_bar_content;

    GtkWidget *type_combo;
    GtkWidget *server_entry;
    GtkWidget *share_entry;
    GtkWidget *folder_entry;
    GtkWidget *domain_entry;
    GtkWidget *user_entry;
    GtkWidget *password_entry;
    GtkWidget *remember_checkbox;
    GtkWidget *connect_button;

    GList *iconized_entries;

    GSimpleAsyncResult *fill_details_res;
    GAskPasswordFlags fill_details_flags;
    GMountOperation *fill_operation;

    gboolean last_password_set;
    gulong password_sensitive_id;
    gboolean should_destroy;
};

G_DEFINE_TYPE (CajaConnectServerDialog, caja_connect_server_dialog,
	       GTK_TYPE_DIALOG)

static void sensitive_entry_changed_callback (GtkEditable *editable,
					      GtkWidget *widget);
static void iconized_entry_changed_cb (GtkEditable *entry,
				       CajaConnectServerDialog *dialog);

enum
{
    RESPONSE_CONNECT
};

struct MethodInfo
{
    const char *scheme;
    guint flags;
    guint default_port;
};

/* A collection of flags for MethodInfo.flags */
enum
{
    DEFAULT_METHOD = (1 << 0),

	/* Widgets to display in connect_dialog_setup_for_type */
    SHOW_SHARE     = (1 << 1),
    SHOW_PORT      = (1 << 2),
    SHOW_USER      = (1 << 3),
    SHOW_DOMAIN    = (1 << 4),

    IS_ANONYMOUS   = (1 << 5)
};

/* Remember to fill in descriptions below */
static struct MethodInfo methods[] =
{
    /* FIXME: we need to alias ssh to sftp */
    { "sftp",  SHOW_PORT | SHOW_USER, 22 },
    { "ftp",  SHOW_PORT | SHOW_USER, 21 },
    { "ftp",  DEFAULT_METHOD | IS_ANONYMOUS | SHOW_PORT, 21 },
    { "smb",  SHOW_SHARE | SHOW_USER | SHOW_DOMAIN, 0 },
    { "dav",  SHOW_PORT | SHOW_USER, 80 },
    /* FIXME: hrm, shouldn't it work? */
    { "davs", SHOW_PORT | SHOW_USER, 443 },
};

/* To get around non constant gettext strings */
static const char*
get_method_description (struct MethodInfo *meth)
{
    if (strcmp (meth->scheme, "sftp") == 0) {
        return _("SSH");
    } else if (strcmp (meth->scheme, "ftp") == 0) {
        if (meth->flags & IS_ANONYMOUS) {
            return _("Public FTP");
        } else {
            return _("FTP (with login)");
        }
    } else if (strcmp (meth->scheme, "smb") == 0) {
        return _("Windows share");
    } else if (strcmp (meth->scheme, "dav") == 0) {
        return _("WebDAV (HTTP)");
    } else if (strcmp (meth->scheme, "davs") == 0) {
        return _("Secure WebDAV (HTTPS)");

        /* No descriptive text */
    } else {
        return meth->scheme;
    }
}

static void
connect_dialog_restore_info_bar (CajaConnectServerDialog *dialog,
				 GtkMessageType message_type)
{
	if (dialog->details->info_bar_content != NULL) {
		gtk_widget_destroy (dialog->details->info_bar_content);
		dialog->details->info_bar_content = NULL;
	}

	gtk_info_bar_set_message_type (GTK_INFO_BAR (dialog->details->info_bar),
				       message_type);
}

static void
connect_dialog_set_connecting (CajaConnectServerDialog *dialog)
{
	GtkWidget *hbox;
	GtkWidget *widget;
	GtkWidget *content_area;
	gint width, height;

	connect_dialog_restore_info_bar (dialog, GTK_MESSAGE_INFO);
	gtk_widget_show (dialog->details->info_bar);	

	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (dialog->details->info_bar));

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (content_area), hbox);
	gtk_widget_show (hbox);

	dialog->details->info_bar_content = hbox;

	widget = gtk_spinner_new ();
	gtk_icon_size_lookup (GTK_ICON_SIZE_SMALL_TOOLBAR, &width, &height);
	gtk_widget_set_size_request (widget, width, height);
	gtk_spinner_start (GTK_SPINNER (widget));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
	gtk_widget_show (widget);

	widget = gtk_label_new (_("Connecting..."));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
	gtk_widget_show (widget);

	gtk_widget_set_sensitive (dialog->details->connect_button, FALSE);
}

static void
connect_dialog_gvfs_error (CajaConnectServerDialog *dialog)
{
	GtkWidget *hbox, *image, *content_area, *label;

	connect_dialog_restore_info_bar (dialog, GTK_MESSAGE_ERROR);

	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (dialog->details->info_bar));

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (content_area), hbox);
	gtk_widget_show (hbox);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 6);
	gtk_widget_show (image);
	
	label = gtk_label_new (_("Can't load the supported server method list.\n"
				 "Please check your GVfs installation."));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);
	gtk_widget_show (label);

	gtk_widget_set_sensitive (dialog->details->connect_button, FALSE);
	gtk_widget_set_sensitive (dialog->details->primary_table, FALSE);

	gtk_widget_show (dialog->details->info_bar);
}

static void
iconized_entry_restore (gpointer data,
			gpointer user_data)
{
	GtkEntry *entry;
	CajaConnectServerDialog *dialog;

	entry = data;
	dialog = user_data;

	gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
				       GTK_ENTRY_ICON_SECONDARY,
				       NULL);

	g_signal_handlers_disconnect_by_func (entry,
					      iconized_entry_changed_cb,
					      dialog);	
}

static void
iconized_entry_changed_cb (GtkEditable *entry,
			   CajaConnectServerDialog *dialog)
{
	dialog->details->iconized_entries =
		g_list_remove (dialog->details->iconized_entries, entry);

	iconized_entry_restore (entry, dialog);
}

static void
iconize_entry (CajaConnectServerDialog *dialog,
	       GtkWidget *entry)
{
	if (!g_list_find (dialog->details->iconized_entries, entry)) {
		dialog->details->iconized_entries =
			g_list_prepend (dialog->details->iconized_entries, entry);

		gtk_entry_set_icon_from_stock (GTK_ENTRY (entry),
					       GTK_ENTRY_ICON_SECONDARY,
					       GTK_STOCK_DIALOG_WARNING);

		gtk_widget_grab_focus (entry);

		g_signal_connect (entry, "changed",
				  G_CALLBACK (iconized_entry_changed_cb), dialog);
	}
}

static void
connect_dialog_set_info_bar_error (CajaConnectServerDialog *dialog,
				   GError *error)
{
	GtkWidget *content_area, *label, *entry, *hbox, *icon;
	gchar *str;
	const gchar *folder, *server;

	connect_dialog_restore_info_bar (dialog, GTK_MESSAGE_WARNING);

	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (dialog->details->info_bar));
	entry = NULL;

	switch (error->code) {
	case G_IO_ERROR_FAILED_HANDLED:
		return;
	case G_IO_ERROR_NOT_FOUND:
		folder = gtk_entry_get_text (GTK_ENTRY (dialog->details->folder_entry));
		server = gtk_entry_get_text (GTK_ENTRY (dialog->details->server_entry));
		str = g_strdup_printf (_("The folder \"%s\" cannot be opened on \"%s\"."),
				       folder, server);
		label = gtk_label_new (str);
		entry = dialog->details->folder_entry;

		g_free (str);

		break;
	case G_IO_ERROR_HOST_NOT_FOUND:
		server = gtk_entry_get_text (GTK_ENTRY (dialog->details->server_entry));
		str = g_strdup_printf (_("The server at \"%s\" cannot be found."), server);
		label = gtk_label_new (str);
		entry = dialog->details->server_entry;

		g_free (str);

		break;		
	case G_IO_ERROR_FAILED:
	default:
		label = gtk_label_new (error->message);
		break;
	}

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (dialog->details->info_bar);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 6);
	gtk_widget_show (hbox);

	icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					 GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 6);
	gtk_widget_show (icon);

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);
	gtk_widget_show (label);

	if (entry != NULL) {
		iconize_entry (dialog, entry);
	}

	dialog->details->info_bar_content = hbox;

	gtk_button_set_label (GTK_BUTTON (dialog->details->connect_button),
			      _("Try Again"));
	gtk_widget_set_sensitive (dialog->details->connect_button, TRUE);
}

static void
connect_dialog_finish_fill (CajaConnectServerDialog *dialog)
{
	GAskPasswordFlags flags;
	GMountOperation *op;

	flags = dialog->details->fill_details_flags;
	op = G_MOUNT_OPERATION (dialog->details->fill_operation);

	if (flags & G_ASK_PASSWORD_NEED_PASSWORD) {
		g_mount_operation_set_password (op, gtk_entry_get_text (GTK_ENTRY (dialog->details->password_entry)));
	}

	if (flags & G_ASK_PASSWORD_NEED_USERNAME) {
		g_mount_operation_set_username (op, gtk_entry_get_text (GTK_ENTRY (dialog->details->user_entry)));
	}

	if (flags & G_ASK_PASSWORD_NEED_DOMAIN) {
		g_mount_operation_set_domain (op, gtk_entry_get_text (GTK_ENTRY (dialog->details->domain_entry)));
	}

	if (flags & G_ASK_PASSWORD_SAVING_SUPPORTED &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->details->remember_checkbox))) {
		g_mount_operation_set_password_save (op, G_PASSWORD_SAVE_PERMANENTLY);
	}

	connect_dialog_set_connecting (dialog);

	g_simple_async_result_set_op_res_gboolean (dialog->details->fill_details_res, TRUE);
	g_simple_async_result_complete (dialog->details->fill_details_res);

	g_object_unref (dialog->details->fill_details_res);
	dialog->details->fill_details_res = NULL;

	g_object_unref (dialog->details->fill_operation);
	dialog->details->fill_operation = NULL;
}

static void
connect_dialog_request_additional_details (CajaConnectServerDialog *self,
					   GAskPasswordFlags flags,
					   const gchar *default_user,
					   const gchar *default_domain)
{
	GtkWidget *content_area, *label, *entry, *hbox, *icon;

	self->details->fill_details_flags = flags;

	connect_dialog_restore_info_bar (self, GTK_MESSAGE_WARNING);

	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (self->details->info_bar));
	entry = NULL;

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 6);
	gtk_widget_show (hbox);

	icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					 GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 6);
	gtk_widget_show (icon);

	label = gtk_label_new (_("Please verify your user details."));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);
	gtk_widget_show (label);

	if (flags & G_ASK_PASSWORD_NEED_PASSWORD) {
		iconize_entry (self, self->details->password_entry);
	}

	if (flags & G_ASK_PASSWORD_NEED_USERNAME) {
		if (default_user != NULL && g_strcmp0 (default_user, "") != 0) {
			gtk_entry_set_text (GTK_ENTRY (self->details->user_entry),
					    default_user);
		} else {
			iconize_entry (self, self->details->user_entry);
		}
	}

	if (flags & G_ASK_PASSWORD_NEED_DOMAIN) {
		if (default_domain != NULL && g_strcmp0 (default_domain, "") != 0) {
			gtk_entry_set_text (GTK_ENTRY (self->details->domain_entry),
					    default_domain);
		} else {
			iconize_entry (self, self->details->domain_entry);
		}
	}

	self->details->info_bar_content = hbox;

	gtk_widget_set_sensitive (self->details->connect_button, TRUE);
	gtk_button_set_label (GTK_BUTTON (self->details->connect_button),
			      _("Continue"));

	if (!(flags & G_ASK_PASSWORD_SAVING_SUPPORTED)) {
		g_signal_handler_disconnect (self->details->password_entry,
					     self->details->password_sensitive_id);
		self->details->password_sensitive_id = 0;

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->details->remember_checkbox),
					      FALSE);
		gtk_widget_set_sensitive (self->details->remember_checkbox, FALSE);
	}
}

static void
display_location_async_cb (GObject *source,
			   GAsyncResult *res,
			   gpointer user_data)
{
	CajaConnectServerDialog *dialog;
	GError *error;

	dialog = CAJA_CONNECT_SERVER_DIALOG (source);
	error = NULL;

	caja_connect_server_dialog_display_location_finish (dialog,
								res, &error);

	if (error != NULL) {
		connect_dialog_set_info_bar_error (dialog, error);
		g_error_free (error);
	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
mount_enclosing_ready_cb (GObject *source,
			  GAsyncResult *res,
			  gpointer user_data)
{
	GFile *location;
	CajaConnectServerDialog *dialog;
	GError *error;

	error = NULL;
	location = G_FILE (source);
	dialog = user_data;

	g_file_mount_enclosing_volume_finish (location, res, &error);

	if (!error || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)) {
		/* volume is mounted, show it */
		caja_connect_server_dialog_display_location_async (dialog,
								       dialog->details->application, location,
								       display_location_async_cb, NULL);
	} else {
		if (dialog->details->should_destroy) {
			gtk_widget_destroy (GTK_WIDGET (dialog));
		} else {
			connect_dialog_set_info_bar_error (dialog, error);
		}
	}

	if (error != NULL) {
		g_error_free (error);
	}
}

static void
connect_dialog_present_uri_async (CajaConnectServerDialog *self,
				  CajaApplication *application,
				  GFile *location)
{
	GMountOperation *op;

	op = caja_connect_server_operation_new (self);
	g_file_mount_enclosing_volume (location,
				       0, op, NULL,
				       mount_enclosing_ready_cb, self);
	g_object_unref (op);
}

static void
connect_dialog_connect_to_server (CajaConnectServerDialog *dialog)
{
    struct MethodInfo *meth;
    GFile *location;
    int index;
    GtkTreeIter iter;
    char *user, *initial_path, *server, *folder, *domain, *port_str;
    char *t, *join, *uri;
    double port;

    /* Get our method info */
    gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo), &iter);
    gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
                        &iter, 0, &index, -1);
    g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
    meth = &(methods[index]);

    server = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->server_entry), 0, -1);

    user = NULL;
	initial_path = g_strdup ("");
    domain = NULL;
    folder = NULL;

    /* FTP special case */
    if (meth->flags & IS_ANONYMOUS) {
    	user = g_strdup ("anonymous");

        /* SMB special case */
    } else if (strcmp (meth->scheme, "smb") == 0) {
		g_free (initial_path);

    	t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->share_entry), 0, -1);
    	initial_path = g_strconcat ("/", t, NULL);

        g_free (t);
    }

    /* username */
    if (!user) {
    	t = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->user_entry), 0, -1);
    	user = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);
    	g_free (t);
    }

    /* domain */
    domain = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->domain_entry), 0, -1);

    if (strlen (domain) != 0) {
    	t = user;

    	user = g_strconcat (domain , ";" , t, NULL);
    	g_free (t);
    }

    /* folder */
    folder = gtk_editable_get_chars (GTK_EDITABLE (dialog->details->folder_entry), 0, -1);

    if (folder[0] != 0 &&
        folder[0] != '/') {
    	join = "/";
    } else {
    	join = "";
    }

	t = folder;
	folder = g_strconcat (initial_path, join, t, NULL);
	g_free (t);

    t = folder;
    folder = g_uri_escape_string (t, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, FALSE);
    g_free (t);

    /* port */
	port = gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->details->port_spinbutton));

    if (port != 0 && port != meth->default_port) {
    	port_str = g_strdup_printf ("%d", (int) port);
    } else {
    	port_str = NULL;
    }

    /* final uri */
    uri = g_strdup_printf ("%s://%s%s%s%s%s%s",
    		       meth->scheme,
    		       (user != NULL) ? user : "",
    		       (user[0] != 0) ? "@" : "",
    		       server,
    		       (port_str != NULL) ? ":" : "",
    		       (port_str != NULL) ? port_str : "",
    		       (folder != NULL) ? folder : "");

    g_free (initial_path);
    g_free (server);
    g_free (folder);
    g_free (user);
    g_free (domain);
    g_free (port_str);

    location = g_file_new_for_uri (uri);
    g_free (uri);

	connect_dialog_set_connecting (dialog);
	connect_dialog_present_uri_async (dialog,
					  dialog->details->application,
					  location);

    g_object_unref (location);
}

static void
connect_to_server_or_finish_fill (CajaConnectServerDialog *dialog)
{
    if (dialog->details->fill_details_res != NULL) {
		connect_dialog_finish_fill (dialog);
    } else {
		connect_dialog_connect_to_server (dialog);
    }
}

static gboolean
connect_dialog_abort_mount_operation (CajaConnectServerDialog *dialog)
{
    if (dialog->details->fill_details_res != NULL) {
    	g_simple_async_result_set_op_res_gboolean (dialog->details->fill_details_res, FALSE);
    	g_simple_async_result_complete (dialog->details->fill_details_res);

    	g_object_unref (dialog->details->fill_details_res);
    	dialog->details->fill_details_res = NULL;

    	if (dialog->details->fill_operation) {
    		g_object_unref (dialog->details->fill_operation);
    		dialog->details->fill_operation = NULL;
        }

        return TRUE;
    }

    return FALSE;
}

static void
connect_dialog_destroy (CajaConnectServerDialog *dialog)
{
	if (connect_dialog_abort_mount_operation (dialog)) {
    	dialog->details->should_destroy = TRUE;
    } else {
    	gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
connect_dialog_response_cb (CajaConnectServerDialog *dialog,
			    int response_id,
			    gpointer data)
{
    GError *error;

    switch (response_id)
    {
    case RESPONSE_CONNECT:
		connect_to_server_or_finish_fill (dialog);
        break;
    case GTK_RESPONSE_NONE:
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CANCEL:
		connect_dialog_destroy (dialog);
        break;
    case GTK_RESPONSE_HELP :
        error = NULL;
        gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (dialog)),
                      "ghelp:user-guide#caja-server-connect",
                      gtk_get_current_event_time (), &error);
        if (error)
        {
            eel_show_error_dialog (_("There was an error displaying help."), error->message,
                                   GTK_WINDOW (dialog));
            g_error_free (error);
        }
        break;
    default :
        g_assert_not_reached ();
    }
}

static void
connect_dialog_cleanup (CajaConnectServerDialog *dialog)
{
	/* hide the infobar */
	gtk_widget_hide (dialog->details->info_bar);

	/* set the connect button label back to 'Connect' */
	gtk_button_set_label (GTK_BUTTON (dialog->details->connect_button),
			      _("C_onnect"));

	/* if there was a pending mount operation, cancel it. */
	connect_dialog_abort_mount_operation (dialog);

	/* restore password checkbox sensitivity */
	if (dialog->details->password_sensitive_id == 0) {
		dialog->details->password_sensitive_id =
			g_signal_connect (dialog->details->password_entry, "changed",
					  G_CALLBACK (sensitive_entry_changed_callback),
					  dialog->details->remember_checkbox);
		sensitive_entry_changed_callback (GTK_EDITABLE (dialog->details->password_entry),
						  dialog->details->remember_checkbox);
	}

	/* remove icons on the entries */
	g_list_foreach (dialog->details->iconized_entries,
			(GFunc) iconized_entry_restore, dialog);
	g_list_free (dialog->details->iconized_entries);
	dialog->details->iconized_entries = NULL;

	dialog->details->last_password_set = FALSE;
}

static void
connect_dialog_setup_for_type (CajaConnectServerDialog *dialog)
{
    struct MethodInfo *meth;
    int index;
    GtkTreeIter iter;

	connect_dialog_cleanup (dialog);

	/* get our method info */
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (dialog->details->type_combo),
					    &iter)) {
		/* there are no entries in the combo, something is wrong
		 * with our GVfs installation.
		 */
		connect_dialog_gvfs_error (dialog);

		return;
	}

    gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->details->type_combo)),
                        &iter, 0, &index, -1);
    g_assert (index < G_N_ELEMENTS (methods) && index >= 0);
    meth = &(methods[index]);

    g_object_set (dialog->details->share_entry,
    	      "visible",
    	      (meth->flags & SHOW_SHARE) != 0,
    	      NULL);

    g_object_set (dialog->details->port_spinbutton,
    	      "sensitive",
    	      (meth->flags & SHOW_PORT) != 0,
    	      "value", (gdouble) meth->default_port,
    	      NULL);

    g_object_set (dialog->details->user_details,
    	      "visible",
    	      (meth->flags & SHOW_USER) != 0 ||
    	      (meth->flags & SHOW_DOMAIN) != 0,
    	      NULL);

    g_object_set (dialog->details->user_entry,
    	      "visible",
    	      (meth->flags & SHOW_USER) != 0,
    	      NULL);

	g_object_set (dialog->details->password_entry,
		      "visible",
		      (meth->flags & SHOW_USER) != 0,
		      NULL);

    g_object_set (dialog->details->domain_entry,
    	      "visible",
    	      (meth->flags & SHOW_DOMAIN) != 0,
    	      NULL);
}

static void
sensitive_entry_changed_callback (GtkEditable *editable,
				  GtkWidget *widget)
{
    guint length;

    length = gtk_entry_get_text_length (GTK_ENTRY (editable));

	gtk_widget_set_sensitive (widget, length > 0);
}

static void
bind_visibility (CajaConnectServerDialog *dialog,
		 GtkWidget *source,
		 GtkWidget *dest)
{
    g_object_bind_property (source,
    			"visible",
    			dest,
    			"visible",
    			G_BINDING_DEFAULT);
}

static void
caja_connect_server_dialog_init (CajaConnectServerDialog *dialog)
{
    GtkWidget *label;
    GtkWidget *alignment;
    GtkWidget *content_area;
    GtkWidget *combo ,* table;
	GtkWidget *hbox, *connect_button, *checkbox;
    GtkListStore *store;
    GtkCellRenderer *renderer;
    gchar *str;
    int i;

    dialog->details = G_TYPE_INSTANCE_GET_PRIVATE (dialog, CAJA_TYPE_CONNECT_SERVER_DIALOG,
						   CajaConnectServerDialogDetails);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    /* set dialog properties */
    gtk_window_set_title (GTK_WINDOW (dialog), _("Connect to Server"));
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
    gtk_box_set_spacing (GTK_BOX (content_area), 2);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	/* infobar */
	dialog->details->info_bar = gtk_info_bar_new ();
	gtk_info_bar_set_message_type (GTK_INFO_BAR (dialog->details->info_bar),
				       GTK_MESSAGE_INFO);
	gtk_box_pack_start (GTK_BOX (content_area), dialog->details->info_bar,
			    FALSE, FALSE, 6);

    /* server settings label */
    label = gtk_label_new (NULL);
    str = g_strdup_printf ("<b>%s</b>", _("Server Details"));
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 6);
    gtk_widget_show (label);

    /* server settings alignment */
    alignment = gtk_alignment_new (0, 0, 0, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
    			   0, 0, 12, 0);
    gtk_box_pack_start (GTK_BOX (content_area), alignment, TRUE, TRUE, 0);
    gtk_widget_show (alignment);

    table = gtk_table_new (4, 2, FALSE);
    gtk_container_add (GTK_CONTAINER (alignment), table);
    gtk_widget_show (table);

	dialog->details->primary_table = table;

    /* first row: server entry + port spinbutton */
    label = gtk_label_new_with_mnemonic (_("_Server:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  0, 1,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
    gtk_widget_show (label);

    hbox = gtk_hbox_new (FALSE, 6);
    gtk_widget_show (hbox);
    gtk_table_attach (GTK_TABLE (table), hbox,
    		  1, 2,
    		  0, 1,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    dialog->details->server_entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->server_entry), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->details->server_entry, FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->server_entry);
    gtk_widget_show (dialog->details->server_entry);

    /* port */
    label = gtk_label_new_with_mnemonic (_("_Port:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    dialog->details->port_spinbutton =
    	gtk_spin_button_new_with_range (0.0, 65535.0, 1.0);
    g_object_set (dialog->details->port_spinbutton,
    	      "digits", 0,
    	      "numeric", TRUE,
    	      "update-policy", GTK_UPDATE_IF_VALID,
    	      NULL);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->details->port_spinbutton,
    		    FALSE, FALSE, 0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->port_spinbutton);
    gtk_widget_show (dialog->details->port_spinbutton);

    /* second row: type combobox */
    label = gtk_label_new (_("Type:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  1, 2,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
    gtk_widget_show (label);

    dialog->details->type_combo = combo = gtk_combo_box_new ();

    /* each row contains: method index, textual description */
    store = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
    gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
    g_object_unref (store);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 1);

    for (i = 0; i < G_N_ELEMENTS (methods); i++)
    {
        GtkTreeIter iter;
        const gchar * const *supported;
        int j;

		/* skip methods that don't have corresponding gvfs uri schemes */
        supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

        if (methods[i].scheme != NULL)
        {
            gboolean found;

            found = FALSE;
            for (j = 0; supported[j] != NULL; j++)
            {
                if (strcmp (methods[i].scheme, supported[j]) == 0)
                {
                    found = TRUE;
                    break;
                }
            }

            if (!found)
            {
                continue;
            }
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, i,
                            1, get_method_description (&(methods[i])),
                            -1);


        if (methods[i].flags & DEFAULT_METHOD)
        {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
        }
    }

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) < 0)
    {
        /* default method not available, use any other */
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }

    gtk_widget_show (combo);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
    gtk_table_attach (GTK_TABLE (table), combo,
    		  1, 2,
    		  1, 2,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND, 6, 3);
    g_signal_connect_swapped (combo, "changed",
				  G_CALLBACK (connect_dialog_setup_for_type),
    			  dialog);

    /* third row: share entry */
    label = gtk_label_new (_("Share:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  2, 3,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    dialog->details->share_entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->share_entry), TRUE);
    gtk_table_attach (GTK_TABLE (table), dialog->details->share_entry,
    		  1, 2,
    		  2, 3,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    bind_visibility (dialog, dialog->details->share_entry, label);

    /* fourth row: folder entry */
    label = gtk_label_new (_("Folder:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  3, 4,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
    gtk_widget_show (label);
    dialog->details->folder_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (dialog->details->folder_entry), "/");
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->folder_entry), TRUE);
    gtk_table_attach (GTK_TABLE (table), dialog->details->folder_entry,
    		  1, 2,
    		  3, 4,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);
    gtk_widget_show (dialog->details->folder_entry);

    /* user details label */
    label = gtk_label_new (NULL);
    str = g_strdup_printf ("<b>%s</b>", _("User Details"));
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (content_area), label, FALSE, FALSE, 6);

    /* user details alignment */
    alignment = gtk_alignment_new (0, 0, 0, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
    			   0, 0, 12, 0);
    gtk_box_pack_start (GTK_BOX (content_area), alignment, TRUE, TRUE, 0);

    bind_visibility (dialog, alignment, label);
    dialog->details->user_details = alignment;

	table = gtk_table_new (4, 2, FALSE);
    gtk_container_add (GTK_CONTAINER (alignment), table);
    gtk_widget_show (table);

    /* first row: domain entry */
    label = gtk_label_new (_("Domain Name:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  0, 1,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    dialog->details->domain_entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->domain_entry), TRUE);
    gtk_table_attach (GTK_TABLE (table), dialog->details->domain_entry,
    		  1, 2,
    		  0, 1,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    bind_visibility (dialog, dialog->details->domain_entry, label);

    /* second row: username entry */
    label = gtk_label_new (_("User Name:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_table_attach (GTK_TABLE (table), label,
    		  0, 1,
    		  1, 2,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    dialog->details->user_entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->user_entry), TRUE);
    gtk_table_attach (GTK_TABLE (table), dialog->details->user_entry,
    		  1, 2,
    		  1, 2,
    		  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

    bind_visibility (dialog, dialog->details->user_entry, label);

	/* third row: password entry */
	label = gtk_label_new (_("Password:"));
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  2, 3,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	dialog->details->password_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->password_entry), TRUE);
	gtk_entry_set_visibility (GTK_ENTRY (dialog->details->password_entry), FALSE);
	gtk_table_attach (GTK_TABLE (table), dialog->details->password_entry,
			  1, 2,
			  2, 3,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 3);

	bind_visibility (dialog, dialog->details->password_entry, label);

	/* fourth row: remember checkbox */
	checkbox = gtk_check_button_new_with_label (_("Remember this password"));
	gtk_table_attach (GTK_TABLE (table), checkbox,
			  1, 2,
			  3, 4,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 6, 0);
	dialog->details->remember_checkbox = checkbox;

	bind_visibility (dialog, dialog->details->password_entry, checkbox);

    gtk_dialog_add_button (GTK_DIALOG (dialog),
                           GTK_STOCK_HELP,
                           GTK_RESPONSE_HELP);
    gtk_dialog_add_button (GTK_DIALOG (dialog),
                           GTK_STOCK_CANCEL,
                           GTK_RESPONSE_CANCEL);
    connect_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
    					_("C_onnect"),
    					RESPONSE_CONNECT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                     RESPONSE_CONNECT);
	dialog->details->connect_button = connect_button;

    g_signal_connect (dialog->details->server_entry, "changed",
			  G_CALLBACK (sensitive_entry_changed_callback),
    		  connect_button);
	sensitive_entry_changed_callback (GTK_EDITABLE (dialog->details->server_entry),
					  connect_button);

    g_signal_connect (dialog, "response",
			  G_CALLBACK (connect_dialog_response_cb),
                      dialog);

	connect_dialog_setup_for_type (dialog);
}

static void
caja_connect_server_dialog_finalize (GObject *object)
{
	CajaConnectServerDialog *dialog;

	dialog = CAJA_CONNECT_SERVER_DIALOG (object);

	connect_dialog_abort_mount_operation (dialog);

	if (dialog->details->iconized_entries != NULL) {
		g_list_free (dialog->details->iconized_entries);
		dialog->details->iconized_entries = NULL;
	}

	G_OBJECT_CLASS (caja_connect_server_dialog_parent_class)->finalize (object);
}

static void
caja_connect_server_dialog_class_init (CajaConnectServerDialogClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);
	oclass->finalize = caja_connect_server_dialog_finalize;

	g_type_class_add_private (class, sizeof (CajaConnectServerDialogDetails));
}

GtkWidget *
caja_connect_server_dialog_new (CajaWindow *window)
{
    CajaConnectServerDialog *conndlg;
    GtkWidget *dialog;

    dialog = gtk_widget_new (CAJA_TYPE_CONNECT_SERVER_DIALOG, NULL);
    conndlg = CAJA_CONNECT_SERVER_DIALOG (dialog);

    if (window)
    {
        gtk_window_set_screen (GTK_WINDOW (dialog),
                               gtk_window_get_screen (GTK_WINDOW (window)));
        conndlg->details->application = window->application;
    }

    return dialog;
}

gboolean
caja_connect_server_dialog_fill_details_finish (CajaConnectServerDialog *self,
						    GAsyncResult *result)
{
	return g_simple_async_result_get_op_res_gboolean (G_SIMPLE_ASYNC_RESULT (result));
}

void
caja_connect_server_dialog_fill_details_async (CajaConnectServerDialog *self,
						   GMountOperation *operation,
						   const gchar *default_user,
						   const gchar *default_domain,
						   GAskPasswordFlags flags,
						   GAsyncReadyCallback callback,
						   gpointer user_data)
{
	GSimpleAsyncResult *fill_details_res;
	const gchar *str;
	GAskPasswordFlags set_flags;

	fill_details_res = g_simple_async_result_new (G_OBJECT (self), callback, user_data,
						      caja_connect_server_dialog_fill_details_async);

	self->details->fill_details_res = fill_details_res;
	set_flags = (flags & G_ASK_PASSWORD_NEED_PASSWORD) |
		(flags & G_ASK_PASSWORD_NEED_USERNAME) |
		(flags & G_ASK_PASSWORD_NEED_DOMAIN);

	if (set_flags & G_ASK_PASSWORD_NEED_PASSWORD) {
		/* provide the password */
		str = gtk_entry_get_text (GTK_ENTRY (self->details->password_entry));

		if (str != NULL && g_strcmp0 (str, "") != 0 &&
		    !self->details->last_password_set) {
			g_mount_operation_set_password (G_MOUNT_OPERATION (operation),
							str);
			set_flags ^= G_ASK_PASSWORD_NEED_PASSWORD;

			self->details->last_password_set = TRUE;
		}
	}

	if (set_flags & G_ASK_PASSWORD_NEED_USERNAME) {
		/* see if the default username is different from ours */
		str = gtk_entry_get_text (GTK_ENTRY (self->details->user_entry));

		if (str != NULL && g_strcmp0 (str, "") != 0 &&
		    g_strcmp0 (str, default_user) != 0) {
			g_mount_operation_set_username (G_MOUNT_OPERATION (operation),
							str);
			set_flags ^= G_ASK_PASSWORD_NEED_USERNAME;
		}
	}

	if (set_flags & G_ASK_PASSWORD_NEED_DOMAIN) {
		/* see if the default domain is different from ours */
		str = gtk_entry_get_text (GTK_ENTRY (self->details->domain_entry));

		if (str != NULL && g_strcmp0 (str, "") &&
		    g_strcmp0 (str, default_domain) != 0) {
			g_mount_operation_set_domain (G_MOUNT_OPERATION (operation),
						      str);
			set_flags ^= G_ASK_PASSWORD_NEED_DOMAIN;
		}
	}

	if (set_flags != 0) {
		set_flags |= (flags & G_ASK_PASSWORD_SAVING_SUPPORTED);
		self->details->fill_operation = g_object_ref (operation);
		connect_dialog_request_additional_details (self, set_flags, default_user, default_domain);
	} else {
		g_simple_async_result_set_op_res_gboolean (fill_details_res, TRUE);
		g_simple_async_result_complete (fill_details_res);
		g_object_unref (self->details->fill_details_res);

		self->details->fill_details_res = NULL;
	}
}
