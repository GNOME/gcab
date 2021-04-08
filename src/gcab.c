/*
 * LibGCab
 * Copyright (c) 2012, Marc-André Lureau <marcandre.lureau@gmail.com>
 * Copyright (c) 2017, Richard Hughes <richard@hughsie.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include "config.h"

#include <stdlib.h>
#include <libgcab.h>
#include <glib/gprintf.h>
#include <locale.h>
#include <glib/gi18n.h>

int verbose = 0;

static gboolean
file_callback (GCabFile *cabfile, gpointer data)
{
    GFile *file = gcab_file_get_file (cabfile);
    GFile *cwd = G_FILE (data);

    if (!verbose)
        return TRUE;

    if (file) {
        g_autofree gchar *path =  g_file_get_relative_path (cwd, file);
        if (!path)
            path = g_file_get_parse_name (file);
        g_print ("%s\n", path);
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
        g_debug ("removing leading '%s' from member names", name);
        name[i] = c;
    }

    return name + i;
}

static gboolean
save_array_to_file (const GByteArray *array, const gchar *base, const gchar *suffix, GError **error)
{
    g_autofree gchar *resname = NULL;
    g_autoptr(GFile) outputfile = NULL;
    g_autoptr(GOutputStream) output = NULL;
    resname = g_strdup_printf ("%s.%s", base, suffix);
    g_print ("%s %s...", _("Dumping data to:"), resname);
    outputfile = g_file_new_for_commandline_arg (resname);
    output = G_OUTPUT_STREAM (g_file_replace (outputfile, NULL, FALSE, 0, NULL, error));
    if (output == NULL) {
        g_prefix_error (error, "cannot write %s: ", resname);
        return FALSE;
    }
    if (!g_output_stream_write_all (output, array->data, array->len, NULL, NULL, error)) {
        g_prefix_error (error, "cannot write %s: ", resname);
        return FALSE;
    }
    return TRUE;
}

int
main (int argc, char *argv[])
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GOptionContext) context = NULL;

    g_auto(GStrv) args = NULL;
    g_autofree gchar *change = NULL;
    gboolean version = FALSE;
    gboolean nopath = FALSE;
    gboolean space = FALSE;
    gboolean compress = FALSE;
    gboolean list = FALSE;
    gboolean list_details = FALSE;
    gboolean create = FALSE;
    gboolean extract = FALSE;
    gboolean dump_reserved = FALSE;
    GOptionEntry entries[] = {
        { "version", 0, 0, G_OPTION_ARG_NONE, &version, N_("Print program version"), NULL },
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("Be verbose"), NULL },
        { "create", 'c', 0, G_OPTION_ARG_NONE, &create, N_("Create archive"), NULL },
        { "extract", 'x', 0, G_OPTION_ARG_NONE, &extract, N_("Extract all files"), NULL },
        { "dump-reserved", 'D', 0, G_OPTION_ARG_NONE, &dump_reserved, N_("Dump reserved and extra data"), NULL },
        { "list", 't', 0, G_OPTION_ARG_NONE, &list, N_("List content"), NULL },
        { "list-details", 'l', 0, G_OPTION_ARG_NONE, &list_details, N_("List content with file details"), NULL },
        { "directory", 'C', 0, G_OPTION_ARG_FILENAME, &change, N_("Change to directory DIR"), N_("DIR") },
        { "zip", 'z', 0, G_OPTION_ARG_NONE, &compress, N_("Use zip compression"), NULL },
        { "nopath", 'n', 0, G_OPTION_ARG_NONE, &nopath, N_("Do not include path"), NULL },
        { "space", 's', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &space, N_("Reserve space in cabinet for signing (e.g. -s 6144 reserves 6K bytes)"), NULL },
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
    g_autofree gchar *s = g_strdup_printf ("%s %s", _("Report bugs to:"), PACKAGE_BUGREPORT);
    g_option_context_set_description (context, s);
    g_option_context_set_summary (context, _("\
gcab saves many files together into a cabinet archive, and can restore\n\
individual files from the archive.\
"));
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
    g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("%s: %s\n", _("Option parsing failed"), error->message);
        return EXIT_FAILURE;
    }

    if (version) {
        g_printf (PACKAGE_STRING "\n");
        return 0;
    }

    if ((list + extract + create + dump_reserved + list_details) != 1) {
        g_printerr ("%s\n", _("Please specify a single operation"));
        return EXIT_FAILURE;
    }

    if (!args || args[0] == NULL) {
        g_printerr ("%s\n", _("Cabinet file must be specified"));
        return EXIT_FAILURE;
    }

    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
    g_autoptr(GCabFolder) folder = NULL;
    g_autoptr(GCancellable) cancellable = g_cancellable_new ();
    g_autoptr(GFile) cwd = NULL;
    g_autoptr(GFile) outputfile = NULL;
    g_autoptr(GOutputStream) output = NULL;

    if (list || list_details || extract || dump_reserved) {
        g_autoptr(GFile) file = g_file_new_for_commandline_arg (args[0]);
        g_autoptr(GInputStream) in = G_INPUT_STREAM (g_file_read (file, cancellable, &error));

        if (in == NULL) {
            g_printerr ("%s %s: %s\n", _("Cannot open file for reading"), args[0], error->message);
            return EXIT_FAILURE;
        }
        if (!gcab_cabinet_load (cabinet, in, cancellable, &error)) {
            g_printerr ("%s %s: %s\n", _("Error reading"), args[0], error->message);
            return EXIT_FAILURE;
        }

        if (list || list_details) {
            GPtrArray *folders = gcab_cabinet_get_folders (cabinet);
            for (guint i = 0; i < folders->len; i++) {
                GSList *l;
                g_autoptr(GSList) files = gcab_folder_get_files (g_ptr_array_index (folders, i));

                for (l = files; l != NULL; l = l->next) {
                    if (list_details) {
                        g_autofree char *date = NULL;
                        g_autoptr(GDateTime) dt = gcab_file_get_date_time (GCAB_FILE (l->data));
                        if (dt) {
                            g_autoptr(GDateTime) dtl = g_date_time_to_local (dt);
                            date = g_date_time_format (dtl, "%Y-%m-%d %H:%M:%S");
                        }

                        g_print ("%s %u %s 0x%X\n",
                                 gcab_file_get_name (GCAB_FILE (l->data)),
                                 gcab_file_get_size (GCAB_FILE (l->data)),
                                 date ? date : _("<unknown-date>"),
                                 gcab_file_get_attributes (GCAB_FILE (l->data)));
                    } else {
                        g_print ("%s\n", gcab_file_get_name (GCAB_FILE (l->data)));
                    }
                }
            }
        } else if (extract) {
            g_autoptr(GFile) file2 = NULL;
            if (change == NULL)
                change = g_get_current_dir ();
            file2 = g_file_new_for_path (change);
            if (!gcab_cabinet_extract (cabinet, file2, file_callback, NULL, NULL, cancellable, &error)) {
                g_printerr ("%s: %s\n", _("Error during extraction"), error->message);
                return EXIT_FAILURE;
            }
        } else if (dump_reserved) {
            g_autoptr(GByteArray) reserved = NULL;
            g_object_get (cabinet, "reserved", &reserved, NULL);
            if (reserved != NULL) {
                if (!save_array_to_file (reserved, args[0], "header", &error)) {
                    g_printerr ("%s\n", error->message);
                    return EXIT_FAILURE;
                }
            }

            reserved = (GByteArray *)gcab_cabinet_get_signature (cabinet, cancellable, &error);
            if (reserved == NULL) {
                g_printerr ("%s: %s\n", _("Error while reading signature"), error->message);
                return EXIT_FAILURE;
            }
            if (!save_array_to_file (reserved, args[0], "signature", &error)) {
                g_printerr ("%s\n", error->message);
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }

    if (args[1] == NULL) {
        g_printerr ("%s\n", _("No input files specified"));
        return EXIT_FAILURE;
    }

    if (space) {
        g_autoptr(GByteArray) reserved = g_byte_array_sized_new (space);
        g_byte_array_set_size (reserved, space);
        g_object_set (cabinet, "reserved", reserved, NULL);
    }

    folder = gcab_folder_new (compress ? GCAB_COMPRESSION_MSZIP : 0);

    for (gint i = 1; args[i]; i++) {
        g_autoptr(GFile) file = g_file_new_for_commandline_arg (args[i]);
        g_autofree gchar *name = nopath ? g_path_get_basename (args[i]) : g_strdup (args[i]);
        g_autoptr(GCabFile) cabfile = gcab_file_new_with_file (
                                 remove_leading_path (name), file);

        if (!gcab_folder_add_file (folder, cabfile, TRUE, NULL, &error)) {
            g_warning ("%s %s: %s", _("Cannot add file"), args[i], error->message);
            g_clear_error (&error);
        }
    }

    if (gcab_folder_get_nfiles (folder) == 0) {
        g_printerr ("%s\n", _("No files to be archived"));
        return EXIT_FAILURE;
    }

    outputfile = g_file_new_for_commandline_arg (args[0]);
    output = G_OUTPUT_STREAM (g_file_replace (outputfile, NULL, FALSE,
                                              0, NULL, &error));
    if (output == NULL) {
        g_printerr ("%s %s: %s\n", _("Cannot create cab file"), args[0], error->message);
        return EXIT_FAILURE;
    }

    cwd = g_file_new_for_commandline_arg (".");
    if (!gcab_cabinet_add_folder (cabinet, folder, &error)) {
        g_printerr ("%s %s: %s\n", _("Cannot add folder to cab file"), args[0], error->message);
        return EXIT_FAILURE;
    }

    if (!gcab_cabinet_write (cabinet, output,
                             file_callback,
                             NULL,
                             cwd,
                             NULL,
                             &error)) {
        g_printerr ("%s %s: %s\n", _("Cannot write cab file"), args[0], error->message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
