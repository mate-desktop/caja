/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* CajaUndoTransaction - An object for an undoable transaction.
 *                           Used internally by undo machinery.
 *                           Not public.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <libcaja-private/caja-undo.h>
#include <libcaja-private/caja-undo-manager.h>
#include <libcaja-private/caja-undo-transaction.h>

#include "caja-undo-private.h"
#include <gtk/gtk.h>

#define CAJA_UNDO_TRANSACTION_LIST_DATA "Caja undo transaction list"

/* undo atoms */
static void undo_atom_list_free                  (GList                        *list);
static void undo_atom_list_undo_and_free         (GList                        *list);

G_DEFINE_TYPE (CajaUndoTransaction, caja_undo_transaction,
               G_TYPE_OBJECT);

#ifdef UIH
static Caja_Undo_MenuItem *
impl_Caja_Undo_Transaction__get_undo_menu_item (PortableServer_Servant servant,
        CORBA_Environment *ev)
{
    CajaUndoTransaction *transaction;
    Caja_Undo_MenuItem *item;

    transaction = CAJA_UNDO_TRANSACTION (matecomponent_object_from_servant (servant));

    item = Caja_Undo_MenuItem__alloc ();
    item->label = CORBA_string_dup (transaction->undo_menu_item_label);
    item->hint = CORBA_string_dup (transaction->undo_menu_item_hint);

    return item;
}

static Caja_Undo_MenuItem *
impl_Caja_Undo_Transaction__get_redo_menu_item (PortableServer_Servant servant,
        CORBA_Environment *ev)
{
    CajaUndoTransaction *transaction;
    Caja_Undo_MenuItem *item;

    transaction = CAJA_UNDO_TRANSACTION (matecomponent_object_from_servant (servant));

    item = Caja_Undo_MenuItem__alloc ();
    item->label = CORBA_string_dup (transaction->redo_menu_item_label);
    item->hint = CORBA_string_dup (transaction->redo_menu_item_hint);

    return item;
}

static CORBA_char *
impl_Caja_Undo_Transaction__get_operation_name (PortableServer_Servant servant,
        CORBA_Environment *ev)
{
    CajaUndoTransaction *transaction;

    transaction = CAJA_UNDO_TRANSACTION (matecomponent_object_from_servant (servant));
    return CORBA_string_dup (transaction->operation_name);
}
#endif


CajaUndoTransaction *
caja_undo_transaction_new (const char *operation_name,
                           const char *undo_menu_item_label,
                           const char *undo_menu_item_hint,
                           const char *redo_menu_item_label,
                           const char *redo_menu_item_hint)
{
    CajaUndoTransaction *transaction;

    transaction = CAJA_UNDO_TRANSACTION (g_object_new (caja_undo_transaction_get_type (), NULL));

    transaction->operation_name = g_strdup (operation_name);
    transaction->undo_menu_item_label = g_strdup (undo_menu_item_label);
    transaction->undo_menu_item_hint = g_strdup (undo_menu_item_hint);
    transaction->redo_menu_item_label = g_strdup (redo_menu_item_label);
    transaction->redo_menu_item_hint = g_strdup (redo_menu_item_hint);

    return transaction;
}

static void
caja_undo_transaction_init (CajaUndoTransaction *transaction)
{
}

static void
remove_transaction_from_object (gpointer list_data, gpointer callback_data)
{
    CajaUndoAtom *atom;
    CajaUndoTransaction *transaction;
    GList *list;

    g_assert (list_data != NULL);
    atom = list_data;
    transaction = CAJA_UNDO_TRANSACTION (callback_data);

    /* Remove the transaction from the list on the atom. */
    list = g_object_get_data (atom->target, CAJA_UNDO_TRANSACTION_LIST_DATA);

    if (list != NULL)
    {
        list = g_list_remove (list, transaction);
        g_object_set_data (atom->target, CAJA_UNDO_TRANSACTION_LIST_DATA, list);
    }
}

static void
remove_transaction_from_atom_targets (CajaUndoTransaction *transaction)
{

    g_list_foreach (transaction->atom_list,
                    remove_transaction_from_object,
                    transaction);
}

static void
caja_undo_transaction_finalize (GObject *object)
{
    CajaUndoTransaction *transaction;

    transaction = CAJA_UNDO_TRANSACTION (object);

    remove_transaction_from_atom_targets (transaction);
    undo_atom_list_free (transaction->atom_list);

    g_free (transaction->operation_name);
    g_free (transaction->undo_menu_item_label);
    g_free (transaction->undo_menu_item_hint);
    g_free (transaction->redo_menu_item_label);
    g_free (transaction->redo_menu_item_hint);

    if (transaction->owner != NULL)
    {
        g_object_unref (transaction->owner);
    }

    G_OBJECT_CLASS (caja_undo_transaction_parent_class)->finalize (object);
}

void
caja_undo_transaction_add_atom (CajaUndoTransaction *transaction,
                                const CajaUndoAtom *atom)
{
    GList *list;

    g_return_if_fail (CAJA_IS_UNDO_TRANSACTION (transaction));
    g_return_if_fail (atom != NULL);
    g_return_if_fail (GTK_IS_OBJECT (atom->target));

    /* Add the atom to the atom list in the transaction. */
    transaction->atom_list = g_list_append
                             (transaction->atom_list, g_memdup (atom, sizeof (*atom)));

    /* Add the transaction to the list on the atoms target object. */
    list = g_object_get_data (atom->target, CAJA_UNDO_TRANSACTION_LIST_DATA);
    if (g_list_find (list, transaction) != NULL)
    {
        return;
    }

    /* If it's not already on that atom, this object is new. */
    list = g_list_prepend (list, transaction);
    g_object_set_data (atom->target, CAJA_UNDO_TRANSACTION_LIST_DATA, list);

    /* Connect a signal handler to the atom so it will unregister
     * itself when it's destroyed.
     */
    g_signal_connect (atom->target, "destroy",
                      G_CALLBACK (caja_undo_transaction_unregister_object),
                      NULL);
}

void
caja_undo_transaction_undo (CajaUndoTransaction *transaction)
{
    g_return_if_fail (CAJA_IS_UNDO_TRANSACTION (transaction));

    remove_transaction_from_atom_targets (transaction);
    undo_atom_list_undo_and_free (transaction->atom_list);

    transaction->atom_list = NULL;
}

void
caja_undo_transaction_add_to_undo_manager (CajaUndoTransaction *transaction,
        CajaUndoManager *manager)
{
    g_return_if_fail (CAJA_IS_UNDO_TRANSACTION (transaction));
    g_return_if_fail (transaction->owner == NULL);

    if (manager != NULL)
    {
        caja_undo_manager_append (manager, transaction);
        transaction->owner = g_object_ref (manager);
    }
}

static void
remove_atoms (CajaUndoTransaction *transaction,
              GObject *object)
{
    GList *p, *next;
    CajaUndoAtom *atom;

    g_assert (CAJA_IS_UNDO_TRANSACTION (transaction));
    g_assert (G_IS_OBJECT (object));

    /* Destroy any atoms for this object. */
    for (p = transaction->atom_list; p != NULL; p = next)
    {
        atom = p->data;
        next = p->next;

        if (atom->target == object)
        {
            transaction->atom_list = g_list_remove_link
                                     (transaction->atom_list, p);
            undo_atom_list_free (p);
        }
    }

    /* If all the atoms are gone, forget this transaction.
     * This may end up freeing the transaction.
     */
    if (transaction->atom_list == NULL)
    {
        caja_undo_manager_forget (
            transaction->owner, transaction);
    }
}

static void
remove_atoms_cover (gpointer list_data, gpointer callback_data)
{
    if (CAJA_IS_UNDO_TRANSACTION (list_data))
    {
        remove_atoms (CAJA_UNDO_TRANSACTION (list_data), G_OBJECT (callback_data));
    }
}

void
caja_undo_transaction_unregister_object (GObject *object)
{
    GList *list;

    g_return_if_fail (G_IS_OBJECT (object));

    /* Remove atoms from each transaction on the list. */
    list = g_object_get_data (object, CAJA_UNDO_TRANSACTION_LIST_DATA);
    if (list != NULL)
    {
        g_list_foreach (list, remove_atoms_cover, object);
        g_list_free (list);
        g_object_set_data (object, CAJA_UNDO_TRANSACTION_LIST_DATA, NULL);
    }
}

static void
undo_atom_free (CajaUndoAtom *atom)
{
    /* Call the destroy-notify function if it's present. */
    if (atom->callback_data_destroy_notify != NULL)
    {
        (* atom->callback_data_destroy_notify) (atom->callback_data);
    }

    /* Free the atom storage. */
    g_free (atom);
}

static void
undo_atom_undo_and_free (CajaUndoAtom *atom)
{
    /* Call the function that does the actual undo. */
    (* atom->callback) (atom->target, atom->callback_data);

    /* Get rid of the atom now that it's spent. */
    undo_atom_free (atom);
}

static void
undo_atom_free_cover (gpointer atom, gpointer callback_data)
{
    g_assert (atom != NULL);
    g_assert (callback_data == NULL);
    undo_atom_free (atom);
}

static void
undo_atom_undo_and_free_cover (gpointer atom, gpointer callback_data)
{
    g_assert (atom != NULL);
    g_assert (callback_data == NULL);
    undo_atom_undo_and_free (atom);
}

static void
undo_atom_list_free (GList *list)
{
    g_list_foreach (list, undo_atom_free_cover, NULL);
    g_list_free (list);
}

static void
undo_atom_list_undo_and_free (GList *list)
{
    g_list_foreach (list, undo_atom_undo_and_free_cover, NULL);
    g_list_free (list);
}

static void
caja_undo_transaction_class_init (CajaUndoTransactionClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = caja_undo_transaction_finalize;
}
