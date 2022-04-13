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

static gboolean
_process_file (GFile *file, GError **error)
{
    GPtrArray *folders;
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
    g_autoptr(GInputStream) in = NULL;

    /* load file */
    in = G_INPUT_STREAM (g_file_read (file, NULL, error));
    if (in == NULL) {
        g_prefix_error (error, "Cannot open file: ");
        return FALSE;
    }
    if (!gcab_cabinet_load (cabinet, in, NULL, error)) {
        g_prefix_error (error, "Error reading: ");
        return FALSE;
    }

    /* print file list and extract to memory */
    folders = gcab_cabinet_get_folders (cabinet);
    for (guint i = 0; i < folders->len; i++) {
        GCabFolder *folder = GCAB_FOLDER (g_ptr_array_index (folders, i));
        g_autoptr(GSList) files = gcab_folder_get_files (folder);
        for (GSList *l = files; l != NULL; l = l->next)
            g_print ("%s\n", gcab_file_get_name (GCAB_FILE (l->data)));
    }
    if (!gcab_cabinet_extract (cabinet, NULL, NULL, NULL, NULL, NULL, error)) {
        g_prefix_error (error, "Error during extraction: ");
        return FALSE;
    }
    return TRUE;
}

int
main (int argc, char *argv[])
{
    g_autoptr(GError) error = NULL;

    /* check args */
    if (argc < 2) {
        g_printerr ("No input files specified\n");
        return EXIT_FAILURE;
    }

    /* try to extract each file */
    (void)g_setenv ("GCAB_SKIP_CHECKSUM", "1", FALSE);
    for (gint i = 1; i < argc; i++) {
        g_autoptr(GFile) file = g_file_new_for_commandline_arg (argv[i]);
        if (!_process_file (file, &error)) {
            g_printerr ("Failed to read %s: %s\n", argv[i], error->message);
            return EXIT_FAILURE;
        }
    }

    /* success */
    return EXIT_SUCCESS;
}
