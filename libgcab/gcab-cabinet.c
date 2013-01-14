#include "gcab-priv.h"

struct _GCabCabinetClass
{
    GObjectClass parent_class;
};

struct _GCabCabinet
{
    GObject parent_instance;

    GPtrArray *folders;
    GByteArray *reserved;
};

enum {
    PROP_0,

    PROP_RESERVED
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
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_cabinet_class_init (GCabCabinetClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcab_cabinet_finalize;
    object_class->set_property = gcab_cabinet_set_property;
    object_class->get_property = gcab_cabinet_get_property;

    g_object_class_install_property (object_class, PROP_RESERVED,
         g_param_spec_boxed ("reserved", "Reserved", "Reserved",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gcab_cabinet_add_folder:
 * @cabinet: a #GCabCabinet
 * @folder:
 * @error:
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
 * @file_callback: (allow-none) (scope call): report current file being saved
 * @progress_callback: (allow-none) (scope call): report saving progress
 * @callback_data: (closure): user data to pass to callbacks
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
                    gpointer callback_data,
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
    int i;

    dstream = g_data_output_stream_new (out);
    g_data_output_stream_set_byte_order (dstream, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);

    size_t sumstr = 0;
    GSList *l, *files = gcab_folder_get_files (cabfolder);

    for (l = files; l != NULL; l = l->next)
        sumstr += strlen (GCAB_FILE (l->data)->name) + 1;

    folder.typecomp = cabfolder->compression;
    folder.offsetdata = CFI_START + nfiles * 16 + sumstr;
    folder.ndatab = gcab_folder_get_ndatablocks (cabfolder);

    /* avoid seeking to allow growing output streams */
    for (i = 0; i < folder.offsetdata; i++)
        if (!g_data_output_stream_put_byte (dstream, 0, cancellable, error))
            goto end;

    gssize len, offset = 0;
    cdata_t block = { 0, };
    guint8 data[DATABLOCKSIZE];
    gsize written;
    for (l = files; l != NULL; l = l->next) {
        file = GCAB_FILE (l->data);
        if (file_callback)
            file_callback (file, callback_data);

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
    header.size += CFI_START + nfiles * 16; /* 1st part cfile struct = 16 bytes */
    header.size += sumstr;

    if (!cheader_write (&header, dstream, cancellable, error))
        goto end;

    if (!cfolder_write (&folder, dstream, cancellable, error))
        goto end;

    cfile_t *prevf = NULL;
    for (l = files; l != NULL; l = l->next) {
        file = GCAB_FILE (l->data);
        file->cfile.uoffset = prevf ? prevf->uoffset + prevf->usize : 0;
        prevf = &file->cfile;

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
 * gcab_cabinet_new:
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

    gboolean success = FALSE;
    cheader_t cheader;
    int i;
    GDataInputStream *in = g_data_input_stream_new (stream);
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

        GCabFolder *folder = gcab_folder_new_with_cfolder (&cfolder);
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

        GCabFile *file = gcab_file_new_with_cfile (&cfile);
        GCabFolder *folder = g_ptr_array_index (folders, cfile.index);
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
