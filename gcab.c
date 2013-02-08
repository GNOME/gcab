#include "config.h"

#include <stdlib.h>
#include <libgcab.h>
#include <glib/gprintf.h>
#include <locale.h>
#include <glib/gi18n.h>

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

static gboolean
file_callback (GCabFile *cabfile, gpointer data)
{
    GFile *file = gcab_file_get_file (cabfile);
    GFile *cwd = G_FILE (data);

    if (!verbose)
        return TRUE;

    if (file) {
        gchar *path =  g_file_get_relative_path (cwd, file);
        if (!path)
            path = g_file_get_parse_name (file);
        g_print ("%s\n", path);
        g_free (path);
    } else {
        g_print ("%s\n", gcab_file_get_name (cabfile));
    }

    return TRUE;
}

static const gchar *
remove_leading_path (gchar *name)
{
    unsigned i;

    i = 0;
    if (name[0] == G_DIR_SEPARATOR)
        i = 1;
    while (name[i] == '.' && name[i + 1] == '.' && name[i + 2] == G_DIR_SEPARATOR)
        i += 3;

    if (i != 0) {
        gchar c = name[i];

        name[i] = '\0';
        g_warning (_("Removing leading '%s' from member names"), name);
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
    gchar *change = NULL;
    int version = 0;
    int nopath = 0;
    int compress = 0;
    int list = 0;
    int create = 0;
    int extract = 0;
    GOptionEntry entries[] = {
        { "version", 0, 0, G_OPTION_ARG_NONE, &version, N_("Print program version"), NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("Be verbose"), NULL },
        { "create", 'c', 0, G_OPTION_ARG_NONE, &create, N_("Create archive"), NULL },
        { "extract", 'x', 0, G_OPTION_ARG_NONE, &extract, N_("Extract all files"), NULL },
        { "list", 't', 0, G_OPTION_ARG_NONE, &list, N_("List content"), NULL },
        { "directory", 'C', 0, G_OPTION_ARG_FILENAME, &change, N_("Change to directory DIR"), N_("DIR") },
        { "zip", 'z', 0, G_OPTION_ARG_NONE, &compress, N_("Use zip compression"), NULL },
        { "nopath", 'n', 0, G_OPTION_ARG_NONE, &nopath, N_("Do not include path"), NULL },
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("FILE INPUT_FILES...") },
        { NULL }
    };

    setlocale (LC_ALL, "");
    textdomain (GETTEXT_PACKAGE);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#if !GLIB_CHECK_VERSION(2,35,1)
    g_type_init ();
#endif
    g_set_prgname (PACKAGE_NAME);

    context = g_option_context_new (_("- create a Cabinet file"));
    gchar *s = g_strdup_printf (_("Report bugs to <%s>"), PACKAGE_BUGREPORT);
    g_option_context_set_description (context, s);
    g_free(s);
    g_option_context_set_summary (context, _("\
gcab saves many files together into a cabinet archive, and can restore\n\
individual files from the archive.\
"));
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
    if (!g_option_context_parse (context, &argc, &argv, &error))
        gcab_error (_("option parsing failed: %s\n"), error->message);
    g_option_context_free(context);

    if (version) {
        g_printf (PACKAGE_STRING "\n");
        return 0;
    }

    if ((list + extract + create) != 1)
        gcab_error (_("Please specify a single operation."));

    if (!args || args[0] == NULL)
        gcab_error (_("cabinet file must be specified."));

    GCancellable *cancellable = g_cancellable_new ();
    GCabCabinet *cabinet = gcab_cabinet_new ();
    GCabFolder *folder;
    GFile *outputfile;
    GOutputStream *output;
    GFile *cwd;

    if (list || extract) {
        GFile *file = g_file_new_for_commandline_arg (args[0]);
        GInputStream *in = G_INPUT_STREAM (g_file_read (file, cancellable, &error));

        if (!in)
            gcab_error (_("can't open %s for reading: %s\n"), args[0], error->message);
        if (!gcab_cabinet_load (cabinet, in, cancellable, &error))
            gcab_error (_("error reading %s: %s\n"), args[0], error->message);

        if (list) {
            GPtrArray *folders = gcab_cabinet_get_folders (cabinet);
            for (i = 0; i < folders->len; i++) {
                GSList *l, *files = gcab_folder_get_files (g_ptr_array_index (folders, i));
                for (l = files; l != NULL; l = l->next)
                    g_print ("%s\n", gcab_file_get_name (GCAB_FILE (l->data)));
                g_slist_free (files);
            }
        } else if (extract) {
            g_object_unref (file);
            if (change == NULL)
                change = g_get_current_dir ();
            file = g_file_new_for_path (change);

            if (!gcab_cabinet_extract (cabinet, file, file_callback, NULL, NULL, cancellable, &error))
                gcab_error (_("error during extraction: %s"), error->message);
        }

        g_object_unref (in);
        g_object_unref (file);
        goto end;
    }

    if (args[1] == NULL)
        gcab_error (_("please specify input files."));

    folder = gcab_folder_new (compress ? GCAB_COMPRESSION_MSZIP : 0);

    for (i = 1; args[i]; i++) {
        GFile *file = g_file_new_for_commandline_arg (args[i]);
        gchar *name = nopath ? g_path_get_basename (args[i]) : g_strdup (args[i]);
        GCabFile *cabfile = gcab_file_new_with_file (
                                 remove_leading_path (name), file);

        if (!gcab_folder_add_file (folder, cabfile, TRUE, NULL, &error)) {
            g_warning (_("Can't add file %s: %s"), args[i], error->message);
            g_clear_error (&error);
        }

        g_object_unref (cabfile);
        g_free (name);
        g_object_unref (file);
    }

    if (gcab_folder_get_nfiles (folder) == 0)
        gcab_error (_("no files to be archived."));

    outputfile = g_file_new_for_commandline_arg (args[0]);
    output = G_OUTPUT_STREAM (g_file_replace (outputfile, NULL, FALSE,
                                              0, NULL, &error));
    if (error)
        gcab_error (_("can't create cab file %s: %s"), args[0], error->message);

    cwd = g_file_new_for_commandline_arg (".");
    if (!gcab_cabinet_add_folder (cabinet, folder, &error))
        gcab_error (_("can't add folder to cabinet: %s"), args[0], error->message);

    if (!gcab_cabinet_write (cabinet, output,
                             file_callback,
                             NULL,
                             cwd,
                             NULL,
                             &error))
        gcab_error (_("can't write cab file %s: %s"), args[0], error->message);

    g_object_unref (cwd);
    g_object_unref (output);
    g_object_unref (outputfile);

end:
    g_object_unref (cabinet);
    g_object_unref (cancellable);

    return 0;
}
