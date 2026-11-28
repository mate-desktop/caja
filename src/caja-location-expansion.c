/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "caja-location-expansion.h"
#include <string.h>
#include <unistd.h>

typedef struct
{
    char *text;
    char *suffix;
    char *marker;
    GSubprocess *process;
    GCancellable *cancellable;
    guint timeout_id;
    gulong cancel_id;
} Expansion;

/* Expand only a leading variable, never shell expressions or URI contents. */
static char *
variable_name (const char *text, const char **suffix)
{
    const char *start, *end;
    gboolean braced;

    if (text[0] != '$')
        return NULL;
    braced = text[1] == '{';
    start = text + (braced ? 2 : 1);
    if (!g_ascii_isalpha (*start) && *start != '_')
        return NULL;
    for (end = start + 1; g_ascii_isalnum (*end) || *end == '_'; end++)
        ;
    if (braced && *end != '}')
        return NULL;
    *suffix = end + (braced ? 1 : 0);
    if (**suffix != '\0' && **suffix != '/')
        return NULL;
    return g_strndup (start, end - start);
}

gboolean
caja_location_has_variable (const char *text)
{
    const char *suffix;
    char *name = variable_name (text, &suffix);
    gboolean result = name != NULL;
    g_free (name);
    return result;
}

char *
caja_location_expand_environment (const char *text)
{
    const char *suffix, *value;
    char *name, *result = NULL;

    name = variable_name (text, &suffix);
    if (name != NULL)
    {
        value = g_getenv (name);
        if (value != NULL && *value != '\0' && g_utf8_validate (value, -1, NULL))
            result = g_strconcat (value, suffix, NULL);
    }
    g_free (name);
    return result;
}

static void
expansion_free (gpointer data)
{
    Expansion *expansion = data;
    g_clear_object (&expansion->process);
    g_clear_object (&expansion->cancellable);
    g_free (expansion->text);
    g_free (expansion->suffix);
    g_free (expansion->marker);
    g_free (expansion);
}

/* Interactive Bash must not share Caja's controlling terminal: startup
 * commands and job control could otherwise suspend the entire GUI job. */
static void
detach_lookup_terminal (gpointer data)
{
    if (setsid () == -1)
        _exit (127);
}

static gboolean
lookup_timeout (gpointer data)
{
    Expansion *expansion = data;
    expansion->timeout_id = 0;
    g_cancellable_cancel (expansion->cancellable);
    return G_SOURCE_REMOVE;
}

static void
lookup_cancelled (GCancellable *cancellable, gpointer data)
{
    Expansion *expansion = data;
    g_subprocess_force_exit (expansion->process);
}

static void
lookup_ready (GObject *source, GAsyncResult *result, gpointer data)
{
    GTask *task = data;
    Expansion *expansion = g_task_get_task_data (task);
    GCancellable *cancellable = g_task_get_cancellable (task);
    GBytes *output = NULL;
    char *path = NULL;

    if (expansion->timeout_id != 0)
        g_source_remove (expansion->timeout_id);
    if (expansion->cancel_id != 0)
        g_cancellable_disconnect (cancellable, expansion->cancel_id);

    if (g_subprocess_communicate_finish (G_SUBPROCESS (source), result,
                                       &output, NULL, NULL))
    {
        const char *bytes, *begin, *end;
        gsize size;

        /* A per-request marker separates the value from startup output,
         * including startup output that itself contains NUL bytes. */
        bytes = g_bytes_get_data (output, &size);
        begin = NULL;
        for (gsize i = 0; i + strlen (expansion->marker) < size; i++)
        {
            if (memcmp (bytes + i, expansion->marker,
                        strlen (expansion->marker) + 1) == 0)
            {
                begin = bytes + i + strlen (expansion->marker) + 1;
                break;
            }
        }
        if (begin != NULL)
        {
            end = memchr (begin, '\0', size - (begin - bytes));
            if (end != NULL && end != begin)
            {
                char *value = g_strndup (begin, end - begin);
                path = g_strconcat (value, expansion->suffix, NULL);
                g_free (value);
            }
        }
    }
    g_clear_pointer (&output, g_bytes_unref);
    if (path != NULL && !g_utf8_validate (path, -1, NULL))
        g_clear_pointer (&path, g_free);
    if (path == NULL)
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                 "%s", expansion->text);
    else
        g_task_return_pointer (task, path, g_free);
    g_object_unref (task);
}

void
caja_location_expand_async (const char *text,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
    GTask *task;
    Expansion *expansion;
    GSubprocessLauncher *launcher;
    const char *suffix;
    char *name, *path;

    if (cancellable != NULL)
        g_object_ref (cancellable);
    else
        cancellable = g_cancellable_new ();
    task = g_task_new (NULL, cancellable, callback, user_data);
    g_object_unref (cancellable);
    path = caja_location_expand_environment (text);
    name = variable_name (text, &suffix);
    if (path != NULL || name == NULL)
    {
        g_task_return_pointer (task, path != NULL ? path : g_strdup (text),
                               g_free);
        g_free (name);
        g_object_unref (task);
        return;
    }

    expansion = g_new0 (Expansion, 1);
    expansion->cancellable = g_object_ref (cancellable);
    expansion->text = g_strdup (text);
    expansion->suffix = g_strdup (suffix);
    expansion->marker = g_uuid_string_random ();
    g_task_set_task_data (task, expansion, expansion_free);
    /* Bash reads .bashrc just as an interactive terminal does. The variable
     * name is an argument to fixed code, not interpolated shell source.
     * Indirect expansion does not evaluate the contents of the variable. */
    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    g_subprocess_launcher_set_child_setup (launcher, detach_lookup_terminal,
                                           NULL, NULL);
    expansion->process = g_subprocess_launcher_spawn (launcher, NULL,
                                           "bash", "--noprofile", "-ic",
                                           "builtin printf '%s\\0%s\\0' \"$2\" \"${!1}\"",
                                           "caja-location", name,
                                           expansion->marker, NULL);
    g_object_unref (launcher);
    g_free (name);
    if (expansion->process == NULL)
    {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                                 "%s", text);
        g_object_unref (task);
        return;
    }
    expansion->timeout_id = g_timeout_add_seconds (3, lookup_timeout, expansion);
    expansion->cancel_id = g_cancellable_connect (cancellable,
                           G_CALLBACK (lookup_cancelled), expansion, NULL);
    g_subprocess_communicate_async (expansion->process, NULL, cancellable,
                                   lookup_ready, task);
}

char *
caja_location_expand_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}
