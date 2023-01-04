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

#include <glib/gi18n-lib.h>
#include "gcab-priv.h"

/**
 * SECTION:gcab-cabinet
 * @title: GCabCabinet
 * @short_description: Cabinet archive file operations
 * @see_also: #GCabFolder
 * @stability: Stable
 * @include: libgcab.h
 *
 * A GCabCabinet is a handle to a Cabinet archive. It allows examining,
 * extracting and creation of archives.
 */

struct _GCabCabinet {
    GObject parent_instance;

    GPtrArray *folders;
    GByteArray *reserved;
    cheader_t *cheader;
    GByteArray *signature;
    GInputStream *stream;
    guint8 allowed_compression;
};

enum {
    PROP_0,

    PROP_RESERVED,
    PROP_SIGNATURE,
};

G_DEFINE_TYPE (GCabCabinet, gcab_cabinet, G_TYPE_OBJECT);

GQuark
gcab_error_quark (void)
{
    return g_quark_from_static_string ("gcab-error-quark");
}

static void
gcab_cabinet_init (GCabCabinet *self)
{
    self->allowed_compression = GCAB_COMPRESSION_MASK;
    self->folders = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gcab_cabinet_finalize (GObject *object)
{
    GCabCabinet *self = GCAB_CABINET (object);

    cheader_free (self->cheader);
    g_ptr_array_unref (self->folders);
    if (self->reserved)
        g_byte_array_unref (self->reserved);
    if (self->signature)
        g_byte_array_unref (self->signature);
    if (self->stream)
        g_object_unref (self->stream);

    G_OBJECT_CLASS (gcab_cabinet_parent_class)->finalize (object);
}

static void
gcab_cabinet_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_CABINET (object));
    GCabCabinet *self = GCAB_CABINET (object);

    switch (prop_id) {
    case PROP_RESERVED:
        if (self->reserved)
            g_byte_array_unref (self->reserved);
        self->reserved = g_value_dup_boxed (value);
        break;
    case PROP_SIGNATURE:
        if (self->signature)
            g_byte_array_unref (self->signature);
        self->signature = g_value_dup_boxed (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_cabinet_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_CABINET (object));
    GCabCabinet *self = GCAB_CABINET (object);

    switch (prop_id) {
    case PROP_RESERVED:
        g_value_set_boxed (value, self->reserved);
        break;
    case PROP_SIGNATURE:
        g_value_set_boxed (value, gcab_cabinet_get_signature (self, NULL, NULL));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_cabinet_class_init (GCabCabinetClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    object_class->finalize = gcab_cabinet_finalize;
    object_class->set_property = gcab_cabinet_set_property;
    object_class->get_property = gcab_cabinet_get_property;

    g_object_class_install_property (object_class, PROP_RESERVED,
         g_param_spec_boxed ("reserved", "Reserved", "Reserved",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_SIGNATURE,
         g_param_spec_boxed ("signature", "Signature", "Signature",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gcab_cabinet_add_folder:
 * @cabinet: a #GCabCabinet
 * @folder: a #GCabFolder
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Add @folder to @cabinet.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_cabinet_add_folder (GCabCabinet *self,
                         GCabFolder *folder,
                         GError **error)
{
    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (GCAB_IS_FOLDER (folder), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    for (guint i = 0; i < self->folders->len; i++) {
        GCabFolder *folder_tmp = g_ptr_array_index (self->folders, i);
        if (folder_tmp == folder) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                         "Folder has already been added");
            return FALSE;
        }
    }

    g_ptr_array_add (self->folders, g_object_ref (folder));

    return TRUE;
}

/**
 * gcab_cabinet_get_folders:
 * @cabinet:a #GCabCabinet
 *
 * Get the Cabinet folders within the @cabinet.
 * Note that Cabinet folders are not like filesystem path, they are
 * group of files sharing some layout parameters.
 *
 * Returns: (element-type GCabFolder) (transfer none): an array of #GCabFolder
 **/
GPtrArray *
gcab_cabinet_get_folders (GCabCabinet *self)
{
    g_return_val_if_fail (GCAB_IS_CABINET (self), NULL);

    return self->folders;
}

/**
 * gcab_cabinet_get_size:
 * @cabinet:a #GCabCabinet
 *
 * Get the size of the compressed cabinet file.
 *
 * Returns: size in bytes
 *
 * Since: 1.0
 **/
guint32
gcab_cabinet_get_size (GCabCabinet *self)
{
    if (self->cheader == NULL)
        return 0;
    return self->cheader->size;
}

/**
 * gcab_cabinet_write:
 * @cabinet: a #GCabCabinet
 * @stream: a #GOutputStream also #GSeekable
 * @file_callback: (allow-none) (scope call) (closure user_data): report current file being saved
 * @progress_callback: (allow-none) (scope call) (closure user_data): report saving progress
 * @user_data: (closure): user data to pass to callbacks
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Save @cabinet to the output stream @out. @out must be a #GSeekable.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_cabinet_write (GCabCabinet *self,
                    GOutputStream *out,
                    GCabFileCallback file_callback,
                    GFileProgressCallback progress_callback,
                    gpointer user_data,
                    GCancellable *cancellable,
                    GError **error)
{
    g_autoptr(cheader_t) cheader = g_new0 (cheader_t, 1);
    cfolder_t folder = { 0, };

    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (out), FALSE);
    g_return_val_if_fail (G_IS_SEEKABLE (out), FALSE);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    /* FIXME: support more than one folder */
    cheader->offsetfiles = CFI_START; // CFHEADER + 1 * CFFOLDER
    cheader->nfolders = 1; // a single CAB folder is enough

    /* nothing to do */
    if (self->folders->len == 0) {
        g_set_error_literal (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                             "Cabinet has no added folders");
        return FALSE;
    }

    /* unsupported */
    if (self->folders->len > 1) {
        g_set_error_literal (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED,
                             "Cabinet has more than one added folder");
        return FALSE;
    }

    GCabFolder *cabfolder = g_ptr_array_index (self->folders, 0);
    gsize nfiles = gcab_folder_get_nfiles (cabfolder);
    g_autoptr(GDataOutputStream) dstream = NULL;
    gssize len, offset = 0;
    cdata_t block = { 0, };
    guint8 data[CAB_MAX_BLOCK_SIZE];
    gsize written;
    size_t sumstr = 0;
    g_autoptr(GSList) files = NULL;
    GCabFile *prevf = NULL;

    dstream = g_data_output_stream_new (out);
    g_data_output_stream_set_byte_order (dstream, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    if (self->reserved) {
        cheader->offsetfiles += self->reserved->len + 4;
        cheader->flags = CABINET_HEADER_RESERVE;
        cheader->res_header = self->reserved->len;
        cheader->res_folder = 0;
        cheader->res_data = 0;
        cheader->reserved = self->reserved->data;
    }

    files = gcab_folder_get_files (cabfolder);
    for (GSList *l = files; l != NULL; l = l->next) {
        GCabFile *cabfile = GCAB_FILE (l->data);
        sumstr += strlen (gcab_file_get_name (cabfile)) + 1;
    }

    folder.typecomp = gcab_folder_get_comptype (cabfolder);
    folder.offsetdata = cheader->offsetfiles + nfiles * 16 + sumstr;
    folder.ndatab = 0;

    /* avoid seeking to allow growing output streams */
    for (guint i = 0; i < folder.offsetdata; i++)
        if (!g_data_output_stream_put_byte (dstream, 0, cancellable, error))
            return FALSE;

    for (GSList *l = files; l != NULL; l = l->next) {
        g_autoptr(GInputStream) in = NULL;

        GCabFile *file = GCAB_FILE (l->data);
        if (file_callback)
            file_callback (file, user_data);

        in = gcab_file_get_input_stream (file, cancellable, error);
        if (in == NULL)
            return FALSE;

        while ((len = g_input_stream_read (in,
                                           &data[offset], CAB_MAX_BLOCK_SIZE - offset,
                                           cancellable, error)) ==
                (CAB_MAX_BLOCK_SIZE - offset)) {
            if (!cdata_write (&block, dstream, folder.typecomp, data,
                              CAB_MAX_BLOCK_SIZE, &written, cancellable, error))
                return FALSE;
            folder.ndatab++;
            cheader->size += written;
            offset = 0;
        }

        if (len == -1)
            return FALSE;

        offset += len;
    }
    if (offset != 0) {
        if (!cdata_write (&block, dstream, folder.typecomp, data, offset, &written, cancellable, error))
            return FALSE;
        folder.ndatab++;
        cheader->size += written;
    }

    if (!g_seekable_seek (G_SEEKABLE (out), 0,
                          G_SEEK_SET, cancellable, error))
        return FALSE;

    cheader->nfiles = nfiles;
    cheader->size += cheader->offsetfiles + nfiles * 16; /* 1st part cfile struct = 16 bytes */
    cheader->size += sumstr;

    if (!cheader_write (cheader, dstream, cancellable, error))
        return FALSE;

    if (!cfolder_write (&folder, dstream, cancellable, error))
        return FALSE;

    for (GSList *l = files; l != NULL; l = l->next) {
        GCabFile *file = GCAB_FILE (l->data);
        gcab_file_set_uoffset (file, prevf ? gcab_file_get_uoffset (prevf) + gcab_file_get_usize (prevf) : 0);
        prevf = file;

        /* automatically set flag if UTF-8 encoding */
        if (!g_str_is_ascii (gcab_file_get_name (file)))
            gcab_file_add_attribute (file, GCAB_FILE_ATTRIBUTE_NAME_IS_UTF);

        if (!cfile_write (gcab_file_get_cfile (file), dstream, cancellable, error))
            return FALSE;
    }

    /* replace the cached copy */
    if (self->cheader != NULL)
        cheader_free (self->cheader);
    self->cheader = g_steal_pointer (&cheader);

    return TRUE;
}

/**
 * gcab_cabinet_write_simple:
 * @cabinet: a #GCabCabinet
 * @stream: a #GOutputStream also #GSeekable
 * @file_callback: (allow-none) (scope call) (closure user_data): report current file being saved
 * @user_data: (closure): user data to pass to callbacks
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Save @cabinet to the output stream @out. @out must be a #GSeekable.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_cabinet_write_simple (GCabCabinet *self,
                           GOutputStream *out,
                           GCabFileCallback file_callback,
                           gpointer user_data,
                           GCancellable *cancellable,
                           GError **error)
{
    return gcab_cabinet_write (self, out, file_callback, NULL, user_data, cancellable, error);
}

/**
 * gcab_cabinet_new:
 *
 * Create a new #GCabCabinet object to read or create a Cabinet
 * archive.
 *
 * Returns: a new #GCabCabinet
 **/
GCabCabinet *
gcab_cabinet_new (void)
{
    return g_object_new (GCAB_TYPE_CABINET, NULL);
}

static gboolean
gcab_cabinet_is_compression_allowed(GCabCabinet *self, GCabCompression compression)
{
    /* for the fuzzing self tests */
    if (self->allowed_compression == 0)
        return TRUE;
    return (self->allowed_compression & (1ull << compression)) > 0;
}

/**
 * gcab_cabinet_add_allowed_compression:
 * @cabinet: a #GCabCabinet
 * @compression: a #GCabCompression kind, e.g. %GCAB_COMPRESSION_MSZIP
 *
 * Adds a compression kind to the allow-list. By default, GCab will use all decompression support
 * compiled in at build time. Once this function has been called only specific compression kinds
 * will be used in functions like gcab_cabinet_load().
 *
 * Since: 1.6
 **/
void
gcab_cabinet_add_allowed_compression (GCabCabinet *self, GCabCompression compression)
{
    g_return_if_fail (GCAB_IS_CABINET (self));
    g_return_if_fail (compression < GCAB_COMPRESSION_MASK);

    /* clear all */
    if (self->allowed_compression == GCAB_COMPRESSION_MASK)
        self->allowed_compression = 0x0;

    /* enable this */
    if (g_getenv ("GCAB_SKIP_COMPRESSION_CHECK") == NULL)
        self->allowed_compression |= 1ull << compression;
}

/**
 * gcab_cabinet_load:
 * @cabinet: a #GCabCabinet
 * @stream: a #GInputStream
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Load a cabinet archive.
 *
 * Returns: %TRUE on success
 **/
gboolean
gcab_cabinet_load (GCabCabinet *self,
                   GInputStream *stream,
                   GCancellable *cancellable,
                   GError **error)
{
    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);
    g_return_val_if_fail (self->folders->len == 0, FALSE);
    g_return_val_if_fail (self->stream == NULL, FALSE);

    g_autoptr(cheader_t) cheader = NULL;
    g_autoptr(GDataInputStream) in = g_data_input_stream_new (stream);
    g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (in), FALSE);
    g_data_input_stream_set_byte_order (in, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    cheader = g_new0 (cheader_t, 1);
    if (!cheader_read (cheader, in, cancellable, error))
        return FALSE;

    if (cheader->reserved != NULL) {
        g_autoptr(GByteArray) blob = NULL;
        blob = g_byte_array_new_take (cheader->reserved, cheader->res_header);
        g_object_set (self, "reserved", blob, NULL);
        cheader->reserved = NULL;
    }

    for (guint i = 0; i < cheader->nfolders; i++) {
        g_autoptr(cfolder_t) cfolder = g_new0 (cfolder_t, 1);
        g_autoptr(GByteArray) blob = NULL;

        if (!cfolder_read (cfolder, cheader->res_folder, in, cancellable, error))
            return FALSE;

        /* only allow some compression types at runtime */
        if (!gcab_cabinet_is_compression_allowed (self, cfolder->typecomp)) {
            g_set_error (error,
                         GCAB_ERROR,
                         GCAB_ERROR_NOT_SUPPORTED,
                         "compression kind 0x%x not allowed",
                         cfolder->typecomp);
            return FALSE;
        }

        /* steal this inelegantly */
        if (cfolder->reserved != NULL) {
            blob = g_byte_array_new_take (cfolder->reserved, cheader->res_folder);
            cfolder->reserved = NULL;
        }

        GCabFolder *folder = gcab_folder_new_steal_cfolder (&cfolder);
        if (blob != NULL)
            g_object_set (folder, "reserved", blob, NULL);
        g_ptr_array_add (self->folders, folder);
    }

    for (guint i = 0; i < cheader->nfiles; i++) {
        g_autoptr(cfile_t) cfile = g_new0 (cfile_t, 1);
        if (!cfile_read (cfile, in, cancellable, error))
            return FALSE;

        if (cfile->index >= self->folders->len) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                         "Invalid folder index");
            return FALSE;
        }

        GCabFolder *folder = g_ptr_array_index (self->folders, cfile->index);
        if (folder == NULL) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                         "Invalid folder pointer");
            return FALSE;
        }

        g_autoptr(GCabFile) file = gcab_file_new_steal_cfile (&cfile);
        if (!gcab_folder_add_file (folder, file, FALSE, cancellable, error))
            return FALSE;
    }

    self->stream = g_object_ref (stream);
    self->cheader = g_steal_pointer (&cheader);
    return TRUE;
}

/**
 * gcab_cabinet_extract:
 * @cabinet: a #GCabCabinet
 * @path: (allow-none): the path to extract files
 * @file_callback: (allow-none) (scope call) (closure user_data): an optional #GCabFile callback,
 *     return %FALSE to filter out or skip files.
 * @progress_callback: (allow-none) (scope call) (closure user_data): a progress callback
 * @user_data: (closure): callback data
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Extract files to given path.
 *
 * If @path is NULL then the files are decompressed to memory blobs stored on
 * each #GCabFile.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_cabinet_extract (GCabCabinet *self,
                      GFile *path,
                      GCabFileCallback file_callback,
                      GFileProgressCallback progress_callback,
                      gpointer user_data,
                      GCancellable *cancellable,
                      GError **error)
{
    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (!path || G_IS_FILE (path), FALSE);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    /* never loaded from a stream */
    if (self->cheader == NULL) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Cabinet has not been loaded");
        return FALSE;
    }

    g_autoptr(GDataInputStream) data = g_data_input_stream_new (self->stream);
    g_data_input_stream_set_byte_order (data, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
    g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (data), FALSE);

    for (guint i = 0; i < self->folders->len; ++i) {
        GCabFolder *folder = g_ptr_array_index (self->folders, i);
        if (!gcab_folder_extract (folder, data, path, self->cheader->res_data,
                                  file_callback, progress_callback, user_data,
                                  cancellable, error)) {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * gcab_cabinet_extract_simple:
 * @cabinet: a #GCabCabinet
 * @path: the path to extract files
 * @file_callback: (allow-none) (scope call) (closure user_data): an optional #GCabFile callback,
 *     return %FALSE to filter out or skip files.
 * @user_data: (closure): callback data
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Extract files to given path.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_cabinet_extract_simple (GCabCabinet *cabinet,
                             GFile *path,
                             GCabFileCallback file_callback,
                             gpointer user_data,
                             GCancellable *cancellable,
                             GError **error)
{
    return gcab_cabinet_extract (cabinet, path, file_callback, NULL, user_data, cancellable, error);
}

/**
 * gcab_cabinet_get_signature:
 * @cabinet: a #GCabCabinet
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Lookup the cabinet authenticode signature if any.
 *
 * Since: 0.5
 *
 * Returns: the array containing the PKCS#7 signed data or %NULL on error.
 **/
const GByteArray *
gcab_cabinet_get_signature (GCabCabinet *self,
                            GCancellable *cancellable,
                            GError **error)
{
    const guint8 magic[] = { 0x00, 0x00, 0x10, 0x00 };
    gssize sz;
    guint8 *reserved;
    guint32 offset;
    guint32 size;

    g_return_val_if_fail (GCAB_IS_CABINET (self), NULL);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    if (self->signature)
        return self->signature;

    if (!G_IS_SEEKABLE (self->stream)) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED,
                     "Cabinet stream is not seekable");
        return NULL;
    }

    if (!self->reserved || self->reserved->len != 20) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Cabinet has no reserved area");
        return NULL;
    }

    reserved = self->reserved->data;
    if (memcmp (reserved, magic, sizeof (magic)) != 0) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                     "Cabinet reserved magic was not correct");
        return NULL;
    }

    offset = GCAB_READ_UINT32_LE (reserved + 4);
    size = GCAB_READ_UINT32_LE (reserved + 8);
    if (g_getenv ("GCAB_DEBUG"))
        g_debug ("signature offset: %u size: %u", offset, size);

    self->signature = g_byte_array_sized_new (size);
    g_byte_array_set_size (self->signature, size);

    if (!g_seekable_seek (G_SEEKABLE (self->stream), offset, G_SEEK_SET, cancellable, error)) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Cannot seek to reserved area");
        return NULL;
    }

    sz = g_input_stream_read (self->stream,
                              self->signature->data, self->signature->len,
                              cancellable, error);
    if (sz < 0) {
        g_prefix_error (error, "Failed to read signature from stream: ");
        return NULL;
    }
    if (sz != self->signature->len) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Failed to read correct size signature from stream: ");
        return NULL;
    }

    return self->signature;
}
