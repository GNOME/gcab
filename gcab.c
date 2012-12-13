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
file_callback (GFile *file, gpointer data)
{
    GFile *cwd = data;

    if (verbose) {
        gchar *path =  g_file_get_relative_path (cwd, file);
        if (!path)
            path = g_file_get_parse_name (file);
        g_print ("%s\n", path);
        g_free (path);
    }
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

    GCabFile *gcab = gcab_file_new ();
    for (i = 1; args[i]; i++) {
        GFile *file = g_file_new_for_commandline_arg (args[i]);
        gchar *name = nopath ? g_path_get_basename (args[i]) : g_strdup (args[i]);
        if (!gcab_file_add (gcab, file, name, TRUE, NULL, &error)) {
            g_warning ("Can't add file %s: %s", args[i], error->message);
            g_clear_error (&error);
        }
        g_free (name);
        g_object_unref (file);
    }

    GFile *outputfile = g_file_new_for_commandline_arg (args[0]);
    GOutputStream *output = G_OUTPUT_STREAM (g_file_create (outputfile, 0, NULL, &error));
    if (error)
        gcab_error ("Can't create cab file %s: %s", args[0], error->message);

    GFile *cwd = g_file_new_for_commandline_arg (".");
    if (!gcab_file_save (gcab, output,
                         compress ? GCAB_COMPRESSION_MSZIP : 0,
                         file_callback,
                         NULL,
                         cwd,
                         NULL,
                         &error))
        gcab_error ("Can't save cab file %s: %s", args[0], error->message);

    g_object_unref (output);
    g_object_unref (outputfile);
    g_object_unref (gcab);

    return 0;
}
