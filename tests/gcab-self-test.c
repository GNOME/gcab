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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <libgcab.h>

#include "gcab-priv.h"

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

static gchar *
gcab_test_get_filename (const gchar *filename)
{
    return g_test_build_filename (G_TEST_DIST, filename, NULL);
}

static gboolean
gcab_test_count_file_callback (GCabFile *file, gpointer user_data)
{
    guint *count = user_data;

    (void)file;
    (*count)++;
    return TRUE;
}

static void
gcab_test_file_func (void)
{
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabFile) cabfile = NULL;
    g_autoptr(GDateTime) dt = NULL;
    g_autoptr(GFile) gfile = NULL;
    g_autoptr(GDateTime) dt_bday = NULL;

    /* create object */
    fn = gcab_test_get_filename ("src/test.sh");
    g_assert (fn != NULL);
    gfile = g_file_new_for_path (fn);
    cabfile = gcab_file_new_with_file ("tæst.bin", gfile);
    g_assert (cabfile != NULL);

    /* set the time */
    dt_bday = g_date_time_new_utc (2017, 9, 15, 0, 0, 0.f);
    gcab_file_set_date_time (cabfile, dt_bday);

    /* verify */
    g_assert (gcab_file_get_file (cabfile) == gfile);
    g_assert_cmpstr (gcab_file_get_name (cabfile), ==, "tæst.bin");
    g_assert_cmpstr (gcab_file_get_extract_name (cabfile), ==, "tæst.bin");
    g_assert_cmpint (gcab_file_get_size (cabfile), ==, 0);
    g_assert_cmpint (gcab_file_get_attributes (cabfile), ==, 0);
    dt = gcab_file_get_date_time (cabfile);
    g_assert_cmpint (g_date_time_get_year (dt), ==, 2017);
    g_assert_cmpint (g_date_time_get_month (dt), ==, 9);
    g_assert_cmpint (g_date_time_get_day_of_month (dt), ==, 15);
    g_assert_cmpint (g_date_time_get_hour (dt), ==, 0);
    g_assert_cmpint (g_date_time_get_minute (dt), ==, 0);
    g_assert_cmpint (g_date_time_get_second (dt), ==, 0);

    /* test changing the extraction name */
    gcab_file_set_extract_name (cabfile, "new.bin");
    g_assert_cmpstr (gcab_file_get_extract_name (cabfile), ==, "new.bin");
    g_assert_cmpstr (gcab_file_get_name (cabfile), ==, "tæst.bin");

    /* deprecated GTimeVal accessors */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GTimeVal tv = { 0 };
    GTimeVal tv_new = { 1500000000, 0 };
    GTimeVal tv_out = { 0 };
    g_assert_true (gcab_file_get_date (cabfile, &tv));
    g_assert_cmpint (tv.tv_sec, ==, g_date_time_to_unix (dt_bday));
    gcab_file_set_date (cabfile, &tv_new);
    g_assert_true (gcab_file_get_date (cabfile, &tv_out));
    g_assert_cmpint (tv_out.tv_sec, ==, tv_new.tv_sec);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
_gcab_test_file_properties_func (void)
{
    const gchar new_contents[] = "new contents";
    g_autoptr(GCabFile) cabfile = NULL;
    g_autoptr(GBytes) bytes = g_bytes_new_static ("contents", 8);
    g_autoptr(GBytes) bytes_new = g_bytes_new_static (new_contents, sizeof (new_contents) - 1);
    g_autoptr(GBytes) bytes_out = NULL;
    g_autoptr(GFile) gfile = g_file_new_for_path ("/tmp/gcab-property-test.bin");
    g_autoptr(GFile) file_out = NULL;
    g_autofree gchar *name = NULL;

    /* exercise the GObject properties directly */
    cabfile = gcab_file_new_with_bytes ("property.bin", bytes);
    g_assert_nonnull (cabfile);

    /* no date has been set yet */
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GTimeVal tv = { 0 };
    g_assert_false (gcab_file_get_date (cabfile, &tv));
    G_GNUC_END_IGNORE_DEPRECATIONS

    g_object_get (cabfile,
                  "name", &name,
                  "file", &file_out,
                  "bytes", &bytes_out,
                  NULL);
    g_assert_cmpstr (name, ==, "property.bin");
    g_assert_null (file_out);
    g_assert_true (bytes_out == bytes);

    g_object_set (cabfile, "name", "renamed.bin", NULL);
    g_assert_cmpstr (gcab_file_get_name (cabfile), ==, "renamed.bin");

    g_object_set (cabfile, "file", gfile, NULL);
    g_clear_pointer (&file_out, g_object_unref);
    g_object_get (cabfile, "file", &file_out, NULL);
    g_assert_true (file_out == gfile);

    /* replacing an existing GBytes also exercises the old-value cleanup */
    g_object_set (cabfile, "bytes", bytes_new, NULL);
    g_clear_pointer (&bytes_out, g_bytes_unref);
    g_object_get (cabfile, "bytes", &bytes_out, NULL);
    g_assert_true (bytes_out == bytes_new);
    g_assert_cmpuint (gcab_file_get_size (cabfile), ==, sizeof (new_contents) - 1);
}

static void
_gcab_test_file_clear_bytes_func (void)
{
    const gchar contents[] = "payload";
    g_autoptr(GBytes) bytes = g_bytes_new_static (contents, sizeof (contents) - 1);
    g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes ("clear.bin", bytes);

    g_assert_true (gcab_file_get_bytes (cabfile) == bytes);
    g_assert_cmpuint (gcab_file_get_size (cabfile), ==, sizeof (contents) - 1);

    /* re-setting the same GBytes must not drop the last reference */
    gcab_file_set_bytes (cabfile, bytes);
    g_assert_true (gcab_file_get_bytes (cabfile) == bytes);
    g_assert_cmpuint (gcab_file_get_size (cabfile), ==, sizeof (contents) - 1);

    /* clearing the property is allowed and resets the cached size */
    g_object_set (cabfile, "bytes", NULL, NULL);
    g_assert_null (gcab_file_get_bytes (cabfile));
    g_assert_cmpuint (gcab_file_get_size (cabfile), ==, 0);
}

static void
_gcab_test_folder_properties_func (void)
{
    const guint8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    g_autoptr(GCabFolder) cabfolder = NULL;
    g_autoptr(GByteArray) reserved = g_byte_array_new_take (g_memdup2 (data, sizeof (data)), sizeof (data));
    g_autoptr(GByteArray) reserved_new = g_byte_array_new_take (g_memdup2 (data, 2), 2);
    g_autoptr(GByteArray) reserved_out = NULL;
    gint comptype = 0;
    gint compression = -1;

    /* exercise the GObject properties directly */
    cabfolder = g_object_new (GCAB_TYPE_FOLDER,
                              "comptype", GCAB_COMPRESSION_MSZIP | 0x20,
                              "reserved", reserved,
                              NULL);
    g_assert_nonnull (cabfolder);

    g_object_get (cabfolder,
                  "compression", &compression,
                  "comptype", &comptype,
                  "reserved", &reserved_out,
                  NULL);
    g_assert_cmpint (compression, ==, GCAB_COMPRESSION_MSZIP);
    g_assert_cmpint (comptype, ==, GCAB_COMPRESSION_MSZIP | 0x20);
    g_assert_cmpuint (reserved_out->len, ==, sizeof (data));
    g_assert_cmpmem (reserved_out->data, reserved_out->len, data, sizeof (data));

    /* replacing the reserved area also exercises the old-value cleanup */
    g_object_set (cabfolder, "reserved", reserved_new, NULL);
    g_clear_pointer (&reserved_out, g_byte_array_unref);
    g_object_get (cabfolder, "reserved", &reserved_out, NULL);
    g_assert_cmpuint (reserved_out->len, ==, 2);
    g_assert_cmpmem (reserved_out->data, reserved_out->len, data, 2);
}

static void
_gcab_test_cabinet_properties_func (void)
{
    const guint8 data[] = { 0x09, 0x08, 0x07, 0x06 };
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
    g_autoptr(GByteArray) reserved = g_byte_array_new_take (g_memdup2 (data, sizeof (data)), sizeof (data));
    g_autoptr(GByteArray) reserved_new = g_byte_array_new_take (g_memdup2 (data, 2), 2);
    g_autoptr(GByteArray) reserved_out = NULL;
    g_autoptr(GByteArray) signature = g_byte_array_new_take (g_memdup2 (data, sizeof (data)), sizeof (data));
    g_autoptr(GByteArray) signature_new = g_byte_array_new_take (g_memdup2 (data, 2), 2);
    g_autoptr(GByteArray) signature_out = NULL;

    g_assert_nonnull (cabinet);
    g_assert_null (gcab_cabinet_get_next_name (cabinet));
    g_assert_null (gcab_cabinet_get_prev_name (cabinet));
    g_assert_null (gcab_cabinet_get_next (cabinet));
    g_assert_null (gcab_cabinet_get_prev (cabinet));

    g_object_set (cabinet, "reserved", reserved, NULL);
    g_object_get (cabinet, "reserved", &reserved_out, NULL);
    g_assert_cmpuint (reserved_out->len, ==, sizeof (data));
    g_assert_cmpmem (reserved_out->data, reserved_out->len, data, sizeof (data));

    /* replacing the reserved area also exercises the old-value cleanup */
    g_object_set (cabinet, "reserved", reserved_new, NULL);
    g_clear_pointer (&reserved_out, g_byte_array_unref);
    g_object_get (cabinet, "reserved", &reserved_out, NULL);
    g_assert_cmpuint (reserved_out->len, ==, 2);

    g_object_set (cabinet, "signature", signature, NULL);
    g_object_get (cabinet, "signature", &signature_out, NULL);
    g_assert_cmpuint (signature_out->len, ==, sizeof (data));
    g_assert_cmpmem (signature_out->data, signature_out->len, data, sizeof (data));

    /* replacing the signature also exercises the old-value cleanup */
    g_object_set (cabinet, "signature", signature_new, NULL);
    g_clear_pointer (&signature_out, g_byte_array_unref);
    g_object_get (cabinet, "signature", &signature_out, NULL);
    g_assert_cmpuint (signature_out->len, ==, 2);

    /* extraction limits have setters and getters */
    gcab_cabinet_set_extract_size_limit (cabinet, 1234);
    g_assert_cmpuint (gcab_cabinet_get_extract_size_limit (cabinet), ==, 1234);
    gcab_cabinet_set_extract_ratio_limit (cabinet, 42);
    g_assert_cmpuint (gcab_cabinet_get_extract_ratio_limit (cabinet), ==, 42);
}

static void
gcab_test_folder_func (void)
{
    GCabFile *cabfile_tmp;
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabFile) cabfile2 = NULL;
    g_autoptr(GCabFile) cabfile = NULL;
    g_autoptr(GCabFolder) cabfolder = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) gfile = NULL;
    g_autoptr(GSList) cabfiles = NULL;

    /* create GCabFile */
    fn = gcab_test_get_filename ("test-none.cab");
    g_assert (fn != NULL);
    gfile = g_file_new_for_path (fn);
    cabfile = gcab_file_new_with_file ("test.bin", gfile);

    /* add single file */
    cabfolder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    ret = gcab_folder_add_file (cabfolder, cabfile, FALSE, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);
    g_assert_cmpint (gcab_folder_get_nfiles (cabfolder), ==, 1);
    cabfiles = gcab_folder_get_files (cabfolder);
    cabfile_tmp = GCAB_FILE (cabfiles->data);
    g_assert (cabfile_tmp == cabfile);
    g_assert_cmpstr (gcab_file_get_name (cabfile_tmp), ==, "test.bin");

    /* add it again */
    cabfile2 = gcab_file_new_with_file ("test.bin", gfile);
    ret = gcab_folder_add_file (cabfolder, cabfile2, FALSE, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT);
    g_assert (!ret);

    /* find using the hash table */
    cabfile_tmp = gcab_folder_get_file_by_name (cabfolder, "test.bin");
    g_assert_nonnull (cabfile_tmp);
    cabfile_tmp = gcab_folder_get_file_by_name (cabfolder, "notgoingtoexist");
    g_assert_null (cabfile_tmp);

    /* find the extracted name too */
    cabfile_tmp = gcab_folder_get_file_by_name (cabfolder, "test.bin");
    gcab_file_set_extract_name (cabfile_tmp, "LVFS-test.bin");
    cabfile_tmp = gcab_folder_get_file_by_name (cabfolder, "LVFS-test.bin");
    g_assert_nonnull (cabfile_tmp);
}

static void
gcab_test_folder_path_func (void)
{
    GCabFile *cabfile_tmp;
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabFile) cabfile = NULL;
    g_autoptr(GCabFolder) cabfolder = NULL;
    g_autoptr(GCabFolder) cabfolder_norecurse = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) gfile = NULL;
    g_autoptr(GSList) cabfiles = NULL;

    /* create GCabFile of a path */
    fn = gcab_test_get_filename ("src");
    g_assert (fn != NULL);
    gfile = g_file_new_for_path (fn);
    cabfile = gcab_file_new_with_file ("dirname", gfile);

    /* add entire subdir */
    cabfolder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    ret = gcab_folder_add_file (cabfolder, cabfile, TRUE, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);
    g_assert_cmpint (gcab_folder_get_nfiles (cabfolder), ==, 2);
    cabfiles = gcab_folder_get_files (cabfolder);
    cabfile_tmp = GCAB_FILE (cabfiles->data);
    g_assert (cabfile_tmp != cabfile);
    g_assert_cmpstr (gcab_file_get_name (cabfile_tmp), ==, "dirname\\test.txt");
    g_assert_cmpstr (gcab_file_get_extract_name (cabfile_tmp), ==, "dirname\\test.txt");

    /* a directory with recursion disabled is accepted but adds no files */
    cabfolder_norecurse = gcab_folder_new (GCAB_COMPRESSION_NONE);
    ret = gcab_folder_add_file (cabfolder_norecurse, cabfile, FALSE, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);
    g_assert_cmpuint (gcab_folder_get_nfiles (cabfolder_norecurse), ==, 0);

    /* add it again */
    ret = gcab_folder_add_file (cabfolder, cabfile, TRUE, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT);
    g_assert (!ret);
}

static void
gcab_test_cabinet_error_not_loaded_func (void)
{
    gboolean ret;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;

    /* we can't extract before loading */
    cabinet = gcab_cabinet_new ();
    file = g_file_new_for_path ("/tmp");
    ret = gcab_cabinet_extract_simple(cabinet, file, NULL, NULL, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_FAILED);
    g_assert (!ret);
}

static void
gcab_test_cabinet_error_no_folders_func (void)
{
    gboolean ret;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GOutputStream) op = NULL;

    /* we can't write with no folders */
    cabinet = gcab_cabinet_new ();
    file = g_file_new_for_path ("/tmp/foo.cab");
    op = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, 0, NULL, &error));
    g_assert_no_error (error);
    g_assert (op != NULL);
    ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_FAILED);
    g_assert (!ret);
}

static void
gcab_test_cabinet_func (void)
{
    GPtrArray *cabfolders;
    gboolean ret;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GCabFolder) folder = NULL;
    g_autoptr(GError) error = NULL;

    /* create cabinet */
    cabinet = gcab_cabinet_new ();
    g_assert_cmpint (gcab_cabinet_get_size (cabinet), ==, 0);

    /* add folder */
    folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    ret = gcab_cabinet_add_folder (cabinet, folder, &error);
    g_assert_no_error (error);
    g_assert (ret);

    /* add folder again */
    ret = gcab_cabinet_add_folder (cabinet, folder, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT);
    g_assert (!ret);
    cabfolders = gcab_cabinet_get_folders (cabinet);
    g_assert_cmpint (cabfolders->len, ==, 1);
}

static GBytes *
_gcab_test_write_cabinet (GCabCabinet *cabinet, GCabFileCallback file_callback,
                          gpointer user_data, GError **error)
{
    g_autoptr(GOutputStream) op = g_memory_output_stream_new_resizable ();

    if (!gcab_cabinet_write_simple (cabinet, op, file_callback, user_data,
                                    NULL, error))
        return NULL;
    if (!g_output_stream_close (op, NULL, error))
        return NULL;

    return g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (op));
}

static void
gcab_test_cabinet_error_multiple_folders_func (void)
{
    gboolean ret;
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
    g_autoptr(GCabFolder) folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    g_autoptr(GCabFolder) folder2 = gcab_folder_new (GCAB_COMPRESSION_NONE);
    g_autoptr(GError) error = NULL;
    g_autoptr(GOutputStream) op = g_memory_output_stream_new_resizable ();

    ret = gcab_cabinet_add_folder (cabinet, folder, &error);
    g_assert_no_error (error);
    g_assert_true (ret);
    ret = gcab_cabinet_add_folder (cabinet, folder2, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_reserved_func (void)
{
    const guint8 bad_magic[20] = { 0, };
    const guint8 bad_offset[20] = {
        0x00, 0x00, 0x10, 0x00,
        0xff, 0xff, 0xff, 0xff,
        0x04, 0x00, 0x00, 0x00,
    };
    const struct {
        const guint8 *data;
        GCabError expected;
    } tests[] = {
        { bad_magic, GCAB_ERROR_FORMAT },
        { bad_offset, GCAB_ERROR_INVALID_DATA },
    };

    for (guint i = 0; i < G_N_ELEMENTS (tests); i++) {
        gboolean ret;
        g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
        g_autoptr(GCabCabinet) loaded = gcab_cabinet_new ();
        g_autoptr(GCabFolder) folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
        g_autoptr(GBytes) file_bytes = g_bytes_new_static ("x", 1);
        g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes ("test.txt", file_bytes);
        g_autoptr(GByteArray) reserved = g_byte_array_new_take (g_memdup2 (tests[i].data, 20), 20);
        g_autoptr(GError) error = NULL;

        ret = gcab_cabinet_add_folder (cabinet, folder, &error);
        g_assert_no_error (error);
        g_assert_true (ret);
        ret = gcab_folder_add_file (folder, cabfile, FALSE, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        g_object_set (cabinet, "reserved", reserved, NULL);
        g_autoptr(GBytes) blob = _gcab_test_write_cabinet (cabinet, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert_nonnull (blob);

        g_autoptr(GInputStream) in = g_memory_input_stream_new_from_bytes (blob);
        ret = gcab_cabinet_load (loaded, in, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        g_autoptr(GError) signature_error = NULL;
        g_assert_null (gcab_cabinet_get_signature (loaded, NULL, &signature_error));
        g_assert_error (signature_error, GCAB_ERROR, (gint) tests[i].expected);
    }

    /* a cabinet without a reserved area has no signature either */
    g_autofree gchar *fn = gcab_test_get_filename ("test-none.cab");
    g_autoptr(GFile) file = g_file_new_for_path (fn);
    g_autoptr(GCabCabinet) loaded = gcab_cabinet_new ();
    g_autoptr(GError) error = NULL;

    g_assert_true (gcab_cabinet_load_from_file (loaded, file, NULL, &error));
    g_assert_no_error (error);
    g_autoptr(GError) signature_error = NULL;
    g_assert_null (gcab_cabinet_get_signature (loaded, NULL, &signature_error));
    g_assert_error (signature_error, GCAB_ERROR, GCAB_ERROR_FAILED);
}

static void
gcab_test_cabinet_error_not_seekable_func (void)
{
    const GByteArray *signature;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;

    /* verify it has no signature */
    cabinet = gcab_cabinet_new ();
    signature = gcab_cabinet_get_signature (cabinet, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED);
    g_assert (signature == NULL);
}

static gchar *
_compute_checksum_for_file (GFile *file, GError **error)
{
    g_autoptr(GBytes) blob = NULL;
    g_autoptr(GInputStream) in = NULL;
    in = G_INPUT_STREAM (g_file_read (file, NULL, error));
    if (in == NULL)
        return NULL;
    blob = g_input_stream_read_bytes (in, 1024, NULL, error);
    if (blob == NULL)
        return NULL;
    return g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, blob);
}

static void
gcab_test_cabinet_blob_func (void)
{
    gboolean ret;
    guint nfiles_cb = 0;
    g_autoptr(GError) error = NULL;
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();

    /* create folder and add to cabinet */
    g_autoptr(GCabFolder) folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    ret = gcab_cabinet_add_folder (cabinet, folder, &error);
    g_assert_no_error (error);
    g_assert (ret);

    /* add the two files */
    struct {
        const gchar *fn;
        const gchar *contents;
    } files[] = {
        { "test.sh",            "echo ola\n" },
        { "test.txt",           "Ola!\n" },
        { NULL,                 NULL }
    };
    for (guint i = 0; files[i].fn != NULL; i++) {
        g_autoptr(GBytes) bytes_tmp = g_bytes_new_static (files[i].contents,
                                                          strlen (files[i].contents));
        g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes (files[i].fn, bytes_tmp);

        /* set the time and attributes */
        g_autoptr(GDateTime) dt = g_date_time_new_utc (2017, 9, 15, 0, 0, 0.f);
        gcab_file_set_date_time (cabfile, dt);
        gcab_file_set_attributes (cabfile, GCAB_FILE_ATTRIBUTE_ARCH);

        /* add file to folder */
        ret = gcab_folder_add_file (folder, cabfile, FALSE, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);
    }

    /* write to a blob */
    g_autoptr(GOutputStream) op = g_memory_output_stream_new_resizable ();
    ret = gcab_cabinet_write_simple (cabinet, op, gcab_test_count_file_callback,
                                     &nfiles_cb, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);
    g_assert_cmpuint (nfiles_cb, ==, 2);

    /* get what the checksum is supposed to be */
    g_autofree gchar *fn = gcab_test_get_filename ("test-none.cab");
    g_assert (fn != NULL);
    g_autoptr(GFile) file = g_file_new_for_path (fn);
    g_assert (file != NULL);
    g_autofree gchar *checksum = _compute_checksum_for_file (file, &error);
    g_assert_no_error (error);
    g_assert (checksum != NULL);

    /* write for debugging */
    ret = g_output_stream_close (op, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);
    g_autoptr(GBytes) blob = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (op));
    ret = g_file_set_contents ("/tmp/test-none.cab", g_bytes_get_data (blob, NULL), g_bytes_get_size (blob), &error);
    g_assert_no_error (error);
    g_assert (ret);

    /* verify the checksum */
    g_autofree gchar *csum = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, blob);
    g_assert_cmpstr (csum, ==, checksum);
}

static void
gcab_test_cabinet_load_func (void)
{
    struct {
        const gchar *fn;
        guint32 size;
        GCabCompression comptype;
    } tests[] = {
        { "test-none.cab",          115,    GCAB_COMPRESSION_NONE },
        { "test-mszip.cab",         119,    GCAB_COMPRESSION_MSZIP },
        { "test-signed.cab",        139,    GCAB_COMPRESSION_NONE },
        { NULL,                     0,      0 }
    };

    for (guint i = 0; tests[i].fn != NULL; i++) {
        GCabFolder *cabfolder_tmp;
        GPtrArray *cabfolders;
        gboolean ret;
        guint nfiles_cb = 0;
        g_autofree gchar *fn = NULL;
        g_autoptr(GCabCabinet) cabinet = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFile) file_tmpdir = NULL;
        g_autoptr(GInputStream) in = NULL;
        struct {
            const gchar *fn;
            const gchar *checksum;
        } files[] = {
            { "test.sh",            "82b4415cf30efc9b5877e366475d652f263c0ced" },
            { "test.txt",           "decc67ff4a11acd93430cbb18c7bbddd00abf4fa" },
            { NULL,                 NULL }
        };

        /* read file */
        g_test_message ("%s... ", tests[i].fn);
        fn = gcab_test_get_filename (tests[i].fn);
        g_assert (fn != NULL);
        file = g_file_new_for_path (fn);
        in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
        g_assert_no_error (error);
        g_assert (in != NULL);
        cabinet = gcab_cabinet_new ();
        ret = gcab_cabinet_load (cabinet, in, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);

        /* check size */
        g_assert_cmpint (gcab_cabinet_get_size (cabinet), ==, tests[i].size);

        cabfolders = gcab_cabinet_get_folders (cabinet);
        g_assert (cabfolders != NULL);
        g_assert_cmpint (cabfolders->len, ==, 1);
        cabfolder_tmp = g_ptr_array_index (cabfolders, 0);
        g_assert_cmpint (gcab_folder_get_comptype (cabfolder_tmp), ==, tests[i].comptype);

        g_autoptr(GSList) cabfiles = gcab_folder_get_files (cabfolder_tmp);
        for (GSList *l = cabfiles; l != NULL; l = l->next) {
            GCabFile *cabfile = GCAB_FILE (l->data);
            g_assert_null (gcab_file_get_file (cabfile));
            g_assert_null (gcab_file_get_bytes (cabfile));
            g_assert_cmpint (gcab_file_get_attributes (cabfile), ==, GCAB_FILE_ATTRIBUTE_ARCH);
        }

        file_tmpdir = g_file_new_for_path ("/tmp");
        ret = gcab_cabinet_extract_simple (cabinet, file_tmpdir,
                                           gcab_test_count_file_callback,
                                           &nfiles_cb, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);
        g_assert_cmpuint (nfiles_cb, ==, 2);

        /* check files were decompressed okay */
        for (guint j = 0; files[j].fn != NULL; j++) {
            g_autofree gchar *fn_src = g_build_filename ("/tmp", files[j].fn, NULL);
            g_autoptr(GFile) file_dst = g_file_new_for_path (fn_src);
            g_autofree gchar *csum = _compute_checksum_for_file (file_dst, &error);
            g_assert_no_error (error);
            g_assert (csum != NULL);
            g_assert_cmpstr (csum, ==, files[j].checksum);
        }

        /* extract again to a memory blob on each GCabFile */
        ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);

        /* check each blob checksum */
        for (guint j = 0; files[j].fn != NULL; j++) {
            GCabFile *cabfile = gcab_folder_get_file_by_name (cabfolder_tmp, files[j].fn);
            g_assert_nonnull (cabfile);
            GBytes *blob = gcab_file_get_bytes (cabfile);
            g_assert_nonnull (blob);
            g_autofree gchar *csum = g_compute_checksum_for_bytes (G_CHECKSUM_SHA1, blob);
            g_assert_cmpstr (csum, ==, files[j].checksum);
        }
    }
}

static void
gcab_test_cabinet_error_cves_func (void)
{
    const gchar *tests[] = {
            "CVE-2014-9556.cab",
            "CVE-2014-9732.cab",
            "CVE-2015-4470.cab",
            "CVE-2015-4471.cab",
            NULL };
    for (guint i = 0; tests[i] != NULL; i++) {
        gboolean ret;
        g_autofree gchar *fn = NULL;
        g_autoptr(GCabCabinet) cabinet = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFile) file_tmpdir = NULL;
        g_autoptr(GInputStream) in = NULL;

        /* read file */
        g_test_message ("%s... ", tests[i]);
        fn = gcab_test_get_filename (tests[i]);
        g_assert (fn != NULL);
        file = g_file_new_for_path (fn);
        in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
        g_assert_no_error (error);
        g_assert (in != NULL);
        cabinet = gcab_cabinet_new ();
        ret = gcab_cabinet_load (cabinet, in, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);
        file_tmpdir = g_file_new_for_path ("/tmp");
        ret = gcab_cabinet_extract_simple (cabinet, file_tmpdir, NULL, NULL, NULL, &error);
        g_assert (error != NULL);
        g_assert (!ret);
    }
}

static void
gcab_test_cabinet_allowed_compression_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* load file */
    fn = gcab_test_get_filename ("test-mszip.cab");
    g_assert (fn != NULL);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert (in != NULL);

    /* add the one it is not for a failure */
    cabinet = gcab_cabinet_new ();
    gcab_cabinet_add_allowed_compression (cabinet, GCAB_COMPRESSION_LZX);
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED);
    g_assert_false (ret);
    g_clear_error (&error);
    g_clear_object (&in);

    /* add the correct one and try again */
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert (in != NULL);
    gcab_cabinet_add_allowed_compression (cabinet, GCAB_COMPRESSION_MSZIP);
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);
}

static void
gcab_test_cabinet_signature_func (void)
{
    const GByteArray *sig;
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* load file */
    fn = gcab_test_get_filename ("test-signed.cab");
    g_assert (fn != NULL);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert (in != NULL);
    cabinet = gcab_cabinet_new ();
    gcab_cabinet_add_allowed_compression (cabinet, GCAB_COMPRESSION_NONE);
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);

    /* get signature */
    sig = gcab_cabinet_get_signature (cabinet, NULL, &error);
    g_assert_no_error (error);
    g_assert (sig != NULL);
    g_assert_cmpint (sig->len, ==, 2040);
}

static void
gcab_test_cabinet_write_func (void)
{
    struct {
        const gchar *fn;
        guint32 size;
        GCabCompression comptype;
    } tests[] = {
        { "test-none.cab",          115,    GCAB_COMPRESSION_NONE },
        { "test-mszip.cab",         119,    GCAB_COMPRESSION_MSZIP },
        { NULL,                     0,      0 }
    };

    for (guint i = 0; tests[i].fn != NULL; i++) {
        gboolean ret;
        g_autofree gchar *checksum_in = NULL;
        g_autofree gchar *checksum_out = NULL;
        g_autofree gchar *fn_in = NULL;
        g_autofree gchar *fn_out = NULL;
        g_autoptr(GCabCabinet) cabinet = NULL;
        g_autoptr(GCabFolder) cabfolder = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) file_in = NULL;
        g_autoptr(GFile) file_out = NULL;
        g_autoptr(GOutputStream) op = NULL;
        struct {
            const gchar *fn;
            const gchar *checksum;
        } files[] = {
            { "test.sh",            "82b4415cf30efc9b5877e366475d652f263c0ced" },
            { "test.txt",           "decc67ff4a11acd93430cbb18c7bbddd00abf4fa" },
            { NULL,                 NULL }
        };

        /* create new cabinet with a single folder of the correct type */
        cabinet = gcab_cabinet_new ();
        cabfolder = gcab_folder_new (tests[i].comptype);
        ret = gcab_cabinet_add_folder (cabinet, cabfolder, &error);
        g_assert_no_error (error);
        g_assert (ret);

        /* add new files to a folder */
        for (guint j = 0; files[j].fn != NULL; j++) {
            g_autofree gchar *fn_src = g_build_filename ("src", files[j].fn, NULL);
            g_autofree gchar *fn_tmp = gcab_test_get_filename (fn_src);
            g_autoptr(GFile) file = g_file_new_for_path (fn_tmp);
            g_autoptr(GCabFile) cabfile = gcab_file_new_with_file (files[j].fn, file);
            g_autoptr(GDateTime) dt = NULL;

            /* set the time so the checksums match */
            dt = g_date_time_new_utc (2017, 9, 15, 0, 0, 0.f);
            gcab_file_set_date_time (cabfile, dt);

            ret = gcab_folder_add_file (cabfolder, cabfile, FALSE, NULL, &error);
            g_assert_no_error (error);
            g_assert (ret);
        }

        /* write the file */
        g_test_message ("%s... ", tests[i].fn);
        fn_out = g_build_filename ("/tmp", tests[i].fn, NULL);
        file_out = g_file_new_for_path (fn_out);
        op = G_OUTPUT_STREAM (g_file_replace (file_out, NULL, FALSE, 0, NULL, &error));
        g_assert_no_error (error);
        g_assert (op != NULL);
        ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);

        /* check size */
        g_assert_cmpint (gcab_cabinet_get_size (cabinet), ==, tests[i].size);

        /* compare checksums */
        fn_in = gcab_test_get_filename (tests[i].fn);
        g_assert (fn_in != NULL);
        file_in = g_file_new_for_path (fn_in);
        checksum_in = _compute_checksum_for_file (file_in, &error);
        g_assert_no_error (error);
        g_assert (checksum_in != NULL);
        checksum_out = _compute_checksum_for_file (file_out, &error);
        g_assert_no_error (error);
        g_assert (checksum_out != NULL);
        g_assert_cmpstr (checksum_in, ==, checksum_out);
    }
}

static void
gcab_test_cabinet_multicab_func (void)
{
    const gchar expected[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    GBytes *bytes;
    GPtrArray *folders;
    GCabFolder *folder;
    GCabFile *cabfile;

    fn = gcab_test_get_filename ("test-multicab-a.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    cabinet = gcab_cabinet_new ();

    ret = gcab_cabinet_load_from_file (cabinet, file, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);
    g_assert_cmpstr (gcab_cabinet_get_next_name (cabinet), ==,
                     "test-multicab-b.cab");

    /* extract to memory; the file spans three cabinets */
    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    folders = gcab_cabinet_get_folders (cabinet);
    g_assert_cmpuint (folders->len, ==, 1);
    folder = g_ptr_array_index (folders, 0);
    cabfile = gcab_folder_get_file_by_name (folder, "file.bin");
    g_assert_nonnull (cabfile);

    bytes = gcab_file_get_bytes (cabfile);
    g_assert_nonnull (bytes);
    g_assert_cmpmem (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
                     expected, sizeof (expected) - 1);
}

static void
gcab_test_cabinet_multicab_prev_func (void)
{
    const gchar expected[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    GCabCabinet *prev;
    GCabCabinet *next;
    GBytes *bytes;
    GPtrArray *folders;
    GCabFolder *folder;
    GCabFile *cabfile;

    fn = gcab_test_get_filename ("test-multicab-b.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    cabinet = gcab_cabinet_new ();

    ret = gcab_cabinet_load_from_file (cabinet, file, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);
    g_assert_cmpstr (gcab_cabinet_get_prev_name (cabinet), ==,
                     "test-multicab-a.cab");
    g_assert_cmpstr (gcab_cabinet_get_next_name (cabinet), ==,
                     "test-multicab-c.cab");

    prev = gcab_cabinet_get_prev (cabinet);
    next = gcab_cabinet_get_next (cabinet);
    g_assert_nonnull (prev);
    g_assert_nonnull (next);
    g_assert_true (gcab_cabinet_get_next (prev) == cabinet);
    g_assert_true (gcab_cabinet_get_prev (next) == cabinet);

    /* extracting from the middle cabinet should still extract the whole set */
    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    /* the file begins in the previous cabinet */
    folders = gcab_cabinet_get_folders (prev);
    g_assert_cmpuint (folders->len, ==, 1);
    folder = g_ptr_array_index (folders, 0);
    cabfile = gcab_folder_get_file_by_name (folder, "file.bin");
    g_assert_nonnull (cabfile);

    bytes = gcab_file_get_bytes (cabfile);
    g_assert_nonnull (bytes);
    g_assert_cmpmem (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
                     expected, sizeof (expected) - 1);
}

static void
gcab_test_cabinet_none_size_mismatch_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* an uncompressed block shorter than its output size would expose stale
     * bytes from the previous block */
    fn = gcab_test_get_filename ("test-none-size-mismatch.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert_nonnull (in);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_extract_bomb_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;

    /* many file entries share the same compressed block.  Extraction must
     * reject the archive before materialising all of the duplicated output. */
    fn = gcab_test_get_filename ("test-extract-bomb.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load_from_file (cabinet, file, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    g_assert_cmpuint (gcab_cabinet_get_extract_size_limit (cabinet), ==, (guint64) 4 << 30);
    g_assert_cmpuint (gcab_cabinet_get_extract_ratio_limit (cabinet), ==, 1024);

    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_mszip_short_cdata_func (void)
{
    gboolean ret;
    GLogLevelFlags old_mask;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* a one-byte CDATA part leaves the CK bytes from the previous block in
     * place and must not underflow the MSZIP input size */
    fn = gcab_test_get_filename ("test-mszip-short-cdata.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert_nonnull (in);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    /* the rejected block must not reach the avail_in underflow warning */
    old_mask = g_log_set_fatal_mask ("gcab",
                                     G_LOG_LEVEL_ERROR |
                                     G_LOG_LEVEL_CRITICAL |
                                     G_LOG_LEVEL_WARNING);
    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_log_set_fatal_mask ("gcab", old_mask);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_lzx_frame_wrap_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* a short frame followed by a full one must not wrap the window and make
     * the decompressor copy from before the window buffer */
    fn = gcab_test_get_filename ("test-lzx-frame-wrap.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert_nonnull (in);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_lzx_offset_func (void)
{
    const gchar *tests[] = {
        "test-lzx-offset-overflow.cab",
        "test-lzx-offset-before-data.cab",
        NULL
    };

    for (guint i = 0; tests[i] != NULL; i++) {
        gboolean ret;
        g_autofree gchar *fn = NULL;
        g_autoptr(GCabCabinet) cabinet = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GInputStream) in = NULL;

        /* offsets from the uncompressed block header must not point outside
         * the window or before the start of the decoded stream */
        fn = gcab_test_get_filename (tests[i]);
        g_assert_nonnull (fn);
        file = g_file_new_for_path (fn);
        in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
        g_assert_no_error (error);
        g_assert_nonnull (in);
        cabinet = gcab_cabinet_new ();
        ret = gcab_cabinet_load (cabinet, in, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
        g_assert_nonnull (error);
        g_assert_false (ret);
    }
}

static void
gcab_test_cabinet_lzx_truncated_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* the compressed stream ends in the middle of a run and must not be
     * satisfied by reading past the input buffer */
    fn = gcab_test_get_filename ("test-lzx-truncated.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert_nonnull (in);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_cabinet_lzx_overrun_func (void)
{
    gboolean ret;
    g_autofree gchar *fn = NULL;
    g_autoptr(GCabCabinet) cabinet = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = NULL;
    g_autoptr(GInputStream) in = NULL;

    /* a match at the end of the 32K window makes the decompressor write
     * beyond the end of the allocated window buffer */
    fn = gcab_test_get_filename ("test-lzx-overrun.cab");
    g_assert_nonnull (fn);
    file = g_file_new_for_path (fn);
    in = G_INPUT_STREAM (g_file_read (file, NULL, &error));
    g_assert_no_error (error);
    g_assert_nonnull (in);
    cabinet = gcab_cabinet_new ();
    ret = gcab_cabinet_load (cabinet, in, NULL, &error);
    g_assert_no_error (error);
    g_assert_true (ret);

    ret = gcab_cabinet_extract_simple (cabinet, NULL, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
}

static void
gcab_test_folder_cdata_range_func (void)
{
    gsize offset = 0;
    gsize count = 0;
    const guint64 beyond_32bit = (guint64) G_MAXUINT32 + 1;

    /* normal range */
    g_assert_true (_gcab_folder_get_cdata_range (0, 0, 10, 10, &offset, &count));
    g_assert_cmpuint (offset, ==, 0);
    g_assert_cmpuint (count, ==, 10);

    /* file starts partway through the block */
    g_assert_true (_gcab_folder_get_cdata_range (0, 5, 10, 10, &offset, &count));
    g_assert_cmpuint (offset, ==, 5);
    g_assert_cmpuint (count, ==, 5);

    /* counters must not wrap at 32 bits */
    g_assert_true (_gcab_folder_get_cdata_range (beyond_32bit, beyond_32bit + 2,
                                                 10, 10, &offset, &count));
    g_assert_cmpuint (offset, ==, 2);
    g_assert_cmpuint (count, ==, 8);

    /* offset at or beyond the end of the CDATA block is invalid */
    g_assert_false (_gcab_folder_get_cdata_range (0, 10, 1, 10, &offset, &count));
    g_assert_false (_gcab_folder_get_cdata_range (beyond_32bit, beyond_32bit + 10,
                                                  1, 10, &offset, &count));

    /* offsets before the current block are invalid */
    g_assert_false (_gcab_folder_get_cdata_range (10, 5, 1, 10, &offset, &count));
}

#ifndef G_OS_WIN32
static GCabCabinet *
_gcab_test_cabinet_new_with_file_name (const gchar *name, GError **error)
{
    gboolean ret;
    g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
    g_autoptr(GCabFolder) folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
    g_autoptr(GBytes) bytes = g_bytes_new_static ("x", 1);
    g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes (name, bytes);
    g_autoptr(GOutputStream) op = g_memory_output_stream_new_resizable ();

    ret = gcab_cabinet_add_folder (cabinet, folder, error);
    if (!ret)
        return NULL;
    ret = gcab_folder_add_file (folder, cabfile, FALSE, NULL, error);
    if (!ret)
        return NULL;
    ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, error);
    if (!ret)
        return NULL;
    ret = g_output_stream_close (op, NULL, error);
    if (!ret)
        return NULL;

    g_autoptr(GBytes) blob =
        g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (op));
    g_autoptr(GInputStream) in = g_memory_input_stream_new_from_bytes (blob);
    GCabCabinet *loaded = gcab_cabinet_new ();
    if (!gcab_cabinet_load (loaded, in, NULL, error)) {
        g_object_unref (loaded);
        return NULL;
    }
    return loaded;
}
#endif

static void
gcab_test_cabinet_path_traversal_func (void)
{
    const gchar *tests[] = {
        "C:\\evil.txt",
        "\\\\server\\share\\evil.txt",
        "\\\\?\\C:\\evil.txt",
        "../evil.txt",
        "a/../../evil.txt",
        "/evil.txt",
        "",
        "/",
        "///",
        NULL
    };
    g_autoptr(GError) error = NULL;
    g_autofree gchar *tmpdir = g_dir_make_tmp ("gcab-test-XXXXXX", &error);
    g_assert_no_error (error);
    g_assert_nonnull (tmpdir);
    g_autoptr(GFile) path_extract = g_file_new_for_path (tmpdir);

    for (guint i = 0; tests[i] != NULL; i++) {
        gboolean ret;
        g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
        g_autoptr(GCabCabinet) loaded = gcab_cabinet_new ();
        g_autoptr(GCabFolder) folder = gcab_folder_new (GCAB_COMPRESSION_NONE);
        g_autoptr(GBytes) bytes = g_bytes_new_static ("x", 1);
        g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes (tests[i], bytes);
        g_autoptr(GOutputStream) op = g_memory_output_stream_new_resizable ();

        ret = gcab_cabinet_add_folder (cabinet, folder, &error);
        g_assert_no_error (error);
        g_assert_true (ret);
        ret = gcab_folder_add_file (folder, cabfile, FALSE, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);
        ret = g_output_stream_close (op, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        g_autoptr(GBytes) blob =
            g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (op));
        g_autoptr(GInputStream) in = g_memory_input_stream_new_from_bytes (blob);
        ret = gcab_cabinet_load (loaded, in, NULL, &error);
        g_assert_no_error (error);
        g_assert_true (ret);

        ret = gcab_cabinet_extract_simple (loaded, path_extract, NULL, NULL,
                                           NULL, &error);
        g_assert_nonnull (error);
        g_assert_false (ret);
        g_clear_error (&error);
    }

    g_file_delete (path_extract, NULL, NULL);
}

#ifndef G_OS_WIN32
static void
gcab_test_cabinet_symlink_func (void)
{
    gboolean ret;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *root = g_dir_make_tmp ("gcab-root-XXXXXX", &error);
    g_assert_no_error (error);
    g_assert_nonnull (root);
    g_autofree gchar *outside = g_dir_make_tmp ("gcab-outside-XXXXXX", &error);
    g_assert_no_error (error);
    g_assert_nonnull (outside);
    g_autoptr(GFile) path_extract = g_file_new_for_path (root);

    /* a directory component that is a symlink must not be followed */
    g_autofree gchar *link_path = g_build_filename (root, "link", NULL);
    g_autofree gchar *outside_file = g_build_filename (outside, "evil.txt", NULL);
    g_assert_cmpint (symlink (outside, link_path), ==, 0);

    g_autoptr(GCabCabinet) cabinet = _gcab_test_cabinet_new_with_file_name ("link/evil.txt", &error);
    g_assert_no_error (error);
    g_assert_nonnull (cabinet);
    ret = gcab_cabinet_extract_simple (cabinet, path_extract, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
    g_assert_false (g_file_test (outside_file, G_FILE_TEST_EXISTS));
    g_clear_error (&error);

    /* a final component that is a symlink must not be replaced */
    g_autofree gchar *final_path = g_build_filename (root, "final", NULL);
    g_autofree gchar *outside_target = g_build_filename (outside, "target.txt", NULL);
    g_assert_cmpint (symlink (outside_target, final_path), ==, 0);

    g_autoptr(GCabCabinet) cabinet_final = _gcab_test_cabinet_new_with_file_name ("final", &error);
    g_assert_no_error (error);
    g_assert_nonnull (cabinet_final);
    ret = gcab_cabinet_extract_simple (cabinet_final, path_extract, NULL, NULL, NULL, &error);
    g_assert_nonnull (error);
    g_assert_false (ret);
    g_assert_false (g_file_test (outside_target, G_FILE_TEST_EXISTS));

    (void)unlink (link_path);
    (void)unlink (final_path);
    (void)rmdir (root);
    (void)rmdir (outside);
}
#endif

static GBytes *
_gcab_test_make_bytes (gsize size, guint seed, gboolean compressible)
{
    guint32 state = seed;
    guint8 *data = g_malloc (size);

    for (gsize i = 0; i < size; i++) {
        state = state * 1664525u + 1013904223u;
        if (compressible && (i % 16) < 12)
            data[i] = "gcab-test"[i % 9];
        else
            data[i] = (state >> 24) & 0xff;
    }

    return g_bytes_new_take (data, size);
}

static void
gcab_test_cabinet_roundtrip_func (void)
{
    const gsize sizes[] = {
        0,
        1,
        7,
        CAB_MAX_BLOCK_SIZE - 1,
        CAB_MAX_BLOCK_SIZE,
        CAB_MAX_BLOCK_SIZE + 1,
        2 * CAB_MAX_BLOCK_SIZE + 123,
    };
    const gint comptypes[] = {
        GCAB_COMPRESSION_NONE,
        GCAB_COMPRESSION_MSZIP,
    };

    for (guint c = 0; c < G_N_ELEMENTS (comptypes); c++) {
        for (guint s = 0; s < G_N_ELEMENTS (sizes); s++) {
            gboolean ret;
            gsize contents_len = 0;
            g_autofree gchar *contents = NULL;
            g_autofree gchar *tmpdir = NULL;
            g_autofree gchar *extracted = NULL;
            g_autoptr(GCabCabinet) cabinet = gcab_cabinet_new ();
            g_autoptr(GCabCabinet) loaded = gcab_cabinet_new ();
            g_autoptr(GCabFolder) folder = gcab_folder_new (comptypes[c]);
            g_autoptr(GBytes) bytes = _gcab_test_make_bytes (sizes[s], s + 1, TRUE);
            g_autoptr(GCabFile) cabfile = gcab_file_new_with_bytes ("roundtrip.bin", bytes);
            g_autoptr(GDateTime) date = g_date_time_new_utc (2020, 2, 3, 4, 5, 6);
            g_autoptr(GError) error = NULL;
            g_autoptr(GFile) path_extract = NULL;
            g_autoptr(GInputStream) in = NULL;

            g_test_message ("comptype %d, size %" G_GSIZE_FORMAT, comptypes[c], sizes[s]);

            gcab_file_set_date_time (cabfile, date);
            gcab_file_set_attributes (cabfile, GCAB_FILE_ATTRIBUTE_RDONLY |
                                               GCAB_FILE_ATTRIBUTE_ARCH);

            ret = gcab_cabinet_add_folder (cabinet, folder, &error);
            g_assert_no_error (error);
            g_assert_true (ret);
            ret = gcab_folder_add_file (folder, cabfile, FALSE, NULL, &error);
            g_assert_no_error (error);
            g_assert_true (ret);

            g_autoptr(GBytes) blob = _gcab_test_write_cabinet (cabinet, NULL, NULL, &error);
            g_assert_no_error (error);
            g_assert_nonnull (blob);
            g_assert_cmpuint (gcab_cabinet_get_size (cabinet), ==, g_bytes_get_size (blob));

            in = g_memory_input_stream_new_from_bytes (blob);
            ret = gcab_cabinet_load (loaded, in, NULL, &error);
            g_assert_no_error (error);
            g_assert_true (ret);

            GPtrArray *folders = gcab_cabinet_get_folders (loaded);
            g_assert_cmpuint (folders->len, ==, 1);
            g_autoptr(GSList) files = gcab_folder_get_files (g_ptr_array_index (folders, 0));
            g_assert_cmpuint (g_slist_length (files), ==, 1);
            GCabFile *file_out = files->data;

            g_assert_cmpstr (gcab_file_get_name (file_out), ==, "roundtrip.bin");
            g_assert_cmpuint (gcab_file_get_size (file_out), ==, sizes[s]);
            g_assert_cmpuint (gcab_file_get_attributes (file_out), ==,
                              GCAB_FILE_ATTRIBUTE_RDONLY | GCAB_FILE_ATTRIBUTE_ARCH);
            g_autoptr(GDateTime) date_out = gcab_file_get_date_time (file_out);
            g_assert_nonnull (date_out);
            g_assert_cmpint (g_date_time_to_unix (date_out), ==, g_date_time_to_unix (date));

            /* decompress to memory */
            ret = gcab_cabinet_extract_simple (loaded, NULL, NULL, NULL, NULL, &error);
            g_assert_no_error (error);
            g_assert_true (ret);
            GBytes *bytes_out = gcab_file_get_bytes (file_out);
            g_assert_nonnull (bytes_out);
            g_assert_true (g_bytes_equal (bytes_out, bytes));

            /* and to a directory, which exercises the path handling too */
            tmpdir = g_dir_make_tmp ("gcab-roundtrip-XXXXXX", &error);
            g_assert_no_error (error);
            g_assert_nonnull (tmpdir);
            path_extract = g_file_new_for_path (tmpdir);
            ret = gcab_cabinet_extract_simple (loaded, path_extract, NULL, NULL, NULL, &error);
            g_assert_no_error (error);
            g_assert_true (ret);
            extracted = g_build_filename (tmpdir, "roundtrip.bin", NULL);
            g_assert_true (g_file_get_contents (extracted, &contents, &contents_len, &error));
            g_assert_no_error (error);
            g_assert_cmpuint (contents_len, ==, sizes[s]);
            g_assert_true (contents_len == 0 ||
                           memcmp (contents, g_bytes_get_data (bytes, NULL), contents_len) == 0);

            g_autoptr(GFile) extracted_file = g_file_new_for_path (extracted);
            g_assert_true (g_file_delete (extracted_file, NULL, &error));
            g_assert_no_error (error);
            g_assert_true (g_file_delete (path_extract, NULL, &error));
            g_assert_no_error (error);
        }
    }
}

gint
main (gint argc, gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    /* only critical and error are fatal */
    g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
    (void)g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

    /* tests go here */
    g_test_add_func ("/GCab/file", gcab_test_file_func);
    g_test_add_func ("/GCab/file{properties}", _gcab_test_file_properties_func);
    g_test_add_func ("/GCab/file{clear-bytes}", _gcab_test_file_clear_bytes_func);
    g_test_add_func ("/GCab/folder", gcab_test_folder_func);
    g_test_add_func ("/GCab/folder{properties}", _gcab_test_folder_properties_func);
    g_test_add_func ("/GCab/folder{path}", gcab_test_folder_path_func);
    g_test_add_func ("/GCab/folder{cdata-range}", gcab_test_folder_cdata_range_func);
    g_test_add_func ("/GCab/cabinet", gcab_test_cabinet_func);
    g_test_add_func ("/GCab/cabinet{properties}", _gcab_test_cabinet_properties_func);
    g_test_add_func ("/GCab/cabinet{error-multiple-folders}", gcab_test_cabinet_error_multiple_folders_func);
    g_test_add_func ("/GCab/cabinet{reserved}", gcab_test_cabinet_reserved_func);
    g_test_add_func ("/GCab/cabinet{error-not-seekable}", gcab_test_cabinet_error_not_seekable_func);
    g_test_add_func ("/GCab/cabinet{error-not-loaded}", gcab_test_cabinet_error_not_loaded_func);
    g_test_add_func ("/GCab/cabinet{error-no-folders}", gcab_test_cabinet_error_no_folders_func);
    g_test_add_func ("/GCab/cabinet{error-cves}", gcab_test_cabinet_error_cves_func);
    g_test_add_func ("/GCab/cabinet{lzx-overrun}", gcab_test_cabinet_lzx_overrun_func);
    g_test_add_func ("/GCab/cabinet{lzx-truncated}", gcab_test_cabinet_lzx_truncated_func);
    g_test_add_func ("/GCab/cabinet{lzx-offsets}", gcab_test_cabinet_lzx_offset_func);
    g_test_add_func ("/GCab/cabinet{lzx-frame-wrap}", gcab_test_cabinet_lzx_frame_wrap_func);
    g_test_add_func ("/GCab/cabinet{mszip-short-cdata}", gcab_test_cabinet_mszip_short_cdata_func);
    g_test_add_func ("/GCab/cabinet{extract-bomb}", gcab_test_cabinet_extract_bomb_func);
    g_test_add_func ("/GCab/cabinet{none-size-mismatch}", gcab_test_cabinet_none_size_mismatch_func);
    g_test_add_func ("/GCab/cabinet{load}", gcab_test_cabinet_load_func);
    g_test_add_func ("/GCab/cabinet{multicab}", gcab_test_cabinet_multicab_func);
    g_test_add_func ("/GCab/cabinet{multicab-prev}", gcab_test_cabinet_multicab_prev_func);
    g_test_add_func ("/GCab/cabinet{write}", gcab_test_cabinet_write_func);
    g_test_add_func ("/GCab/cabinet{blob}", gcab_test_cabinet_blob_func);
    g_test_add_func ("/GCab/cabinet{roundtrip}", gcab_test_cabinet_roundtrip_func);
    g_test_add_func ("/GCab/cabinet{signature}", gcab_test_cabinet_signature_func);
    g_test_add_func ("/GCab/cabinet{allowed-compression}", gcab_test_cabinet_allowed_compression_func);
    g_test_add_func ("/GCab/cabinet{path-traversal}", gcab_test_cabinet_path_traversal_func);
#ifndef G_OS_WIN32
    g_test_add_func ("/GCab/cabinet{symlink}", gcab_test_cabinet_symlink_func);
#endif
    return g_test_run ();
}
