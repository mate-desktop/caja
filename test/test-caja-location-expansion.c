/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "src/caja-location-expansion.h"
#include <glib/gstdio.h>
#include <unistd.h>

static char *home;
static GMainLoop *loop;
static char *expanded;
static GError *lookup_error;

static void
ready (GObject *source, GAsyncResult *result, gpointer data)
{
    expanded = caja_location_expand_finish (result, &lookup_error);
    g_main_loop_quit (loop);
}

static void
lookup (const char *input)
{
    loop = g_main_loop_new (NULL, FALSE);
    caja_location_expand_async (input, NULL, ready, NULL);
    g_main_loop_run (loop);
    g_main_loop_unref (loop);
}

static void
check_lookup (const char *input, const char *expected)
{
    lookup (input);
    g_assert_no_error (lookup_error);
    g_assert_cmpstr (expanded, ==, expected);
    g_clear_pointer (&expanded, g_free);
}

static void
check_unresolved (const char *input)
{
    lookup (input);
    g_assert_error (lookup_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_assert_cmpstr (lookup_error->message, ==, input);
    g_assert_null (expanded);
    g_clear_error (&lookup_error);
}

static void
set_bashrc (const char *contents)
{
    char *path = g_build_filename (home, ".bashrc", NULL);
    g_assert_true (g_file_set_contents (path, contents, -1, NULL));
    g_free (path);
}

static void
test_environment (void)
{
    char *path;
    g_setenv ("CAJA_TEST_LOCATION", "/tmp/a b/音楽", TRUE);
    path = caja_location_expand_environment ("$CAJA_TEST_LOCATION/caja");
    g_assert_cmpstr (path, ==, "/tmp/a b/音楽/caja");
    g_free (path);
    set_bashrc ("CAJA_TEST_LOCATION=/wrong\n");
    check_lookup ("${CAJA_TEST_LOCATION}/caja", "/tmp/a b/音楽/caja");
    g_unsetenv ("CAJA_TEST_LOCATION");
}

static void
test_literal (void)
{
    const char *inputs[] = {
        "", "$", "${}", "${GIT", "${GIT:-/tmp}", "$(touch marker)",
        "`touch marker`", "$GIT.txt", "foo/$GIT", "/tmp/$GIT",
        "sftp://host/$GIT", "./$GIT", "~", "relative", "${1}", "$9", NULL
    };
    for (guint i = 0; inputs[i] != NULL; i++)
    {
        g_assert_false (caja_location_has_variable (inputs[i]));
        g_assert_null (caja_location_expand_environment (inputs[i]));
        check_lookup (inputs[i], inputs[i]);
    }
}

static void
test_bash (void)
{
    char *aliases = g_build_filename (home, ".bash_aliases", NULL);
    g_assert_true (g_file_set_contents (aliases,
                  "CAJA_TEST_LOCATION='/tmp/from aliases'\n", -1, NULL));
    set_bashrc ("printf 'startup\\0noise\\n'\n. \"$HOME/.bash_aliases\"\n");
    check_lookup ("$CAJA_TEST_LOCATION/caja", "/tmp/from aliases/caja");
    check_lookup ("${CAJA_TEST_LOCATION}", "/tmp/from aliases");
    set_bashrc ("CAJA_TEST_LOCATION='/tmp/$(touch marker);* $HOME'\n");
    check_lookup ("$CAJA_TEST_LOCATION/$(touch marker)",
                  "/tmp/$(touch marker);* $HOME/$(touch marker)");
    g_assert_false (g_file_test ("marker", G_FILE_TEST_EXISTS));
    set_bashrc ("unset CAJA_TEST_LOCATION\n");
    check_unresolved ("$CAJA_TEST_LOCATION");
    check_unresolved ("${CAJA_TEST_LOCATION}");
    check_unresolved ("$CAJA_TEST_LOCATION/caja");
    set_bashrc ("CAJA_TEST_LOCATION=''\n");
    check_unresolved ("$CAJA_TEST_LOCATION/caja");
    g_remove (aliases);
    g_free (aliases);
}

static void
test_completion (void)
{
    GFilenameCompleter *completer;
    GMainLoop *completion_loop;
    char *directory = g_build_filename (home, "caja", NULL);
    char *child = g_build_filename (directory, "nested", NULL);
    const char *inputs[] = {
        "$CAJA_TEST_LOCATION/ca", "${CAJA_TEST_LOCATION}/ca",
        "$CAJA_TEST_LOCATION/caja/ne", NULL
    };
    const char *suffixes[] = { "ja/", "ja/", "sted/" };

    g_assert_cmpint (g_mkdir (directory, 0700), ==, 0);
    g_assert_cmpint (g_mkdir (child, 0700), ==, 0);
    set_bashrc ("CAJA_TEST_LOCATION=$HOME\n");
    for (guint i = 0; inputs[i] != NULL; i++)
    {
        char *suffix;
        lookup (inputs[i]);
        g_assert_no_error (lookup_error);
        completer = g_filename_completer_new ();
        g_filename_completer_set_dirs_only (completer, TRUE);
        completion_loop = g_main_loop_new (NULL, FALSE);
        g_signal_connect_swapped (completer, "got-completion-data",
                                  G_CALLBACK (g_main_loop_quit), completion_loop);
        suffix = g_filename_completer_get_completion_suffix (completer, expanded);
        if (suffix == NULL)
        {
            g_main_loop_run (completion_loop);
            suffix = g_filename_completer_get_completion_suffix (completer, expanded);
        }
        g_assert_cmpstr (suffix, ==, suffixes[i]);
        g_free (suffix);
        g_clear_pointer (&expanded, g_free);
        g_main_loop_unref (completion_loop);
        g_object_unref (completer);
    }
    g_rmdir (child);
    g_rmdir (directory);
    g_free (child);
    g_free (directory);
}

static void
test_detached_session (void)
{
    gint64 session;
    set_bashrc ("CAJA_TEST_LOCATION=$(ps -o sid= -p $$)\n");
    lookup ("$CAJA_TEST_LOCATION");
    g_assert_no_error (lookup_error);
    g_assert_nonnull (expanded);
    session = g_ascii_strtoll (expanded, NULL, 10);
    g_assert_cmpint (session, >, 0);
    g_assert_cmpint (session, !=, getsid (0));
    g_clear_pointer (&expanded, g_free);
}

static void
test_missing_bash (void)
{
    char *old_path = g_strdup (g_getenv ("PATH"));
    g_setenv ("PATH", home, TRUE);
    check_unresolved ("$CAJA_TEST_LOCATION/caja");
    if (old_path != NULL)
        g_setenv ("PATH", old_path, TRUE);
    else
        g_unsetenv ("PATH");
    g_free (old_path);
}

static void
test_cancel (void)
{
    GCancellable *cancellable = g_cancellable_new ();
    set_bashrc ("sleep 10\nCAJA_TEST_LOCATION=/tmp\n");
    loop = g_main_loop_new (NULL, FALSE);
    caja_location_expand_async ("$CAJA_TEST_LOCATION", cancellable, ready, NULL);
    g_cancellable_cancel (cancellable);
    g_main_loop_run (loop);
    g_assert_error (lookup_error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_null (expanded);
    g_clear_error (&lookup_error);
    g_main_loop_unref (loop);
    g_object_unref (cancellable);
}

static void
test_timeout (void)
{
    gint64 start = g_get_monotonic_time ();
    set_bashrc ("sleep 10\nCAJA_TEST_LOCATION=/tmp\n");
    lookup ("$CAJA_TEST_LOCATION");
    g_assert_error (lookup_error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_null (expanded);
    g_assert_cmpint (g_get_monotonic_time () - start, <, 6 * G_USEC_PER_SEC);
    g_clear_error (&lookup_error);
}

int
main (int argc, char **argv)
{
    char *bashrc;
    int result;

    g_test_init (&argc, &argv, NULL);
    home = g_dir_make_tmp ("caja-location-test-XXXXXX", NULL);
    g_assert_nonnull (home);
    g_setenv ("HOME", home, TRUE);
    g_unsetenv ("CAJA_TEST_LOCATION");
    g_unsetenv ("BASH_ENV");
    g_test_add_func ("/location/environment", test_environment);
    g_test_add_func ("/location/literal", test_literal);
    g_test_add_func ("/location/bash", test_bash);
    g_test_add_func ("/location/completion", test_completion);
    g_test_add_func ("/location/detached-session", test_detached_session);
    g_test_add_func ("/location/missing-bash", test_missing_bash);
    g_test_add_func ("/location/cancel", test_cancel);
    g_test_add_func ("/location/timeout", test_timeout);
    result = g_test_run ();
    bashrc = g_build_filename (home, ".bashrc", NULL);
    g_remove (bashrc);
    g_free (bashrc);
    g_rmdir (home);
    g_free (home);
    return result;
}
