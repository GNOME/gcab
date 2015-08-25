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

struct _GCabCabinetClass
{
    GObjectClass parent_class;
};

struct _GCabCabinet
{
    GObject parent_instance;

    GPtrArray *folders;
    GByteArray *reserved;
    cheader_t cheader;
    GByteArray *signature;
    GInputStream *stream;
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
    self->folders = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gcab_cabinet_finalize (GObject *object)
{
    GCabCabinet *self = GCAB_CABINET (object);

    g_ptr_array_unref (self->folders);
    if (self->reserved)
        g_byte_array_unref (self->reserved);
    if (self->signature)
        g_byte_array_unref (self->signature);
    g_clear_object (&self->stream);

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
 * Returns: (element-type GCabFolder) (transfer full): an array of #GCabFolder
 **/
GPtrArray *
gcab_cabinet_get_folders (GCabCabinet *self)
{
    g_return_val_if_fail (GCAB_IS_CABINET (self), NULL);

    return self->folders;
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
    cheader_t header = {
        .offsetfiles = CFI_START, // CFHEADER + 1 * CFFOLDER
        .nfolders = 1, // a single CAB folder is enough
    };
    cfolder_t folder = { 0, };

    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (out), FALSE);
    g_return_val_if_fail (G_IS_SEEKABLE (out), FALSE);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);
    /* FIXME: current limitation of 1 folder */
    g_return_val_if_fail (self->folders->len == 1, FALSE);

    GCabFolder *cabfolder = g_ptr_array_index (self->folders, 0);
    GCabFile *file;
    gsize nfiles = gcab_folder_get_nfiles (cabfolder);
    GInputStream *in = NULL;
    GDataOutputStream *dstream = NULL;
    gboolean success = FALSE;
    gssize len, offset = 0;
    cdata_t block = { 0, };
    guint8 data[DATABLOCKSIZE];
    gsize written;
    size_t sumstr = 0;
    GSList *l, *files;
    cfile_t *prevf = NULL;
    int i;

    dstream = g_data_output_stream_new (out);
    g_data_output_stream_set_byte_order (dstream, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    if (self->reserved) {
        header.offsetfiles += self->reserved->len + 4;
        header.flags = CABINET_HEADER_RESERVE;
        header.res_header = self->reserved->len;
        header.res_folder = 0;
        header.res_data = 0;
        header.reserved = self->reserved->data;
    }

    files = gcab_folder_get_files (cabfolder);
    for (l = files; l != NULL; l = l->next)
        sumstr += strlen (GCAB_FILE (l->data)->name) + 1;

    folder.typecomp = cabfolder->comptype;
    folder.offsetdata = header.offsetfiles + nfiles * 16 + sumstr;
    folder.ndatab = gcab_folder_get_ndatablocks (cabfolder);

    /* avoid seeking to allow growing output streams */
    for (i = 0; i < folder.offsetdata; i++)
        if (!g_data_output_stream_put_byte (dstream, 0, cancellable, error))
            goto end;

    for (l = files; l != NULL; l = l->next) {
        file = GCAB_FILE (l->data);
        if (file_callback)
            file_callback (file, user_data);

        g_clear_object (&in);
        in = G_INPUT_STREAM (g_file_read (file->file, cancellable, error));
        if (*error)
            goto end;

        while ((len = g_input_stream_read (in,
                                           &data[offset], DATABLOCKSIZE - offset,
                                           cancellable, error)) == (DATABLOCKSIZE - offset)) {
            if (!cdata_write (&block, dstream, folder.typecomp, data, DATABLOCKSIZE, &written, cancellable, error))
                goto end;
            header.size += written;
            offset = 0;
        }

        if (len == -1)
            goto end;

        offset += len;
    }
    if (offset != 0) {
        if (!cdata_write (&block, dstream, folder.typecomp, data, offset, &written, cancellable, error))
            goto end;
        header.size += written;
    }

    if (!g_seekable_seek (G_SEEKABLE (out), 0,
                          G_SEEK_SET, cancellable, error))
        goto end;

    header.nfiles = nfiles;
    header.size += header.offsetfiles + nfiles * 16; /* 1st part cfile struct = 16 bytes */
    header.size += sumstr;

    if (!cheader_write (&header, dstream, cancellable, error))
        goto end;

    if (!cfolder_write (&folder, dstream, cancellable, error))
        goto end;

    for (l = files; l != NULL; l = l->next) {
        file = GCAB_FILE (l->data);
        file->cfile.uoffset = prevf ? prevf->uoffset + prevf->usize : 0;
        prevf = &file->cfile;

        /* automatically set flag if UTF-8 encoding */
        if (!g_str_is_ascii (file->cfile.name))
            file->cfile.fattr |= GCAB_FILE_ATTRIBUTE_NAME_IS_UTF;

        if (!cfile_write (&file->cfile, dstream, cancellable, error))
            goto end;
    }

    success = TRUE;

end:
    g_clear_object (&dstream);
    g_clear_object (&in);
    g_slist_free (files);

    return success;
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

    self->stream = g_object_ref (stream);

    gboolean success = FALSE;
    cheader_t cheader;
    int i;
    GDataInputStream *in = g_data_input_stream_new (stream);
    g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (in), FALSE);
    g_data_input_stream_set_byte_order (in, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    GPtrArray *folders = self->folders;

    if (!cheader_read (&cheader, in, cancellable, error))
        goto end;

    if (cheader.reserved)
        g_object_set (self, "reserved",
                      g_byte_array_new_take (cheader.reserved, cheader.res_header),
                      NULL);

    for (i = 0; i < cheader.nfolders; i++) {
        cfolder_t cfolder = { 0, };
        if (!cfolder_read (&cfolder, cheader.res_folder, in, cancellable, error))
            goto end;

        GCabFolder *folder = gcab_folder_new_with_cfolder (&cfolder, stream);
        if (cfolder.reserved)
            g_object_set (folder, "reserved",
                          g_byte_array_new_take (cfolder.reserved, cheader.res_folder),
                          NULL);
        g_ptr_array_add (folders, folder);
        cfolder.reserved = NULL;
    }

    for (i = 0; i < cheader.nfiles; i++) {
        cfile_t cfile = { 0, };
        if (!cfile_read (&cfile, in, cancellable, error))
            goto end;

        if (cfile.index >= folders->len) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                         "Invalid folder index");
            goto end;
        }

        GCabFolder *folder = g_ptr_array_index (folders, cfile.index);
        if (folder == NULL) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                         "Invalid folder pointer");
            goto end;
        }

        GCabFile *file = gcab_file_new_with_cfile (&cfile);
        if (!gcab_folder_add_file (folder, file, FALSE, cancellable, error)) {
            g_object_unref (file);
            goto end;
        }
    }

    success = TRUE;

end:
    if (in)
        g_object_unref (in);
    return success;
}

/**
 * gcab_cabinet_extract:
 * @cabinet: a #GCabCabinet
 * @path: the path to extract files
 * @file_callback: (allow-none) (scope call) (closure user_data): an optionnal #GCabFile callback,
 *     return %FALSE to filter out or skip files.
 * @progress_callback: (allow-none) (scope call) (closure user_data): a progress callback
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
gcab_cabinet_extract (GCabCabinet *self,
                      GFile *path,
                      GCabFileCallback file_callback,
                      GFileProgressCallback progress_callback,
                      gpointer user_data,
                      GCancellable *cancellable,
                      GError **error)
{
    gboolean success = TRUE;
    int i;

    g_return_val_if_fail (GCAB_IS_CABINET (self), FALSE);
    g_return_val_if_fail (G_IS_FILE (path), FALSE);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    for (i = 0; i < self->folders->len; ++i) {
        GCabFolder *folder = g_ptr_array_index (self->folders, i);
        if (!gcab_folder_extract (folder, path, self->cheader.res_data,
                                  file_callback, progress_callback, user_data,
                                  cancellable, error)) {
            success = FALSE;
            break;
        }
    }

    return success;
}

/**
 * gcab_cabinet_extract_simple:
 * @cabinet: a #GCabCabinet
 * @path: the path to extract files
 * @file_callback: (allow-none) (scope call) (closure user_data): an optionnal #GCabFile callback,
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
 * Returns: the array containing the PKCS#7 signed data or %NULL if
 * none found or error.
 **/
const GByteArray *
gcab_cabinet_get_signature (GCabCabinet *self,
                            GCancellable *cancellable,
                            GError **error)
{
    const guint8 magic[] = { 0x00, 0x00, 0x10, 0x00 };
    guint8 *reserved;
    guint32 offset;
    guint32 size;

    g_return_val_if_fail (GCAB_IS_CABINET (self), NULL);
    g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    if (self->signature)
        return self->signature;

    if (!G_IS_SEEKABLE (self->stream))
        return NULL;

    if (!self->reserved || self->reserved->len != 20)
        return NULL;

    reserved = self->reserved->data;
    if (memcmp (reserved, magic, sizeof (magic)) != 0)
        return NULL;

    offset = GCAB_READ_UINT32_LE (reserved + 4);
    size = GCAB_READ_UINT32_LE (reserved + 8);
    if (g_getenv ("GCAB_DEBUG"))
        g_debug ("signature offset: %u size: %u", offset, size);

    self->signature = g_byte_array_sized_new (size);
    g_byte_array_set_size (self->signature, size);

    if (!g_seekable_seek (G_SEEKABLE (self->stream), offset, G_SEEK_SET, cancellable, error))
        return NULL;

    if (g_input_stream_read (self->stream, self->signature->data, self->signature->len,
                             cancellable, error) != self->signature->len)
        return NULL;

    return self->signature;
}
