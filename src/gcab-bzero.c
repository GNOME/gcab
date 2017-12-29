/*
 * LibGCab
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

int
main (int argc, char *argv[])
{
    guint64 bufsz = 1024;
    guint64 nfiles = 1;
    g_autofree guint8 *buf = NULL;
    g_autoptr(GBytes) bytes = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GCabFolder) folder = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) opfile = NULL;
    g_autoptr(GOutputStream) ostream = NULL;

    /* check args */
    if (argc < 2) {
        g_printerr ("Incorrect args, expected FILENAME [SIZE-IN-MB] [NR-FILES]\n");
        return EXIT_FAILURE;
    }

    /* get size */
    if (argc > 2) {
        bufsz = g_ascii_strtoull (argv[2], NULL, 10);
        if (bufsz == 0) {
            g_printerr ("Failed to parse base-10 integer: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }
    bufsz *= 0x100000;

    /* get number of files */
    if (argc > 3) {
        nfiles = g_ascii_strtoull (argv[3], NULL, 10);
        if (nfiles == 0) {
            g_printerr ("Failed to parse base-10 integer: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
    }

    /* create archive */
    cabinet = gcab_cabinet_new ();
    folder = gcab_folder_new (GCAB_COMPRESSION_MSZIP);
    if (!gcab_cabinet_add_folder (cabinet, folder, &error)) {
        g_printerr ("%s\n", error->message);
        return EXIT_FAILURE;
    }

    /* allocate huge buffer */
    buf = g_malloc0 (bufsz);
    bytes = g_bytes_new_take (g_steal_pointer (&buf), bufsz);

    /* add multiple files to the archive */
    for (guint64 i = 0; i < nfiles; i++) {
        g_autofree gchar *name = g_strdup_printf ("%04x", (guint) i);
        g_autoptr(GCabFile) file = gcab_file_new_with_bytes (name, bytes);
        if (!gcab_folder_add_file (folder, file, FALSE, NULL, &error)) {
            g_printerr ("%s\n", error->message);
            return EXIT_FAILURE;
        }
    }

    /* save file */
    opfile = g_file_new_for_path (argv[1]);
    ostream = G_OUTPUT_STREAM (g_file_replace (opfile, NULL, FALSE,
                                               0, NULL, &error));
    if (ostream == NULL) {
        g_printerr ("%s\n", error->message);
        return EXIT_FAILURE;
    }
    if (!gcab_cabinet_write_simple (cabinet, ostream, NULL, NULL, NULL, &error)) {
        g_printerr ("%s\n", error->message);
        return EXIT_FAILURE;
    }

    /* success */
    return EXIT_SUCCESS;
}
