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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <libgcab.h>

#define FUZZ_PRIMARY_NAME "fuzz.cab"

gint LLVMFuzzerInitialize (gint *argc, gchar ***argv);
gint LLVMFuzzerTestOneInput (const guint8 *data, gsize size);

static gboolean
fuzz_name_is_safe (const gchar *name)
{
    return name != NULL &&
           name[0] != '\0' &&
           g_strcmp0 (name, ".") != 0 &&
           g_strcmp0 (name, "..") != 0 &&
           strchr (name, '/') == NULL &&
           strchr (name, '\\') == NULL &&
           g_utf8_validate (name, -1, NULL);
}

static gboolean
fuzz_write_file (GFile *dir,
                 const gchar *name,
                 const guint8 *data,
                 gsize size,
                 GError **error)
{
    g_autoptr(GFile) file = g_file_get_child (dir, name);

    return g_file_replace_contents (file,
                                    (const gchar *) data,
                                    size,
                                    NULL,
                                    FALSE,
                                    G_FILE_CREATE_REPLACE_DESTINATION,
                                    NULL,
                                    NULL,
                                    error);
}

static gboolean
fuzz_write_chained_file (GFile *dir,
                         const gchar *name,
                         const guint8 *data,
                         gsize size,
                         gint delta,
                         GError **error)
{
    g_autofree guint8 *copy = NULL;
    guint16 cab_id;

    /* the CFHEADER setID and cabID fields are 16-bit little-endian values at
     * offsets 32 and 34; the rest of the cabinet is reused so that header
     * validation, continuation handling and extraction get exercised */
    if (size < 36 ||
        !fuzz_name_is_safe (name) ||
        g_strcmp0 (name, FUZZ_PRIMARY_NAME) == 0)
        return FALSE;

    cab_id = data[34] | ((guint16) data[35] << 8);
    cab_id = (guint16) (cab_id + delta);

    copy = g_memdup2 (data, size);
    copy[34] = cab_id & 0xff;
    copy[35] = cab_id >> 8;

    return fuzz_write_file (dir, name, copy, size, error);
}

static gboolean
fuzz_create_chain (GCabCabinet *cabinet,
                   GFile *dir,
                   const guint8 *data,
                   gsize size)
{
    const gchar *next_name = gcab_cabinet_get_next_name (cabinet);
    const gchar *prev_name = gcab_cabinet_get_prev_name (cabinet);
    g_autoptr(GError) error = NULL;
    gboolean created = FALSE;

    if (next_name != NULL &&
        fuzz_write_chained_file (dir, next_name, data, size, 1, &error))
        created = TRUE;
    g_clear_error (&error);

    if (prev_name != NULL &&
        g_strcmp0 (prev_name, next_name) != 0 &&
        fuzz_write_chained_file (dir, prev_name, data, size, -1, &error))
        created = TRUE;
    g_clear_error (&error);

    return created;
}

static void
fuzz_inspect_cabinet (GCabCabinet *cabinet)
{
    GPtrArray *folders = gcab_cabinet_get_folders (cabinet);
    g_autoptr(GError) error = NULL;

    for (guint i = 0; i < folders->len; i++) {
        GCabFolder *folder = GCAB_FOLDER (g_ptr_array_index (folders, i));
        g_autoptr(GSList) files = gcab_folder_get_files (folder);

        (void) gcab_folder_get_nfiles (folder);
        (void) gcab_folder_get_comptype (folder);

        for (GSList *l = files; l != NULL; l = l->next) {
            GCabFile *file = GCAB_FILE (l->data);
            g_autoptr(GDateTime) date_time = NULL;

            (void) gcab_file_get_name (file);
            (void) gcab_file_get_extract_name (file);
            (void) gcab_file_get_size (file);
            (void) gcab_file_get_attributes (file);
            (void) gcab_file_is_continued_from_prev (file);
            (void) gcab_file_is_continued_to_next (file);
            date_time = gcab_file_get_date_time (file);
        }
    }

    (void) gcab_cabinet_get_size (cabinet);
    (void) gcab_cabinet_get_signature (cabinet, NULL, &error);
}

static void
fuzz_extract_to_memory (const guint8 *data, gsize size)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(GInputStream) in = NULL;
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();

    in = g_memory_input_stream_new_from_data (data, (gssize) size, NULL);
    if (!gcab_cabinet_load (cabinet, in, NULL, &error))
        return;

    fuzz_inspect_cabinet (cabinet);
    g_clear_error (&error);
    (void) gcab_cabinet_extract (cabinet, NULL, NULL, NULL, NULL, NULL, &error);
}

static void
fuzz_remove_tree (GFile *file)
{
    if (g_file_query_file_type (file,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                NULL) == G_FILE_TYPE_DIRECTORY) {
        g_autoptr(GFileEnumerator) enumerator = NULL;

        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                NULL,
                                                NULL);
        if (enumerator != NULL) {
            for (;;) {
                g_autoptr(GFileInfo) info = NULL;
                g_autoptr(GFile) child = NULL;

                info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                if (info == NULL)
                    break;

                child = g_file_get_child (file, g_file_info_get_name (info));
                fuzz_remove_tree (child);
            }
        }
    }

    (void) g_file_delete (file, NULL, NULL);
}

static void
fuzz_extract_to_directory (const guint8 *data, gsize size)
{
    g_autofree gchar *path = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) dir = NULL;
    g_autoptr(GFile) primary = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;

    path = g_dir_make_tmp ("gcab-fuzz-XXXXXX", &error);
    if (path == NULL)
        return;

    dir = g_file_new_for_path (path);
    if (!fuzz_write_file (dir, FUZZ_PRIMARY_NAME, data, size, &error))
        goto out;

    primary = g_file_get_child (dir, FUZZ_PRIMARY_NAME);
    cabinet = gcab_cabinet_new ();
    if (!gcab_cabinet_load_from_file (cabinet, primary, NULL, &error))
        goto out;

    /* materialise the siblings named in the header so the file loader follows
     * the chain instead of silently stopping at the missing next cabinet */
    g_clear_error (&error);
    if (fuzz_create_chain (cabinet, dir, data, size)) {
        g_autoptr(GCabCabinet) chained = gcab_cabinet_new ();

        if (!gcab_cabinet_load_from_file (chained, primary, NULL, &error))
            goto out;
        cabinet = g_steal_pointer (&chained);
    }

    fuzz_inspect_cabinet (cabinet);
    g_clear_error (&error);
    (void) gcab_cabinet_extract (cabinet, dir, NULL, NULL, NULL, NULL, &error);

out:
    fuzz_remove_tree (dir);
}

gint
LLVMFuzzerInitialize (gint *argc, gchar ***argv)
{
    (void) argc;
    (void) argv;

    g_setenv ("GCAB_SKIP_CHECKSUM", "1", FALSE);
    g_setenv ("GIO_USE_VFS", "local", FALSE);
    return 0;
}

gint
LLVMFuzzerTestOneInput (const guint8 *data, gsize size)
{
    if (size == 0 || size > G_MAXSSIZE)
        return 0;

    /* Alternate between the in-memory and file/directory paths so both are
     * covered by the same corpus. */
    if (size & 1)
        fuzz_extract_to_directory (data, size);
    else
        fuzz_extract_to_memory (data, size);

    return 0;
}
