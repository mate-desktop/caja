/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* CajaUndoStackManager - Manages undo of file operations (implementation)
 *
 * Copyright (C) 2007-2010 Amos Brocco
 * Copyright (C) 2011 Stefano Karapetsas
 *
 * Authors: Amos Brocco <amos.brocco@unifr.ch>,
 *          Stefano Karapetsas <stefano@karapetsas.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "caja-undostack-manager.h"
#include "caja-file-operations.h"
#include "caja-file.h"
#include <gio/gio.h>
#include <glib/gprintf.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <gdk/gdk.h>

/* *****************************************************************
 Private fields
 ***************************************************************** */

struct _CajaUndoStackActionData
{
  /* Common stuff */
  CajaUndoStackActionType type;
  gboolean isValid;
  gboolean locked;              /* True if the action is being undone/redone */
  gboolean freed;               /* True if the action must be freed after undo/redo */
  guint count;                  /* Size of affected uris (count of items) */
  CajaUndoStackManager *manager;    /* Pointer to the manager */

  /* Copy / Move stuff */
  GFile *src_dir;
  GFile *dest_dir;
  GList *sources;               /* Relative to src_dir */
  GList *destinations;          /* Relative to dest_dir */

  /* Cached labels/descriptions */
  char *undo_label;
  char *undo_description;
  char *redo_label;
  char *redo_description;

  /* Create new file/folder stuff/set permissions */
  char *template;
  char *target_uri;

  /* Rename stuff */
  char *old_uri;
  char *new_uri;

  /* Trash stuff */
  GHashTable *trashed;

  /* Recursive change permissions stuff */
  GHashTable *original_permissions;
  guint32 dir_mask;
  guint32 dir_permissions;
  guint32 file_mask;
  guint32 file_permissions;

  /* Single file change permissions stuff */
  guint32 current_permissions;
  guint32 new_permissions;

  /* Group */
  char *original_group_name_or_id;
  char *new_group_name_or_id;

  /* Owner */
  char *original_user_name_or_id;
  char *new_user_name_or_id;

};

struct _CajaUndoStackManagerPrivate
{
  /* Private fields */
  GQueue *stack;
  guint undo_levels;
  guint index;
  GMutex mutex;                /* Used to protect access to stack (because of async file ops) */
  gboolean dispose_has_run;
  gboolean undo_redo_flag;
  gboolean confirm_delete;
};

/* *****************************************************************
 Properties management prototypes
 ***************************************************************** */
enum
{
  PROP_UNDOSTACK_MANAGER_0, PROP_UNDO_LEVELS, PROP_CONFIRM_DELETE
};

static void caja_undostack_manager_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

static void caja_undostack_manager_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

/* *****************************************************************
 Destructors prototypes
 ***************************************************************** */
static void caja_undostack_manager_finalize (GObject * object);

static void caja_undostack_manager_dispose (GObject * object);

/* *****************************************************************
 Type definition
 ***************************************************************** */
G_DEFINE_TYPE_WITH_PRIVATE (CajaUndoStackManager, caja_undostack_manager,
    G_TYPE_OBJECT);

/* *****************************************************************
 Private methods prototypes
 ***************************************************************** */

static void stack_clear_n_oldest (GQueue * stack, guint n);

static void stack_fix_size (CajaUndoStackManagerPrivate * priv);

static gboolean can_undo (CajaUndoStackManagerPrivate * priv);

static gboolean can_redo (CajaUndoStackManagerPrivate * priv);

static void stack_push_action (CajaUndoStackManagerPrivate * priv,
    CajaUndoStackActionData * action);

static CajaUndoStackActionData
    * stack_scroll_left (CajaUndoStackManagerPrivate * priv);

static CajaUndoStackActionData
    * stack_scroll_right (CajaUndoStackManagerPrivate * priv);

static CajaUndoStackActionData
    * get_next_redo_action (CajaUndoStackManagerPrivate * priv);

static CajaUndoStackActionData
    * get_next_undo_action (CajaUndoStackManagerPrivate * priv);

static gchar *get_undo_label (CajaUndoStackActionData * action);

static gchar *get_undo_description (CajaUndoStackActionData * action);

static gchar *get_redo_label (CajaUndoStackActionData * action);

static gchar *get_redo_description (CajaUndoStackActionData * action);

static void do_menu_update (CajaUndoStackManager * manager);

static void free_undostack_action (gpointer data, gpointer user_data);

static void undostack_dispose_all (GQueue * queue);

static void undo_redo_done_transfer_callback (GHashTable * debuting_uris,
    gpointer data);

static void undo_redo_op_callback (gpointer callback_data);

static void undo_redo_done_rename_callback (CajaFile * file,
    GFile * result_location, GError * error, gpointer callback_data);

static void undo_redo_done_delete_callback (GHashTable * debuting_uris,
    gboolean user_cancel, gpointer callback_data);

static void undo_redo_done_create_callback (GFile * new_file,
    gpointer callback_data);

static void clear_redo_actions (CajaUndoStackManagerPrivate * priv);

static gchar *get_first_target_short_name (CajaUndoStackActionData *
    action);

static GList *construct_gfile_list (const GList * urilist, GFile * parent);

static GList *construct_gfile_list_from_uri (char *uri);

static GList *uri_list_to_gfile_list (GList * urilist);

static char *get_uri_basename (char *uri);

static char *get_uri_parent (char *uri);

static char *get_uri_parent_path (char *uri);

static GHashTable *retrieve_files_to_restore (GHashTable * trashed);

/* *****************************************************************
 Base functions
 ***************************************************************** */
static void
caja_undostack_manager_class_init (CajaUndoStackManagerClass * klass)
{
  GParamSpec *undo_levels;
  GParamSpec *confirm_delete;
  GObjectClass *g_object_class;

  /* Create properties */
  undo_levels = g_param_spec_uint ("undo-levels", "undo levels",
      "Number of undo levels to be stored",
      1, UINT_MAX, 30, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  confirm_delete =
      g_param_spec_boolean ("confirm-delete", "confirm delete",
      "Always confirm file deletion", FALSE,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  /* Set properties get/set methods */
  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->set_property = caja_undostack_manager_set_property;
  g_object_class->get_property = caja_undostack_manager_get_property;

  /* Install properties */
  g_object_class_install_property (g_object_class, PROP_UNDO_LEVELS,
      undo_levels);

  g_object_class_install_property (g_object_class, PROP_CONFIRM_DELETE,
      confirm_delete);

  /* The UI menu needs to update its status */
  g_signal_new ("request-menu-update",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE |
      G_SIGNAL_NO_HOOKS, 0, NULL, NULL,
      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  /* Hook deconstructors */
  g_object_class->dispose = caja_undostack_manager_dispose;
  g_object_class->finalize = caja_undostack_manager_finalize;
}

static void
caja_undostack_manager_init (CajaUndoStackManager * self)
{
  CajaUndoStackManagerPrivate *priv;

  priv = caja_undostack_manager_get_instance_private (self);

  self->priv = priv;

  /* Initialize private fields */
  priv->stack = g_queue_new ();
  g_mutex_init (&priv->mutex);
  priv->index = 0;
  priv->dispose_has_run = FALSE;
  priv->undo_redo_flag = FALSE;
  priv->confirm_delete = FALSE;
}

static void
caja_undostack_manager_dispose (GObject * object)
{
  CajaUndoStackManager *self = CAJA_UNDOSTACK_MANAGER (object);
  CajaUndoStackManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  g_mutex_lock (&priv->mutex);

  /* Free each undoable action in the stack and the stack itself */
  undostack_dispose_all (priv->stack);
  g_queue_free (priv->stack);
  g_mutex_unlock (&priv->mutex);
  g_mutex_clear (&priv->mutex);

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (caja_undostack_manager_parent_class)->dispose (object);
}

static void
caja_undostack_manager_finalize (GObject * object)
{
  G_OBJECT_CLASS (caja_undostack_manager_parent_class)->finalize (object);
}

/* *****************************************************************
 Property management
 ***************************************************************** */
static void
caja_undostack_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  g_return_if_fail (IS_CAJA_UNDOSTACK_MANAGER (object));

  CajaUndoStackManager *manager = CAJA_UNDOSTACK_MANAGER (object);
  CajaUndoStackManagerPrivate *priv = manager->priv;
  guint new_undo_levels;

  switch (prop_id) {
    case PROP_UNDO_LEVELS:
      new_undo_levels = g_value_get_uint (value);
      if (new_undo_levels > 0 && (priv->undo_levels != new_undo_levels)) {
        priv->undo_levels = new_undo_levels;
        g_mutex_lock (&priv->mutex);
        stack_fix_size (priv);
        g_mutex_unlock (&priv->mutex);
        do_menu_update (manager);
      }
      break;
    case PROP_CONFIRM_DELETE:
      priv->confirm_delete = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
caja_undostack_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  g_return_if_fail (IS_CAJA_UNDOSTACK_MANAGER (object));

  CajaUndoStackManager *manager = CAJA_UNDOSTACK_MANAGER (object);
  CajaUndoStackManagerPrivate *priv = manager->priv;

  switch (prop_id) {
    case PROP_UNDO_LEVELS:
      g_value_set_uint (value, priv->undo_levels);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* *****************************************************************
 Public methods
 ***************************************************************** */

/** ****************************************************************
 * Returns the undo stack manager instance (singleton pattern)
 ** ****************************************************************/
CajaUndoStackManager *
caja_undostack_manager_instance (void)
{
  static CajaUndoStackManager *manager = NULL;

  if (manager == NULL) {
    manager =
        g_object_new (TYPE_CAJA_UNDOSTACK_MANAGER, "undo-levels", 32, NULL);
  }

  return manager;
}

/** ****************************************************************
 * True if undoing / redoing
 ** ****************************************************************/
gboolean
caja_undostack_manager_is_undo_redo (CajaUndoStackManager * manager)
{
  CajaUndoStackManagerPrivate *priv = manager->priv;
  if (priv->undo_redo_flag) {
    return TRUE;
  }

  return FALSE;
}

void
caja_undostack_manager_request_menu_update (CajaUndoStackManager *
    manager)
{
  do_menu_update (manager);
}

/** ****************************************************************
 * Redoes the last file operation
 ** ****************************************************************/
void
caja_undostack_manager_redo (CajaUndoStackManager * manager,
    GtkWidget * parent_view, CajaUndostackFinishCallback cb)
{
  CajaUndoStackManagerPrivate *priv = manager->priv;

  g_mutex_lock (&priv->mutex);

  CajaUndoStackActionData *action = stack_scroll_left (priv);

  /* Action will be NULL if redo is not possible */
  if (action != NULL) {
    action->locked = TRUE;
  }

  g_mutex_unlock (&priv->mutex);

  do_menu_update (manager);

  if (action != NULL) {
    action->locked = TRUE;      /* Remember to unlock when redo is finished */
    priv->undo_redo_flag = TRUE;
    switch (action->type) {
      case CAJA_UNDOSTACK_COPY:
      {
        GList *uris;

        uris = construct_gfile_list (action->sources, action->src_dir);
        caja_file_operations_copy (uris, NULL,
            action->dest_dir, NULL, undo_redo_done_transfer_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      }
      case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
      {
        char *new_name;
        char *puri;

        puri = get_uri_parent (action->target_uri);
        new_name = get_uri_basename (action->target_uri);
        caja_file_operations_new_file_from_template (NULL,
            NULL,
            puri,
            new_name, action->template, undo_redo_done_create_callback, action);
        g_free (puri);
        g_free (new_name);
        break;
      }
      case CAJA_UNDOSTACK_DUPLICATE:
      {
        GList *uris;

        uris = construct_gfile_list (action->sources, action->src_dir);
        caja_file_operations_duplicate (uris, NULL, NULL,
            undo_redo_done_transfer_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      }
      case CAJA_UNDOSTACK_RESTOREFROMTRASH:
      case CAJA_UNDOSTACK_MOVE:
      {
        GList *uris;

        uris = construct_gfile_list (action->sources, action->src_dir);
        caja_file_operations_move (uris, NULL,
            action->dest_dir, NULL, undo_redo_done_transfer_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      }
      case CAJA_UNDOSTACK_RENAME:
      {
        CajaFile *file;
        char *new_name;

        new_name = get_uri_basename (action->new_uri);
        file = caja_file_get_by_uri (action->old_uri);
        caja_file_rename (file, new_name,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        g_free (new_name);
        break;
      }
      case CAJA_UNDOSTACK_CREATEEMPTYFILE:
      {
        char *new_name;
        char *puri;

        puri = get_uri_parent (action->target_uri);
        new_name = get_uri_basename (action->target_uri);
        caja_file_operations_new_file (NULL, NULL, puri,
            new_name,
            action->template,
            0, undo_redo_done_create_callback, action);
        g_free (puri);
        g_free (new_name);
        break;
      }
      case CAJA_UNDOSTACK_CREATEFOLDER:
      {
        char *puri;

        puri = get_uri_parent (action->target_uri);
        caja_file_operations_new_folder (NULL, NULL, puri,
            undo_redo_done_create_callback, action);
        g_free (puri);
        break;
      }
      case CAJA_UNDOSTACK_MOVETOTRASH:
        if (g_hash_table_size (action->trashed) > 0) {
          GList *uris;

          GList *uri_to_trash = g_hash_table_get_keys (action->trashed);
          uris = uri_list_to_gfile_list (uri_to_trash);
          priv->undo_redo_flag = TRUE;
          caja_file_operations_trash_or_delete
              (uris, NULL, undo_redo_done_delete_callback, action);
          g_list_free (uri_to_trash);
    	  g_list_free_full (uris, g_object_unref);
        }
        break;
      case CAJA_UNDOSTACK_CREATELINK:
      {
        GList *uris;

        uris = construct_gfile_list (action->sources, action->src_dir);
        caja_file_operations_link (uris, NULL,
            action->dest_dir, NULL, undo_redo_done_transfer_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      }
      case CAJA_UNDOSTACK_SETPERMISSIONS:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_permissions (file,
            action->new_permissions, undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
      {
        char *puri;

        puri = g_file_get_uri (action->dest_dir);
        caja_file_set_permissions_recursive (puri,
            action->file_permissions,
            action->file_mask,
            action->dir_permissions,
            action->dir_mask, undo_redo_op_callback, action);
        g_free (puri);
        break;
      }
      case CAJA_UNDOSTACK_CHANGEGROUP:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_group (file,
            action->new_group_name_or_id,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_CHANGEOWNER:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_owner (file,
            action->new_user_name_or_id,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_DELETE:
      default:
        priv->undo_redo_flag = FALSE;
        break;                  /* We shouldn't be here */
    }
  }

  (*cb) ((gpointer) parent_view);
}

/** ****************************************************************
 * Undoes the last file operation
 ** ****************************************************************/
void
caja_undostack_manager_undo (CajaUndoStackManager * manager,
    GtkWidget * parent_view, CajaUndostackFinishCallback cb)
{
  CajaUndoStackManagerPrivate *priv = manager->priv;
  GList *uris = NULL;

  g_mutex_lock (&priv->mutex);

  CajaUndoStackActionData *action = stack_scroll_right (priv);

  if (action != NULL) {
    action->locked = TRUE;
  }

  g_mutex_unlock (&priv->mutex);

  do_menu_update (manager);

  if (action != NULL) {
    priv->undo_redo_flag = TRUE;
    switch (action->type) {
      case CAJA_UNDOSTACK_CREATEEMPTYFILE:
      case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
      case CAJA_UNDOSTACK_CREATEFOLDER:
        uris = construct_gfile_list_from_uri (action->target_uri);
      case CAJA_UNDOSTACK_COPY:
      case CAJA_UNDOSTACK_DUPLICATE:
      case CAJA_UNDOSTACK_CREATELINK:
        if (!uris) {
          uris = construct_gfile_list (action->destinations, action->dest_dir);
          uris = g_list_reverse (uris); // Deleting must be done in reverse
        }
        if (priv->confirm_delete) {
          caja_file_operations_delete (uris, NULL,
              undo_redo_done_delete_callback, action);
    	  g_list_free_full (uris, g_object_unref);
        } else {
          /* We skip the confirmation message
           */
          GList *f;
          for (f = uris; f != NULL; f = f->next) {
            char *name;
            name = g_file_get_uri (f->data);
            g_free (name);
            g_file_delete (f->data, NULL, NULL);
            g_object_unref (f->data);
          }
          g_list_free (uris);
          /* Here we must do what's necessary for the callback */
          undo_redo_done_transfer_callback (NULL, action);
        }
        break;
      case CAJA_UNDOSTACK_RESTOREFROMTRASH:
        uris = construct_gfile_list (action->destinations, action->dest_dir);
        caja_file_operations_trash_or_delete (uris, NULL,
            undo_redo_done_delete_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      case CAJA_UNDOSTACK_MOVETOTRASH:
      {
        GHashTable *files_to_restore;

        files_to_restore = retrieve_files_to_restore (action->trashed);
        if (g_hash_table_size (files_to_restore) > 0) {
          GList *l;
          GList *gfiles_in_trash = g_hash_table_get_keys (files_to_restore);
          GFile *item = NULL;
          GFile *dest = NULL;

          for (l = gfiles_in_trash; l != NULL; l = l->next) {
            char *value;

            item = l->data;
            value = g_hash_table_lookup (files_to_restore, item);
            dest = g_file_new_for_uri (value);
            g_file_move (item, dest,
                G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL, NULL, NULL, NULL);
            g_object_unref (dest);
          }

          g_list_free (gfiles_in_trash);
        }
        g_hash_table_destroy (files_to_restore);
        /* Here we must do what's necessary for the callback */
        undo_redo_done_transfer_callback (NULL, action);
        break;
      }
      case CAJA_UNDOSTACK_MOVE:
        uris = construct_gfile_list (action->destinations, action->dest_dir);
        caja_file_operations_move (uris, NULL,
            action->src_dir, NULL, undo_redo_done_transfer_callback, action);
    	g_list_free_full (uris, g_object_unref);
        break;
      case CAJA_UNDOSTACK_RENAME:
      {
        CajaFile *file;
        char *new_name;

        new_name = get_uri_basename (action->old_uri);
        file = caja_file_get_by_uri (action->new_uri);
        caja_file_rename (file, new_name,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        g_free (new_name);
        break;
      }
      case CAJA_UNDOSTACK_SETPERMISSIONS:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_permissions (file,
            action->current_permissions,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
        if (g_hash_table_size (action->original_permissions) > 0) {
          GList *gfiles_list =
              g_hash_table_get_keys (action->original_permissions);

          GList *l;
          GFile *dest = NULL;

          for (l = gfiles_list; l != NULL; l = l->next) {
            guint32 *perm;
            char *item;

            item = l->data;
            perm = g_hash_table_lookup (action->original_permissions, item);
            dest = g_file_new_for_uri (item);
            g_file_set_attribute_uint32 (dest,
                G_FILE_ATTRIBUTE_UNIX_MODE,
                *perm, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
            g_object_unref (dest);
          }

          g_list_free (gfiles_list);
          /* Here we must do what's necessary for the callback */
          undo_redo_done_transfer_callback (NULL, action);
        }
        break;
      case CAJA_UNDOSTACK_CHANGEGROUP:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_group (file,
            action->original_group_name_or_id,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_CHANGEOWNER:
      {
        CajaFile *file;

        file = caja_file_get_by_uri (action->target_uri);
        caja_file_set_owner (file,
            action->original_user_name_or_id,
            undo_redo_done_rename_callback, action);
        g_object_unref (file);
        break;
      }
      case CAJA_UNDOSTACK_DELETE:
      default:
        priv->undo_redo_flag = FALSE;
        break;                  /* We shouldn't be here */
    }
  }

  (*cb) ((gpointer) parent_view);
}

/** ****************************************************************
 * Adds an operation to the stack
 ** ****************************************************************/
void
caja_undostack_manager_add_action (CajaUndoStackManager * manager,
    CajaUndoStackActionData * action)
{
  CajaUndoStackManagerPrivate *priv = manager->priv;

  if (!action)
    return;

  if (!(action && action->isValid)) {
    free_undostack_action ((gpointer) action, NULL);
    return;
  }

  action->manager = manager;

  g_mutex_lock (&priv->mutex);

  stack_push_action (priv, action);

  g_mutex_unlock (&priv->mutex);

  do_menu_update (manager);
}

static GList *
get_all_trashed_items (GQueue *stack)
{
  CajaUndoStackActionData *action = NULL;
  GList *trash = NULL;
  GList *l;
  GQueue *tmp_stack = g_queue_copy(stack);

  while ((action = (CajaUndoStackActionData *) g_queue_pop_tail (tmp_stack)) != NULL)
    if (action->trashed)
        for (l = g_hash_table_get_keys (action->trashed); l != NULL; l=l->next) {
                trash = g_list_append(trash, l->data);
        }

  g_queue_free (tmp_stack);
  return (trash);
}

static gboolean
is_destination_uri_action_partof_trashed(GList *trash, GList *g)
{
    GList *l;
    char *uri;

    for (l = trash; l != NULL; l=l->next) {
        for (; g != NULL; g=g->next) {
            //printf ("destinations: %s\n", g_file_get_uri(l->data));
            uri = g_file_get_uri(g->data);
            if (!strcmp (uri, l->data)) {
               //printf ("GG %s\nZZ %s\n", uri, l->data);
               g_free (uri);
               return TRUE;
            }
            g_free (uri);
        }
    }

    return FALSE;
}
/** ****************************************************************
 * Callback after emptying the trash
 ** ****************************************************************/
void
caja_undostack_manager_trash_has_emptied (CajaUndoStackManager *
    manager)
{
  CajaUndoStackManagerPrivate *priv = manager->priv;

  /* Clear actions from the oldest to the newest move to trash */

  g_mutex_lock (&priv->mutex);

  clear_redo_actions (priv);
  CajaUndoStackActionData *action = NULL;

  GList *g;
  GQueue *tmp_stack = g_queue_copy(priv->stack);
  GList *trash = get_all_trashed_items (tmp_stack);
  while ((action = (CajaUndoStackActionData *) g_queue_pop_tail (tmp_stack)) != NULL)
  {
    if (action->destinations && action->dest_dir) {
        /* what a pain rebuild again and again an uri
        ** TODO change the struct add uri elements */
        g = construct_gfile_list (action->destinations, action->dest_dir);
        /* remove action for trashed item uris == destination action */
        if (is_destination_uri_action_partof_trashed(trash, g)) {
                g_queue_remove (priv->stack, action);
                continue;
        }
    }
    if (action->type == CAJA_UNDOSTACK_MOVETOTRASH) {
        g_queue_remove (priv->stack, action);
    }
  }

  g_queue_free (tmp_stack);
  g_mutex_unlock (&priv->mutex);
  do_menu_update (manager);
}

/** ****************************************************************
 * Returns the modification time for the given file (used for undo trash)
 ** ****************************************************************/
guint64
caja_undostack_manager_get_file_modification_time (GFile * file)
{
  GFileInfo *info;
  guint64 mtime;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, FALSE, NULL);
  if (info == NULL) {
    return -1;
  }

  mtime = g_file_info_get_attribute_uint64 (info,
      G_FILE_ATTRIBUTE_TIME_MODIFIED);

  g_object_unref (info);

  return mtime;
}

/** ****************************************************************
 * Returns a new undo data container
 ** ****************************************************************/
CajaUndoStackActionData *
caja_undostack_manager_data_new (CajaUndoStackActionType type,
    gint items_count)
{
  CajaUndoStackActionData *data =
      g_slice_new0 (CajaUndoStackActionData);
  data->type = type;
  data->count = items_count;

  if (type == CAJA_UNDOSTACK_MOVETOTRASH) {
    data->trashed =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  } else if (type == CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS) {
    data->original_permissions =
        g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  }

  return data;
}

/** ****************************************************************
 * Sets the source directory
 ** ****************************************************************/
void
caja_undostack_manager_data_set_src_dir (CajaUndoStackActionData *
    data, GFile * src)
{
  if (!data)
    return;

  data->src_dir = src;
}

/** ****************************************************************
 * Sets the destination directory
 ** ****************************************************************/
void
caja_undostack_manager_data_set_dest_dir (CajaUndoStackActionData *
    data, GFile * dest)
{
  if (!data)
    return;

  data->dest_dir = dest;
}

/** ****************************************************************
 * Pushes an origin, target pair in an existing undo data container
 ** ****************************************************************/
void caja_undostack_manager_data_add_origin_target_pair
    (CajaUndoStackActionData * data, GFile * origin, GFile * target)
{

  if (!data)
    return;

  char *src_relative = g_file_get_relative_path (data->src_dir, origin);
  data->sources = g_list_append (data->sources, src_relative);
  char *dest_relative = g_file_get_relative_path (data->dest_dir, target);
  data->destinations = g_list_append (data->destinations, dest_relative);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Pushes an trashed file with modification time in an existing undo data container
 ** ****************************************************************/
void
caja_undostack_manager_data_add_trashed_file (CajaUndoStackActionData
    * data, GFile * file, guint64 mtime)
{

  if (!data)
    return;

  guint64 *modificationTime;
  modificationTime = (guint64 *) g_malloc (sizeof (guint64));
  *modificationTime = mtime;

  char *originalURI = g_file_get_uri (file);

  g_hash_table_insert (data->trashed, originalURI, modificationTime);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Pushes a recursive permission change data in an existing undo data container
 ** ****************************************************************/
void caja_undostack_manager_data_add_file_permissions
    (CajaUndoStackActionData * data, GFile * file, guint32 permission)
{

  if (!data)
    return;

  guint32 *currentPermission;
  currentPermission = (guint32 *) g_malloc (sizeof (guint32));
  *currentPermission = permission;

  char *originalURI = g_file_get_uri (file);

  g_hash_table_insert (data->original_permissions, originalURI,
      currentPermission);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets the original file permission in an existing undo data container
 ** ****************************************************************/
void caja_undostack_manager_data_set_file_permissions
    (CajaUndoStackActionData * data, char *uri,
    guint32 current_permissions, guint32 new_permissions)
{

  if (!data)
    return;

  data->target_uri = uri;

  data->current_permissions = current_permissions;
  data->new_permissions = new_permissions;

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets the change owner information in an existing undo data container
 ** ****************************************************************/
void caja_undostack_manager_data_set_owner_change_information
    (CajaUndoStackActionData * data, char *uri,
    const char *current_user, const char *new_user)
{

  if (!data)
    return;

  data->target_uri = uri;

  data->original_user_name_or_id = g_strdup (current_user);
  data->new_user_name_or_id = g_strdup (new_user);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets the change group information in an existing undo data container
 ** ****************************************************************/
void caja_undostack_manager_data_set_group_change_information
    (CajaUndoStackActionData * data, char *uri,
    const char *current_group, const char *new_group)
{

  if (!data)
    return;

  data->target_uri = uri;

  data->original_group_name_or_id = g_strdup (current_group);
  data->new_group_name_or_id = g_strdup (new_group);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets the permission change mask
 ** ****************************************************************/
void caja_undostack_manager_data_set_recursive_permissions
    (CajaUndoStackActionData * data, guint32 file_permissions,
    guint32 file_mask, guint32 dir_permissions, guint32 dir_mask)
{

  if (!data)
    return;

  data->file_permissions = file_permissions;
  data->file_mask = file_mask;
  data->dir_permissions = dir_permissions;
  data->dir_mask = dir_mask;

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets create file information
 ** ****************************************************************/
void
caja_undostack_manager_data_set_create_data (CajaUndoStackActionData *
    data, char *target_uri, char *template)
{

  if (!data)
    return;

  data->template = g_strdup (template);
  data->target_uri = g_strdup (target_uri);

  data->isValid = TRUE;
}

/** ****************************************************************
 * Sets rename information
 ** ****************************************************************/
void caja_undostack_manager_data_set_rename_information
    (CajaUndoStackActionData * data, GFile * old_file, GFile * new_file)
{

  if (!data)
    return;

  data->old_uri = g_file_get_uri (old_file);
  data->new_uri = g_file_get_uri (new_file);

  data->isValid = TRUE;
}

/* *****************************************************************
 Private methods (nothing to see here, move along)
 ***************************************************************** */

static CajaUndoStackActionData *
stack_scroll_right (CajaUndoStackManagerPrivate * priv)
{
  gpointer data = NULL;

  if (!can_undo (priv))
    return NULL;

  data = g_queue_peek_nth (priv->stack, priv->index);
  if (priv->index < g_queue_get_length (priv->stack)) {
    priv->index++;
  }

  return data;
}

/** ---------------------------------------------------------------- */
static CajaUndoStackActionData *
stack_scroll_left (CajaUndoStackManagerPrivate * priv)
{
  gpointer data = NULL;

  if (!can_redo (priv))
    return NULL;

  priv->index--;
  data = g_queue_peek_nth (priv->stack, priv->index);

  return data;
}

/** ---------------------------------------------------------------- */
static void
stack_clear_n_oldest (GQueue * stack, guint n)
{
  guint i;
  CajaUndoStackActionData *action = NULL;

  for (i = 0; i < n; i++) {
    if ((action = (CajaUndoStackActionData *) g_queue_pop_tail (stack)) == NULL)
        break;
    if (action->locked) {
      action->freed = TRUE;
    } else {
      free_undostack_action (action, NULL);
    }
  }
}

/** ---------------------------------------------------------------- */
static void
stack_fix_size (CajaUndoStackManagerPrivate * priv)
{
  guint length = g_queue_get_length (priv->stack);

  if (length > priv->undo_levels) {
    if (priv->index > (priv->undo_levels + 1)) {
      /* If the index will fall off the stack
       * move it back to the maximum position */
      priv->index = priv->undo_levels + 1;
    }
    stack_clear_n_oldest (priv->stack, length - (priv->undo_levels));
  }
}

/** ---------------------------------------------------------------- */
static void
clear_redo_actions (CajaUndoStackManagerPrivate * priv)
{
  while (priv->index > 0) {
    CajaUndoStackActionData *head = (CajaUndoStackActionData *)
        g_queue_pop_head (priv->stack);
    free_undostack_action (head, NULL);
    priv->index--;
  }
}

/** ---------------------------------------------------------------- */
static void
stack_push_action (CajaUndoStackManagerPrivate * priv,
    CajaUndoStackActionData * action)
{
  guint length;

  clear_redo_actions (priv);

  g_queue_push_head (priv->stack, (gpointer) action);
  length = g_queue_get_length (priv->stack);

  if (length > priv->undo_levels) {
    stack_fix_size (priv);
  }
}

/** ---------------------------------------------------------------- */
static gchar *
get_first_target_short_name (CajaUndoStackActionData * action)
{
  GList *targets_first;
  gchar *file_name;

  targets_first = g_list_first (action->destinations);
  file_name = (gchar *) g_strdup (targets_first->data);

  return file_name;
}

/** ---------------------------------------------------------------- */
static gchar *
get_undo_description (CajaUndoStackActionData * action)
{
  gchar *description = NULL;
  gchar *source = NULL;
  guint count;

  if (action != NULL) {
    if (action->undo_description == NULL) {
      if (action->src_dir) {
        source = g_file_get_path (action->src_dir);
      }
      count = action->count;
      switch (action->type) {
        case CAJA_UNDOSTACK_COPY:
          if (count != 1) {
            description = g_strdup_printf (_("Delete %d copied items"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Delete '%s'"), name);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_DUPLICATE:
          if (count != 1) {
            description =
                g_strdup_printf (_("Delete %d duplicated items"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Delete '%s'"), name);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_MOVE:
          if (count != 1) {
            description =
                g_strdup_printf (_
                ("Move %d items back to '%s'"), count, source);
          } else {
            gchar *name = get_first_target_short_name (action);
            description =
                g_strdup_printf (_("Move '%s' back to '%s'"), name, source);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_RENAME:
        {
          char *from_name = get_uri_basename (action->new_uri);
          char *to_name = get_uri_basename (action->old_uri);
          description =
              g_strdup_printf (_("Rename '%s' as '%s'"), from_name, to_name);
          g_free (from_name);
          g_free (to_name);
        }
          break;
        case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
        case CAJA_UNDOSTACK_CREATEEMPTYFILE:
        case CAJA_UNDOSTACK_CREATEFOLDER:
        {
          char *name = get_uri_basename (action->target_uri);
          description = g_strdup_printf (_("Delete '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_MOVETOTRASH:
        {
          count = g_hash_table_size (action->trashed);
          if (count != 1) {
            description =
                g_strdup_printf (_("Restore %d items from trash"), count);
          } else {
            GList *keys = g_hash_table_get_keys (action->trashed);
            GList *first = g_list_first (keys);
            char *item = (char *) first->data;
            char *name = get_uri_basename (item);
            char *orig_path = get_uri_parent_path (item);
            description =
                g_strdup_printf (_("Restore '%s' to '%s'"), name, orig_path);
            g_free (name);
            g_free (orig_path);
            g_list_free (keys);
          }
        }
          break;
        case CAJA_UNDOSTACK_RESTOREFROMTRASH:
        {
          if (count != 1) {
            description =
                g_strdup_printf (_("Move %d items back to trash"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Move '%s' back to trash"), name);
            g_free (name);
          }
        }
          break;
        case CAJA_UNDOSTACK_CREATELINK:
        {
          if (count != 1) {
            description =
                g_strdup_printf (_("Delete links to %d items"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Delete link to '%s'"), name);
            g_free (name);
          }
        }
          break;
        case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
        {
          char *name = g_file_get_path (action->dest_dir);
          description =
              g_strdup_printf (_
              ("Restore original permissions of items enclosed in '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_SETPERMISSIONS:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_("Restore original permissions of '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CHANGEGROUP:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_
              ("Restore group of '%s' to '%s'"),
              name, action->original_group_name_or_id);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CHANGEOWNER:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_
              ("Restore owner of '%s' to '%s'"),
              name, action->original_user_name_or_id);
          g_free (name);
        }
          break;
        default:
          break;
      }
      if (source) {
        g_free (source);
      }
      action->undo_description = description;
    } else {
      return action->undo_description;
    }
  }

  return description;
}

/** ---------------------------------------------------------------- */
static gchar *
get_redo_description (CajaUndoStackActionData * action)
{
  gchar *description = NULL;
  gchar *destination = NULL;
  guint count;

  if (action != NULL) {
    if (action->redo_description == NULL) {
      if (action->dest_dir) {
        destination = g_file_get_path (action->dest_dir);
      }
      count = action->count;
      switch (action->type) {
        case CAJA_UNDOSTACK_COPY:
          if (count != 1) {
            description =
                g_strdup_printf (_
                ("Copy %d items to '%s'"), count, destination);
          } else {
            gchar *name = get_first_target_short_name (action);
            description =
                g_strdup_printf (_("Copy '%s' to '%s'"), name, destination);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_DUPLICATE:
          if (count != 1) {
            description =
                g_strdup_printf (_
                ("Duplicate of %d items in '%s'"), count, destination);
          } else {
            gchar *name = get_first_target_short_name (action);
            description =
                g_strdup_printf (_
                ("Duplicate '%s' in '%s'"), name, destination);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_MOVE:
          if (count != 1) {
            description =
                g_strdup_printf (_
                ("Move %d items to '%s'"), count, destination);
          } else {
            gchar *name = get_first_target_short_name (action);
            description =
                g_strdup_printf (_("Move '%s' to '%s'"), name, destination);
            g_free (name);
          }
          break;
        case CAJA_UNDOSTACK_RENAME:
        {
          char *from_name = get_uri_basename (action->old_uri);
          char *to_name = get_uri_basename (action->new_uri);
          description =
              g_strdup_printf (_("Rename '%s' as '%s'"), from_name, to_name);
          g_free (from_name);
          g_free (to_name);
        }
          break;
        case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_("Create new file '%s' from template "), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CREATEEMPTYFILE:
        {
          char *name = get_uri_basename (action->target_uri);
          description = g_strdup_printf (_("Create an empty file '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CREATEFOLDER:
        {
          char *name = get_uri_basename (action->target_uri);
          description = g_strdup_printf (_("Create a new folder '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_MOVETOTRASH:
        {
          count = g_hash_table_size (action->trashed);
          if (count != 1) {
            description = g_strdup_printf (_("Move %d items to trash"), count);
          } else {
            GList *keys = g_hash_table_get_keys (action->trashed);
            GList *first = g_list_first (keys);
            char *item = (char *) first->data;
            char *name = get_uri_basename (item);
            description = g_strdup_printf (_("Move '%s' to trash"), name);
            g_free (name);
            g_list_free (keys);
          }
        }
          break;
        case CAJA_UNDOSTACK_RESTOREFROMTRASH:
        {
          if (count != 1) {
            description =
                g_strdup_printf (_("Restore %d items from trash"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Restore '%s' from trash"), name);
            g_free (name);
          }
        }
          break;
        case CAJA_UNDOSTACK_CREATELINK:
        {
          if (count != 1) {
            description =
                g_strdup_printf (_("Create links to %d items"), count);
          } else {
            gchar *name = get_first_target_short_name (action);
            description = g_strdup_printf (_("Create link to '%s'"), name);
            g_free (name);
          }
        }
          break;
        case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
        {
          char *name = g_file_get_path (action->dest_dir);
          description =
              g_strdup_printf (_("Set permissions of items enclosed in '%s'"),
              name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_SETPERMISSIONS:
        {
          char *name = get_uri_basename (action->target_uri);
          description = g_strdup_printf (_("Set permissions of '%s'"), name);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CHANGEGROUP:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_
              ("Set group of '%s' to '%s'"),
              name, action->new_group_name_or_id);
          g_free (name);
        }
          break;
        case CAJA_UNDOSTACK_CHANGEOWNER:
        {
          char *name = get_uri_basename (action->target_uri);
          description =
              g_strdup_printf (_
              ("Set owner of '%s' to '%s'"), name, action->new_user_name_or_id);
          g_free (name);
        }
          break;
        default:
          break;
      }
      if (destination) {
        g_free (destination);
      }
      action->redo_description = description;
    } else {
      return action->redo_description;
    }
  }

  return description;
}

/** ---------------------------------------------------------------- */
static gchar *
get_undo_label (CajaUndoStackActionData * action)
{
  gchar *label = NULL;
  guint count;

  if (action != NULL) {
    if (action->undo_label == NULL) {
      count = action->count;
      switch (action->type) {
        case CAJA_UNDOSTACK_COPY:
          label = g_strdup_printf (ngettext
              ("_Undo copy of %d item",
                  "_Undo copy of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_DUPLICATE:
          label = g_strdup_printf (ngettext
              ("_Undo duplicate of %d item",
                  "_Undo duplicate of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_MOVE:
          label = g_strdup_printf (ngettext
              ("_Undo move of %d item",
                  "_Undo move of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RENAME:
          label = g_strdup_printf (ngettext
              ("_Undo rename of %d item",
                  "_Undo rename of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CREATEEMPTYFILE:
          label = g_strdup_printf (_("_Undo creation of an empty file"));
          break;
        case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
          label = g_strdup_printf (_("_Undo creation of a file from template"));
          break;
        case CAJA_UNDOSTACK_CREATEFOLDER:
          label = g_strdup_printf (ngettext
              ("_Undo creation of %d folder",
                  "_Undo creation of %d folders", count), count);
          break;
        case CAJA_UNDOSTACK_MOVETOTRASH:
          label = g_strdup_printf (ngettext
              ("_Undo move to trash of %d item",
                  "_Undo move to trash of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RESTOREFROMTRASH:
          label = g_strdup_printf (ngettext
              ("_Undo restore from trash of %d item",
                  "_Undo restore from trash of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CREATELINK:
          label = g_strdup_printf (ngettext
              ("_Undo create link to %d item",
                  "_Undo create link to %d items", count), count);
          break;
        case CAJA_UNDOSTACK_DELETE:
          label = g_strdup_printf (ngettext
              ("_Undo delete of %d item",
                  "_Undo delete of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
          label = g_strdup_printf (ngettext
              ("Undo recursive change permissions of %d item",
                  "Undo recursive change permissions of %d items",
                  count), count);
          break;
        case CAJA_UNDOSTACK_SETPERMISSIONS:
          label = g_strdup_printf (ngettext
              ("Undo change permissions of %d item",
                  "Undo change permissions of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CHANGEGROUP:
          label = g_strdup_printf (ngettext
              ("Undo change group of %d item",
                  "Undo change group of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CHANGEOWNER:
          label = g_strdup_printf (ngettext
              ("Undo change owner of %d item",
                  "Undo change owner of %d items", count), count);
          break;
        default:
          break;
      }
      action->undo_label = label;
    } else {
      return action->undo_label;
    }
  }

  return label;
}

/** ---------------------------------------------------------------- */
static gchar *
get_redo_label (CajaUndoStackActionData * action)
{
  gchar *label = NULL;
  guint count;

  if (action != NULL) {
    if (action->redo_label == NULL) {
      count = action->count;
      switch (action->type) {
        case CAJA_UNDOSTACK_COPY:
          label = g_strdup_printf (ngettext
              ("_Redo copy of %d item",
                  "_Redo copy of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_DUPLICATE:
          label = g_strdup_printf (ngettext
              ("_Redo duplicate of %d item",
                  "_Redo duplicate of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_MOVE:
          label = g_strdup_printf (ngettext
              ("_Redo move of %d item",
                  "_Redo move of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RENAME:
          label = g_strdup_printf (ngettext
              ("_Redo rename of %d item",
                  "_Redo rename of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CREATEEMPTYFILE:
          label = g_strdup_printf (_("_Redo creation of an empty file"));
          break;
        case CAJA_UNDOSTACK_CREATEFILEFROMTEMPLATE:
          label = g_strdup_printf (_("_Redo creation of a file from template"));
          break;
        case CAJA_UNDOSTACK_CREATEFOLDER:
          label = g_strdup_printf (ngettext
              ("_Redo creation of %d folder",
                  "_Redo creation of %d folders", count), count);
          break;
        case CAJA_UNDOSTACK_MOVETOTRASH:
          label = g_strdup_printf (ngettext
              ("_Redo move to trash of %d item",
                  "_Redo move to trash of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RESTOREFROMTRASH:
          label = g_strdup_printf (ngettext
              ("_Redo restore from trash of %d item",
                  "_Redo restore from trash of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CREATELINK:
          label = g_strdup_printf (ngettext
              ("_Redo create link to %d item",
                  "_Redo create link to %d items", count), count);
          break;
        case CAJA_UNDOSTACK_DELETE:
          label = g_strdup_printf (ngettext
              ("_Redo delete of %d item",
                  "_Redo delete of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_RECURSIVESETPERMISSIONS:
          label = g_strdup_printf (ngettext
              ("Redo recursive change permissions of %d item",
                  "Redo recursive change permissions of %d items",
                  count), count);
          break;
        case CAJA_UNDOSTACK_SETPERMISSIONS:
          label = g_strdup_printf (ngettext
              ("Redo change permissions of %d item",
                  "Redo change permissions of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CHANGEGROUP:
          label = g_strdup_printf (ngettext
              ("Redo change group of %d item",
                  "Redo change group of %d items", count), count);
          break;
        case CAJA_UNDOSTACK_CHANGEOWNER:
          label = g_strdup_printf (ngettext
              ("Redo change owner of %d item",
                  "Redo change owner of %d items", count), count);
          break;
        default:
          break;
      }
      action->redo_label = label;
    } else {
      return action->redo_label;
    }
  }

  return label;
}

/** ---------------------------------------------------------------- */
static void
undo_redo_done_transfer_callback (GHashTable * debuting_uris, gpointer data)
{
  CajaUndoStackActionData *action;

  action = (CajaUndoStackActionData *) data;

  /* If the action needed to be freed but was locked, free now */
  if (action->freed) {
    free_undostack_action (action, NULL);
  } else {
    action->locked = FALSE;
  }

  CajaUndoStackManager *manager = action->manager;
  manager->priv->undo_redo_flag = FALSE;

  /* Update menus */
  do_menu_update (action->manager);
}

/** ---------------------------------------------------------------- */
static void
undo_redo_done_delete_callback (GHashTable *
    debuting_uris, gboolean user_cancel, gpointer callback_data)
{
  undo_redo_done_transfer_callback (debuting_uris, callback_data);
}

/** ---------------------------------------------------------------- */
static void
undo_redo_done_create_callback (GFile * new_file, gpointer callback_data)
{
  undo_redo_done_transfer_callback (NULL, callback_data);
}

/** ---------------------------------------------------------------- */
static void
undo_redo_op_callback (gpointer callback_data)
{
  undo_redo_done_transfer_callback (NULL, callback_data);
}

/** ---------------------------------------------------------------- */
static void
undo_redo_done_rename_callback (CajaFile * file,
    GFile * result_location, GError * error, gpointer callback_data)
{
  undo_redo_done_transfer_callback (NULL, callback_data);
}

/** ---------------------------------------------------------------- */
static void
free_undostack_action (gpointer data, gpointer user_data)
{
  CajaUndoStackActionData *action = (CajaUndoStackActionData *) data;

  if (!action)
    return;

  g_free (action->template);
  g_free (action->target_uri);
  g_free (action->old_uri);
  g_free (action->new_uri);

  g_free (action->undo_label);
  g_free (action->undo_description);
  g_free (action->redo_label);
  g_free (action->redo_description);

  g_free (action->original_group_name_or_id);
  g_free (action->original_user_name_or_id);
  g_free (action->new_group_name_or_id);
  g_free (action->new_user_name_or_id);

  if (action->sources) {
    g_list_free_full (action->sources, g_free);
  }
  if (action->destinations) {
    g_list_free_full (action->destinations, g_free);
  }

  if (action->trashed) {
    g_hash_table_destroy (action->trashed);
  }

  if (action->original_permissions) {
    g_hash_table_destroy (action->original_permissions);
  }

  if (action->src_dir)
    g_object_unref (action->src_dir);
  if (action->dest_dir)
    g_object_unref (action->dest_dir);

  if (action)
    g_slice_free (CajaUndoStackActionData, action);
}

/** ---------------------------------------------------------------- */
static void
undostack_dispose_all (GQueue * queue)
{
  g_queue_foreach (queue, free_undostack_action, NULL);
}

/** ---------------------------------------------------------------- */
static gboolean
can_undo (CajaUndoStackManagerPrivate * priv)
{
  return (get_next_undo_action (priv) != NULL);
}

/** ---------------------------------------------------------------- */
static gboolean
can_redo (CajaUndoStackManagerPrivate * priv)
{
  return (get_next_redo_action (priv) != NULL);
}

/** ---------------------------------------------------------------- */
static CajaUndoStackActionData *
get_next_redo_action (CajaUndoStackManagerPrivate * priv)
{
  if (g_queue_is_empty (priv->stack)) {
    return NULL;
  }

  if (priv->index == 0) {
    /* ... no redo actions */
    return NULL;
  }

  CajaUndoStackActionData *action = g_queue_peek_nth (priv->stack,
      priv->index - 1);

  if (action->locked) {
    return NULL;
  } else {
    return action;
  }
}

/** ---------------------------------------------------------------- */
static CajaUndoStackActionData *
get_next_undo_action (CajaUndoStackManagerPrivate * priv)
{
  if (g_queue_is_empty (priv->stack)) {
    return NULL;
  }

  guint stack_size = g_queue_get_length (priv->stack);

  if (priv->index == stack_size) {
    return NULL;
  }

  CajaUndoStackActionData *action = g_queue_peek_nth (priv->stack,
      priv->index);

  if (action->locked) {
    return NULL;
  } else {
    return action;
  }
}

/** ---------------------------------------------------------------- */
static void
do_menu_update (CajaUndoStackManager * manager)
{

  if (!manager)
    return;

  CajaUndoStackActionData *action;
  CajaUndoStackManagerPrivate *priv = manager->priv;
  CajaUndoStackMenuData *data = g_slice_new0 (CajaUndoStackMenuData);

  g_mutex_lock (&priv->mutex);

  action = get_next_undo_action (priv);
  data->undo_label = get_undo_label (action);
  data->undo_description = get_undo_description (action);

  action = get_next_redo_action (priv);

  data->redo_label = get_redo_label (action);
  data->redo_description = get_redo_description (action);

  g_mutex_unlock (&priv->mutex);

  /* Update menus */
  g_signal_emit_by_name (manager, "request-menu-update", data);

  /* Free the signal data */
  // Note: we do not own labels and descriptions, they are part of the action.
  g_slice_free (CajaUndoStackMenuData, data);
}

/** ---------------------------------------------------------------- */
static GList *
construct_gfile_list (const GList * urilist, GFile * parent)
{
  const GList *l;
  GList *file_list = NULL;
  GFile *file = NULL;

  for (l = urilist; l != NULL; l = l->next) {
    file = g_file_get_child (parent, l->data);
    file_list = g_list_append (file_list, file);
  }

  return file_list;
}

/** ---------------------------------------------------------------- */
static GList *
construct_gfile_list_from_uri (char *uri)
{
  GList *file_list = NULL;
  GFile *file;

  file = g_file_new_for_uri (uri);
  file_list = g_list_append (file_list, file);

  return file_list;
}

/** ---------------------------------------------------------------- */
static GList *
uri_list_to_gfile_list (GList * urilist)
{
  const GList *l;
  GList *file_list = NULL;
  GFile *file = NULL;

  for (l = urilist; l != NULL; l = l->next) {
    file = g_file_new_for_uri (l->data);
    file_list = g_list_append (file_list, file);
  }

  return file_list;
}

/** ---------------------------------------------------------------- */
static char *
get_uri_basename (char *uri)
{
  GFile *f = g_file_new_for_uri (uri);
  char *basename = g_file_get_basename (f);
  g_object_unref (f);
  return basename;
}

/** ---------------------------------------------------------------- */
static char *
get_uri_parent (char *uri)
{
  GFile *f = g_file_new_for_uri (uri);
  GFile *p = g_file_get_parent (f);
  char *parent = g_file_get_uri (p);
  g_object_unref (f);
  g_object_unref (p);
  return parent;
}

/** ---------------------------------------------------------------- */
static char *
get_uri_parent_path (char *uri)
{
  GFile *f = g_file_new_for_uri (uri);
  GFile *p = g_file_get_parent (f);
  char *parent = g_file_get_path (p);
  g_object_unref (f);
  g_object_unref (p);
  return parent;
}

/** ---------------------------------------------------------------- */
static GHashTable *
retrieve_files_to_restore (GHashTable * trashed)
{
  if ((!(g_hash_table_size (trashed))) > 0) {
    return NULL;
  }

  GFile *trash = g_file_new_for_uri ("trash:");

  GFileEnumerator *enumerator = g_file_enumerate_children (trash,
      G_FILE_ATTRIBUTE_STANDARD_NAME
      ","
      G_FILE_ATTRIBUTE_TIME_MODIFIED
      ","
      G_FILE_ATTRIBUTE_TRASH_ORIG_PATH,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, FALSE, NULL);

  GHashTable *to_restore = g_hash_table_new_full (g_direct_hash,
      g_direct_equal, g_object_unref, g_free);

  if (enumerator) {
    GFileInfo *info;
    while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
      /* Retrieve the original file uri */
      const char *origpath = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH);
      if (origpath == NULL) {
        g_warning ("The item cannot be restored from trash: could not determine original location");
        continue;
      }

      GFile *origfile = g_file_new_for_path (origpath);
      char *origuri = g_file_get_uri (origfile);
      g_object_unref (origfile);

      gboolean origuri_inserted = FALSE;
      gpointer lookupvalue = g_hash_table_lookup (trashed, origuri);

      if (lookupvalue) {
        guint64 *mtime = (guint64 *) lookupvalue;
        guint64 mtime_item = g_file_info_get_attribute_uint64(info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        if (*mtime == mtime_item) {
          GFile *item = g_file_get_child (trash, g_file_info_get_name (info)); /* File in the trash */
          g_hash_table_insert (to_restore, item, origuri);
          origuri_inserted = TRUE;
        }
      }

      if (!origuri_inserted) {
        g_free (origuri);
      }
    }

    g_file_enumerator_close (enumerator, FALSE, NULL);
    g_object_unref (enumerator);
  }

  g_object_unref (trash);

  return to_restore;
}

/** ---------------------------------------------------------------- */
