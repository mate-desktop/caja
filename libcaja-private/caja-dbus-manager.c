/*
 * caja-dbus-manager: caja DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "caja-dbus-manager.h"

#include "caja-file-operations.h"

#include <gio/gio.h>

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.mate.Caja.FileOperations'>"
  "    <method name='CopyURIs'>"
  "      <arg type='as' name='URIList' direction='in'/>"
  "      <arg type='s' name='Destination' direction='in'/>"
  "    </method>"
  "    <method name='EmptyTrash'>"
  "    </method>"
  "  </interface>"
  "</node>";

typedef struct _CajaDBusManager CajaDBusManager;
typedef struct _CajaDBusManagerClass CajaDBusManagerClass;

struct _CajaDBusManager {
  GObject parent;

  GDBusConnection *connection;

  guint owner_id;
  guint registration_id;
};

struct _CajaDBusManagerClass {
  GObjectClass parent_class;
};

static GType caja_dbus_manager_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (CajaDBusManager, caja_dbus_manager, G_TYPE_OBJECT);

static CajaDBusManager *singleton = NULL;

static void
caja_dbus_manager_dispose (GObject *object)
{
  CajaDBusManager *self = (CajaDBusManager *) object;

  if (self->registration_id != 0)
    {
      g_dbus_connection_unregister_object (self->connection, self->registration_id);
      self->registration_id = 0;
    }

  if (self->owner_id != 0)
    {
      g_bus_unown_name (self->owner_id);
      self->owner_id = 0;
    }

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (caja_dbus_manager_parent_class)->dispose (object);
}

static void
caja_dbus_manager_class_init (CajaDBusManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = caja_dbus_manager_dispose;
}

static void
trigger_copy_file_operation (const gchar **sources,
                             const gchar *destination)
{
  GList *source_files = NULL;
  GFile *dest_dir;
  gint idx;

  if (sources == NULL || sources[0] == NULL || destination == NULL)
    {
      g_debug ("Called 'CopyURIs' with NULL arguments, discarding");
      return;
    }

  dest_dir = g_file_new_for_uri (destination);

  for (idx = 0; sources[idx] != NULL; idx++)
    source_files = g_list_prepend (source_files,
                                   g_file_new_for_uri (sources[idx]));

  caja_file_operations_copy (source_files, NULL,
                                 dest_dir,
                                 NULL, NULL, NULL);

  g_list_free_full (source_files, g_object_unref);
  g_object_unref (dest_dir);
}

static void
trigger_empty_trash_operation (void)
{
  caja_file_operations_empty_trash (NULL);
}

static void
handle_method_call (GDBusConnection *connection,
                    const gchar *sender,
                    const gchar *object_path,
                    const gchar *interface_name,
                    const gchar *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
  const gchar **uris = NULL;
  const gchar *destination_uri = NULL;

  if (g_strcmp0 (method_name, "CopyURIs") == 0)
    {
      g_variant_get (parameters, "(^a&s&s)", &uris, &destination_uri);

      trigger_copy_file_operation (uris, destination_uri);

      g_debug ("Called CopyURIs with dest %s and uri %s\n", destination_uri, uris[0]);

      goto out;
    }

  if (g_strcmp0 (method_name, "EmptyTrash") == 0)
    {
      trigger_empty_trash_operation ();

      g_debug ("Called EmptyTrash");
    }

 out:
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL,
};

static void
bus_acquired_handler_cb (GDBusConnection *conn,
                         const gchar *name,
                         gpointer user_data)
{
  CajaDBusManager *self = user_data;
  GDBusNodeInfo *introspection_data;
  GError *error = NULL;

  self->connection = g_object_ref (conn);
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, &error);

  if (error != NULL)
    {
      g_warning ("Error parsing the FileOperations XML interface: %s", error->message);
      g_error_free (error);

      g_bus_unown_name (self->owner_id);
      self->owner_id = 0;

      return;
    }

  self->registration_id = g_dbus_connection_register_object (conn,
                                                             "/org/mate/Caja",
                                                             introspection_data->interfaces[0],
                                                             &interface_vtable,
                                                             self,
                                                             NULL, &error);

  if (error != NULL)
    {
      g_warning ("Error registering the FileOperations proxy on the bus: %s", error->message);
      g_error_free (error);

      g_bus_unown_name (self->owner_id);

      return;
    }
}

static void
caja_dbus_manager_init (CajaDBusManager *self)
{
  self->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                   "org.mate.Caja",
                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                   bus_acquired_handler_cb,
                                   NULL,
                                   NULL,
                                   self,
                                   NULL);
}

void
caja_dbus_manager_start (void)
{
  singleton = g_object_new (caja_dbus_manager_get_type (),
                            NULL);
}

void
caja_dbus_manager_stop (void)
{
  g_clear_object (&singleton);
}
