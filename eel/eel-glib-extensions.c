/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-glib-extensions.c - implementation of new functions that conceptually
                                belong in glib. Perhaps some of these will be
                                actually rolled into glib someday.

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "eel-glib-extensions.h"

#include "eel-debug.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <glib-object.h>
#include <math.h>
#include <stdlib.h>

typedef struct
{
    GHashTable *hash_table;
    char *display_name;
    gboolean keys_known_to_be_strings;
} HashTableToFree;

static GList *hash_tables_to_free_at_exit;

/**
 * eel_g_list_exactly_one_item
 *
 * Like g_list_length (list) == 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has exactly one item.
 **/
gboolean
eel_g_list_exactly_one_item (GList *list)
{
    return list != NULL && list->next == NULL;
}

/**
 * eel_g_list_more_than_one_item
 *
 * Like g_list_length (list) > 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has more than one item.
 **/
gboolean
eel_g_list_more_than_one_item (GList *list)
{
    return list != NULL && list->next != NULL;
}

/**
 * eel_g_list_equal
 *
 * Compares two lists to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists are the same length with the same elements.
 **/
gboolean
eel_g_list_equal (GList *list_a, GList *list_b)
{
    GList *p, *q;

    for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next)
    {
        if (p->data != q->data)
        {
            return FALSE;
        }
    }
    return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_equal
 *
 * Compares two lists of C strings to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists contain the same strings.
 **/
gboolean
eel_g_str_list_equal (GList *list_a, GList *list_b)
{
    GList *p, *q;

    for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next)
    {
        if (eel_strcmp (p->data, q->data) != 0)
        {
            return FALSE;
        }
    }
    return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_alphabetize
 *
 * Sort a list of strings using locale-sensitive rules.
 *
 * @list: List of strings and/or NULLs.
 *
 * Return value: @list, sorted.
 **/
GList *
eel_g_str_list_alphabetize (GList *list)
{
    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

int
eel_g_str_list_index (GList *str_list,
                      const char *str)
{
    int i;
    GList *l;
    for (i = 0, l = str_list; l != NULL; l = l->next, i++)
    {
        if (!strcmp (str, (const char*)l->data))
        {
            return i;
        }
    }
    return -1;
}

/**
 * eel_g_list_free_deep
 *
 * Frees the elements of a list and then the list.
 * @list: List of elements that can be freed with g_free.
 **/
void
eel_g_list_free_deep (GList *list)
{
	g_list_free_full (list, (GDestroyNotify) g_free);
}

/**
 * eel_g_strv_find
 *
 * Get index of string in array of strings.
 *
 * @strv: NULL-terminated array of strings.
 * @find_me: string to search for.
 *
 * Return value: index of array entry in @strv that
 * matches @find_me, or -1 if no matching entry.
 */
int
eel_g_strv_find (char **strv, const char *find_me)
{
    int index;

    g_return_val_if_fail (find_me != NULL, -1);

    for (index = 0; strv[index] != NULL; ++index)
    {
        if (strcmp (strv[index], find_me) == 0)
        {
            return index;
        }
    }

    return -1;
}

gboolean
eel_g_strv_equal (char **a, char **b)
{
    int i;

    if (g_strv_length (a) != g_strv_length (b))
    {
        return FALSE;
    }

    for (i = 0; a[i] != NULL; i++)
    {
        if (strcmp (a[i], b[i]) != 0)
        {
            return FALSE;
        }
    }
    return TRUE;
}

static int
compare_pointers (gconstpointer pointer_1, gconstpointer pointer_2)
{
    if ((const char *) pointer_1 < (const char *) pointer_2)
    {
        return -1;
    }
    if ((const char *) pointer_1 > (const char *) pointer_2)
    {
        return +1;
    }
    return 0;
}

gboolean
eel_g_lists_sort_and_check_for_intersection (GList **list_1,
        GList **list_2)

{
    GList *node_1, *node_2;

    *list_1 = g_list_sort (*list_1, compare_pointers);
    *list_2 = g_list_sort (*list_2, compare_pointers);

    node_1 = *list_1;
    node_2 = *list_2;

    while (node_1 != NULL && node_2 != NULL)
    {
        int compare_result;

        compare_result = compare_pointers (node_1->data, node_2->data);
        if (compare_result == 0)
        {
            return TRUE;
        }
        if (compare_result <= 0)
        {
            node_1 = node_1->next;
        }
        if (compare_result >= 0)
        {
            node_2 = node_2->next;
        }
    }

    return FALSE;
}


/**
 * eel_g_list_partition
 *
 * Parition a list into two parts depending on whether the data
 * elements satisfy a provided predicate. Order is preserved in both
 * of the resulting lists, and the original list is consumed. A list
 * of the items that satisfy the predicate is returned, and the list
 * of items not satisfying the predicate is returned via the failed
 * out argument.
 *
 * @list: List to partition.
 * @predicate: Function to call on each element.
 * @user_data: Data to pass to function.
 * @failed: The GList * variable pointed to by this argument will be
 * set to the list of elements for which the predicate returned
 * false. */

GList *
eel_g_list_partition (GList *list,
                      EelPredicateFunction  predicate,
                      gpointer user_data,
                      GList **failed)
{
    GList *predicate_true;
    GList *predicate_false;
    GList *reverse;
    GList *p;
    GList *next;

    predicate_true = NULL;
    predicate_false = NULL;

    reverse = g_list_reverse (list);

    for (p = reverse; p != NULL; p = next)
    {
        next = p->next;

        if (next != NULL)
        {
            next->prev = NULL;
        }

        if (predicate (p->data, user_data))
        {
            p->next = predicate_true;
            if (predicate_true != NULL)
            {
                predicate_true->prev = p;
            }
            predicate_true = p;
        }
        else
        {
            p->next = predicate_false;
            if (predicate_false != NULL)
            {
                predicate_false->prev = p;
            }
            predicate_false = p;
        }
    }

    *failed = predicate_false;
    return predicate_true;
}

static void
print_key_string (gpointer key, gpointer value, gpointer callback_data)
{
    g_assert (callback_data == NULL);

    g_print ("--> %s\n", (char *) key);
}

static void
free_hash_tables_at_exit (void)
{
    GList *p;
    guint size;
    HashTableToFree *hash_table_to_free = NULL;

    for (p = hash_tables_to_free_at_exit; p != NULL; p = p->next)
    {
        hash_table_to_free = p->data;

        size = g_hash_table_size (hash_table_to_free->hash_table);
        if (size != 0)
        {
            if (hash_table_to_free->keys_known_to_be_strings)
            {
                g_print ("\n--- Hash table keys for warning below:\n");
                g_hash_table_foreach (hash_table_to_free->hash_table,
                                      print_key_string,
                                      NULL);
            }
            g_warning ("\"%s\" hash table still has %u element%s at quit time%s",
                       hash_table_to_free->display_name, size,
                       size == 1 ? "" : "s",
                       hash_table_to_free->keys_known_to_be_strings
                       ? " (keys above)" : "");
        }

        g_hash_table_destroy (hash_table_to_free->hash_table);
        g_free (hash_table_to_free->display_name);
        g_free (hash_table_to_free);
    }
    g_list_free (hash_tables_to_free_at_exit);
    hash_tables_to_free_at_exit = NULL;
}

GHashTable *
eel_g_hash_table_new_free_at_exit (GHashFunc hash_func,
                                   GCompareFunc key_compare_func,
                                   const char *display_name)
{
    GHashTable *hash_table;
    HashTableToFree *hash_table_to_free;

    /* FIXME: We can take out the CAJA_DEBUG check once we
     * have fixed more of the leaks. For now, it's a bit too noisy
     * for the general public.
     */
    if (hash_tables_to_free_at_exit == NULL)
    {
        eel_debug_call_at_shutdown (free_hash_tables_at_exit);
    }

    hash_table = g_hash_table_new (hash_func, key_compare_func);

    hash_table_to_free = g_new (HashTableToFree, 1);
    hash_table_to_free->hash_table = hash_table;
    hash_table_to_free->display_name = g_strdup (display_name);
    hash_table_to_free->keys_known_to_be_strings =
        hash_func == g_str_hash;

    hash_tables_to_free_at_exit = g_list_prepend
                                  (hash_tables_to_free_at_exit, hash_table_to_free);

    return hash_table;
}

typedef struct
{
    GList *keys;
    GList *values;
} FlattenedHashTable;

static void
flatten_hash_table_element (gpointer key, gpointer value, gpointer callback_data)
{
    FlattenedHashTable *flattened_table;

    flattened_table = callback_data;
    flattened_table->keys = g_list_prepend
                            (flattened_table->keys, key);
    flattened_table->values = g_list_prepend
                              (flattened_table->values, value);
}

void
eel_g_hash_table_safe_for_each (GHashTable *hash_table,
                                GHFunc callback,
                                gpointer callback_data)
{
    FlattenedHashTable flattened;
    GList *p, *q;

    flattened.keys = NULL;
    flattened.values = NULL;

    g_hash_table_foreach (hash_table,
                          flatten_hash_table_element,
                          &flattened);

    for (p = flattened.keys, q = flattened.values;
            p != NULL;
            p = p->next, q = q->next)
    {
        (* callback) (p->data, q->data, callback_data);
    }

    g_list_free (flattened.keys);
    g_list_free (flattened.values);
}

int
eel_round (double d)
{
    double val;

    val = floor (d + .5);

    /* The tests are needed because the result of floating-point to integral
     * conversion is undefined if the floating point value is not representable
     * in the new type. E.g. the magnititude is too large or a negative
     * floating-point value being converted to an unsigned.
     */
    g_return_val_if_fail (val <= INT_MAX, INT_MAX);
    g_return_val_if_fail (val >= INT_MIN, INT_MIN);

    return val;
}

/**
 * eel_add_weak_pointer
 *
 * Nulls out a saved reference to an object when the object gets destroyed.
 *
 * @pointer_location: Address of the saved pointer.
 **/
void
eel_add_weak_pointer (gpointer pointer_location)
{
    gpointer *object_location;

    g_return_if_fail (pointer_location != NULL);

    object_location = (gpointer *) pointer_location;
    if (*object_location == NULL)
    {
        /* The reference is NULL, nothing to do. */
        return;
    }

    g_return_if_fail (G_IS_OBJECT (*object_location));

    g_object_add_weak_pointer (G_OBJECT (*object_location),
                               object_location);
}

/**
 * eel_remove_weak_pointer
 *
 * Removes the weak pointer that was added by eel_add_weak_pointer.
 * Also nulls out the pointer.
 *
 * @pointer_location: Pointer that was passed to eel_add_weak_pointer.
 **/
void
eel_remove_weak_pointer (gpointer pointer_location)
{
    gpointer *object_location;

    g_return_if_fail (pointer_location != NULL);

    object_location = (gpointer *) pointer_location;
    if (*object_location == NULL)
    {
        /* The object was already destroyed and the reference
         * nulled out, nothing to do.
         */
        return;
    }

    g_return_if_fail (G_IS_OBJECT (*object_location));

    g_object_remove_weak_pointer (G_OBJECT (*object_location),
                                  object_location);

    *object_location = NULL;
}

static void
update_auto_boolean (GSettings   *settings,
                     const gchar *key,
                     gpointer     user_data)
{
    int *storage = user_data;

    *storage = g_settings_get_boolean (settings, key);
}

void
eel_g_settings_add_auto_boolean (GSettings *settings,
                 const char *key,
                 gboolean *storage)
{
    char *signal;

    *storage = g_settings_get_boolean (settings, key);
    signal = g_strconcat ("changed::", key, NULL);
    g_signal_connect (settings, signal,
              G_CALLBACK(update_auto_boolean),
              storage);

    g_free (signal);
}

static void
update_auto_int (GSettings   *settings,
                 const gchar *key,
                 gpointer     user_data)
{
    int *storage = user_data;

    *storage = g_settings_get_int (settings, key);
}

void
eel_g_settings_add_auto_int (GSettings *settings,
                             const char *key,
                             int *storage)
{
    char *signal;

    *storage = g_settings_get_int (settings, key);
    signal = g_strconcat ("changed::", key, NULL);
    g_signal_connect (settings, signal,
              G_CALLBACK(update_auto_int),
              storage);

    g_free (signal);
}

static void
update_auto_enum (GSettings   *settings,
                  const gchar *key,
                  gpointer     user_data)
{
    int *storage = user_data;

    *storage = g_settings_get_enum (settings, key);
}

void
eel_g_settings_add_auto_enum (GSettings *settings,
                              const char *key,
                              int *storage)
{
    char *signal;

    *storage = g_settings_get_enum (settings, key);
    signal = g_strconcat ("changed::", key, NULL);
    g_signal_connect (settings, signal,
                      G_CALLBACK(update_auto_enum),
                      storage);

    g_free (signal);
}

static void
update_auto_strv_as_quarks (GSettings   *settings,
                            const gchar *key,
                            gpointer     user_data)
{
    GQuark **storage = user_data;
    int i = 0;
    char **value;

    value = g_settings_get_strv (settings, key);

    g_free (*storage);
    *storage = g_new (GQuark, g_strv_length (value) + 1);

    for (i = 0; value[i] != NULL; ++i) {
        (*storage)[i] = g_quark_from_string (value[i]);
    }
    (*storage)[i] = 0;

    g_strfreev (value);
}

static void
update_auto_strv (GSettings   *settings,
          const gchar *key,
          gpointer     user_data)
{
    char ***storage = user_data;

    g_free (*storage);
    *storage = g_settings_get_strv (settings, key);
}

void
eel_g_settings_add_auto_strv (GSettings *settings,
                  const char *key,
                  char ***storage)
{
    char *signal;

    *storage = NULL;
    update_auto_strv (settings, key, storage);
    signal = g_strconcat ("changed::", key, NULL);
    g_signal_connect (settings, signal,
              G_CALLBACK(update_auto_strv),
              storage);

    g_free (signal);
}

void
eel_g_settings_add_auto_strv_as_quarks (GSettings *settings,
                                        const char *key,
                                        GQuark **storage)
{
    char *signal;

    *storage = NULL;
    update_auto_strv_as_quarks (settings, key, storage);
    signal = g_strconcat ("changed::", key, NULL);
    g_signal_connect (settings, signal,
              G_CALLBACK(update_auto_strv_as_quarks),
              storage);

    g_free (signal);
}

#if !defined (EEL_OMIT_SELF_CHECK)

static gboolean
eel_test_predicate (gpointer data,
                    gpointer callback_data)
{
    return g_ascii_strcasecmp (data, callback_data) <= 0;
}

void
eel_self_check_glib_extensions (void)
{
    char **strv;
    GList *compare_list_1;
    GList *compare_list_2;
    GList *compare_list_3;
    GList *compare_list_4;
    GList *compare_list_5;
    GList *list_to_partition;
    GList *expected_passed;
    GList *expected_failed;
    GList *actual_passed;
    GList *actual_failed;

    strv = g_strsplit ("zero|one|two|three|four", "|", 0);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "zero"), 0);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "one"), 1);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "four"), 4);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "five"), -1);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, ""), -1);
    EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "o"), -1);
    g_strfreev (strv);

    /* eel_g_str_list_equal */

    /* We g_strdup because identical string constants can be shared. */

    compare_list_1 = NULL;
    compare_list_1 = g_list_append (compare_list_1, g_strdup ("Apple"));
    compare_list_1 = g_list_append (compare_list_1, g_strdup ("zebra"));
    compare_list_1 = g_list_append (compare_list_1, g_strdup ("!@#!@$#@$!"));

    compare_list_2 = NULL;
    compare_list_2 = g_list_append (compare_list_2, g_strdup ("Apple"));
    compare_list_2 = g_list_append (compare_list_2, g_strdup ("zebra"));
    compare_list_2 = g_list_append (compare_list_2, g_strdup ("!@#!@$#@$!"));

    compare_list_3 = NULL;
    compare_list_3 = g_list_append (compare_list_3, g_strdup ("Apple"));
    compare_list_3 = g_list_append (compare_list_3, g_strdup ("zebra"));

    compare_list_4 = NULL;
    compare_list_4 = g_list_append (compare_list_4, g_strdup ("Apple"));
    compare_list_4 = g_list_append (compare_list_4, g_strdup ("zebra"));
    compare_list_4 = g_list_append (compare_list_4, g_strdup ("!@#!@$#@$!"));
    compare_list_4 = g_list_append (compare_list_4, g_strdup ("foobar"));

    compare_list_5 = NULL;
    compare_list_5 = g_list_append (compare_list_5, g_strdup ("Apple"));
    compare_list_5 = g_list_append (compare_list_5, g_strdup ("zzzzzebraaaaaa"));
    compare_list_5 = g_list_append (compare_list_5, g_strdup ("!@#!@$#@$!"));

    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_2), TRUE);
    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_3), FALSE);
    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_4), FALSE);
    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_5), FALSE);

    g_list_free_full (compare_list_1, g_free);
    g_list_free_full (compare_list_2, g_free);
    g_list_free_full (compare_list_3, g_free);
    g_list_free_full (compare_list_4, g_free);
    g_list_free_full (compare_list_5, g_free);

    /* eel_g_list_partition */

    list_to_partition = NULL;
    list_to_partition = g_list_append (list_to_partition, "Cadillac");
    list_to_partition = g_list_append (list_to_partition, "Pontiac");
    list_to_partition = g_list_append (list_to_partition, "Ford");
    list_to_partition = g_list_append (list_to_partition, "Range Rover");

    expected_passed = NULL;
    expected_passed = g_list_append (expected_passed, "Cadillac");
    expected_passed = g_list_append (expected_passed, "Ford");

    expected_failed = NULL;
    expected_failed = g_list_append (expected_failed, "Pontiac");
    expected_failed = g_list_append (expected_failed, "Range Rover");

    actual_passed = eel_g_list_partition (list_to_partition,
                                          eel_test_predicate,
                                          "m",
                                          &actual_failed);

    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_passed, actual_passed), TRUE);
    EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_failed, actual_failed), TRUE);

    /* Don't free "list_to_partition", since it is consumed
     * by eel_g_list_partition.
     */

    g_list_free (expected_passed);
    g_list_free (actual_passed);
    g_list_free (expected_failed);
    g_list_free (actual_failed);
}

#endif /* !EEL_OMIT_SELF_CHECK */
