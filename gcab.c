#include "config.h"

#include <stdlib.h>
#include <libgcab.h>
#include <glib/gprintf.h>

static void
gcab_error (const gchar *format, ...)
{
    va_list args;

    g_fprintf (stderr, PACKAGE_NAME ": ");
    va_start(args, format);
    g_vfprintf (stderr, format, args);
    va_end(args);
    g_fprintf (stderr, "\n");

    exit (1);
}

int verbose = 0;

static void
file_callback (GCabFile *cabfile, gpointer data)
{
    GFile *file = gcab_file_get_file (cabfile);
    GFile *cwd = G_FILE (data);

    if (verbose) {
        gchar *path =  g_file_get_relative_path (cwd, file);
        if (!path)
            path = g_file_get_parse_name (file);
        g_print ("%s\n", path);
        g_free (path);
    }
}

static const gchar *
remove_leading_path (gchar *name)
{
    int i;

    i = 0;
    if (name[0] == G_DIR_SEPARATOR)
        i = 1;
    while (name[i] == '.' && name[i + 1] == '.' && name[i + 2] == G_DIR_SEPARATOR)
        i += 3;

    if (i != 0) {
        gchar c = name[i];

        name[i] = '\0';
        g_warning ("Removing leading '%s' from member names", name);
        name[i] = c;
    }

    return name + i;
}

int
main (int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *context;
    int i;

    gchar **args = NULL;
    int nopath = 0;
    int compress = 0;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Be verbose", NULL },
        { "zip", 'z', 0, G_OPTION_ARG_NONE, &compress, "Use zip compression", NULL },
        { "nopath", 'n', 0, G_OPTION_ARG_NONE, &nopath, "Do not include path", NULL },
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, "FILE INPUT_FILES..." },
        { NULL }
    };

    g_type_init ();
    g_set_prgname (PACKAGE_NAME);

    context = g_option_context_new ("- create a Cabinet file");
    g_option_context_set_description (context, "Report bugs to <" PACKAGE_BUGREPORT ">");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error))
        gcab_error ("option parsing failed: %s\n", error->message);
    g_option_context_free(context);


    if (!args || args[0] == NULL)
        gcab_error ("output cabinet file must be specified.");

    if (args[1] == NULL)
        gcab_error ("please specify input files.");

    GCabFolder *folder = gcab_folder_new ();
    g_object_set (folder, "compression", compress ? GCAB_COMPRESSION_MSZIP : 0, NULL);

    for (i = 1; args[i]; i++) {
        GFile *file = g_file_new_for_commandline_arg (args[i]);
        gchar *name = nopath ? g_path_get_basename (args[i]) : g_strdup (args[i]);
        GCabFile *cabfile = gcab_file_new_with_file (
                                 remove_leading_path (name), file);

        if (!gcab_folder_add_file (folder, cabfile, TRUE, NULL, &error)) {
            g_warning ("Can't add file %s: %s", args[i], error->message);
            g_clear_error (&error);
        }

        g_object_unref (cabfile);
        g_free (name);
        g_object_unref (file);
    }

    if (gcab_folder_get_nfiles (folder) == 0)
        gcab_error ("No files to be archived.");

    GFile *outputfile = g_file_new_for_commandline_arg (args[0]);
    GOutputStream *output = G_OUTPUT_STREAM (g_file_create (outputfile, 0, NULL, &error));
    if (error)
        gcab_error ("Can't create cab file %s: %s", args[0], error->message);

    GFile *cwd = g_file_new_for_commandline_arg (".");
    GCabCabinet *cabinet = gcab_cabinet_new ();
    if (!gcab_cabinet_add_folder (cabinet, folder, &error))
        gcab_error ("Can't add folder to cabinet: %s", args[0], error->message);

    if (!gcab_cabinet_write (cabinet, output,
                             file_callback,
                             NULL,
                             cwd,
                             NULL,
                             &error))
        gcab_error ("Can't write cab file %s: %s", args[0], error->message);

    g_object_unref (cabinet);
    g_object_unref (cwd);
    g_object_unref (output);
    g_object_unref (outputfile);

    return 0;
}
