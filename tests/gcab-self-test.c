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

static gchar *
gcab_test_get_filename (const gchar *filename)
{
    return g_test_build_filename (G_TEST_DIST, filename, NULL);
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
    ret = gcab_cabinet_write_simple (cabinet, op, NULL, NULL, NULL, &error);
    g_assert_no_error (error);
    g_assert (ret);

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
        ret = gcab_cabinet_extract_simple (cabinet, file_tmpdir, NULL, NULL, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);

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
    (void)g_setenv ("GCAB_SKIP_CHECKSUM", "1", TRUE);
    (void)g_setenv ("GCAB_SKIP_COMPRESSION_CHECK", "1", TRUE);
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
        gcab_cabinet_add_allowed_compression (cabinet, GCAB_COMPRESSION_NONE);
        ret = gcab_cabinet_load (cabinet, in, NULL, &error);
        g_assert_no_error (error);
        g_assert (ret);
        file_tmpdir = g_file_new_for_path ("/tmp");
        ret = gcab_cabinet_extract_simple (cabinet, file_tmpdir, NULL, NULL, NULL, &error);
        g_assert (error != NULL);
        g_assert (!ret);
    }
    g_unsetenv ("GCAB_SKIP_CHECKSUM");
    g_unsetenv ("GCAB_SKIP_COMPRESSION_CHECK");
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

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    /* only critical and error are fatal */
    g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
    (void)g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

    /* tests go here */
    g_test_add_func ("/GCab/file", gcab_test_file_func);
    g_test_add_func ("/GCab/folder", gcab_test_folder_func);
    g_test_add_func ("/GCab/folder{path}", gcab_test_folder_path_func);
    g_test_add_func ("/GCab/cabinet", gcab_test_cabinet_func);
    g_test_add_func ("/GCab/cabinet{error-not-seekable}", gcab_test_cabinet_error_not_seekable_func);
    g_test_add_func ("/GCab/cabinet{error-not-loaded}", gcab_test_cabinet_error_not_loaded_func);
    g_test_add_func ("/GCab/cabinet{error-no-folders}", gcab_test_cabinet_error_no_folders_func);
    g_test_add_func ("/GCab/cabinet{error-cves}", gcab_test_cabinet_error_cves_func);
    g_test_add_func ("/GCab/cabinet{load}", gcab_test_cabinet_load_func);
    g_test_add_func ("/GCab/cabinet{write}", gcab_test_cabinet_write_func);
    g_test_add_func ("/GCab/cabinet{blob}", gcab_test_cabinet_blob_func);
    g_test_add_func ("/GCab/cabinet{signature}", gcab_test_cabinet_signature_func);
    g_test_add_func ("/GCab/cabinet{allowed-compression}", gcab_test_cabinet_allowed_compression_func);
    return g_test_run ();
}
