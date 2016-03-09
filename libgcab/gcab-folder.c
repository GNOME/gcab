#include "gcab-priv.h"

/**
 * SECTION:gcab-folder
 * @title: GCabFolder
 * @short_description: A Cabinet folder
 * @see_also: #GCabFolder
 * @stability: Stable
 * @include: libgcab.h
 *
 * A GCabFolder is a handle to a folder within the Cabinet archive. A
 * Cabinet folder <emphasis>is not</emphasis> like a directory. It is
 * a sub-container grouping GCabFiles together, sharing some common
 * settings like the compression method.
 *
 * You can retrieve the files within a folder with
 * gcab_folder_get_files().
 *
 * In order to add a file to a folder for creation, use
 * gcab_folder_add_file().
 */

struct _GCabFolderClass
{
    GObjectClass parent_class;
};

enum {
    PROP_0,

    PROP_COMPRESSION,
    PROP_COMPTYPE,
    PROP_RESERVED
};

G_DEFINE_TYPE (GCabFolder, gcab_folder, G_TYPE_OBJECT);

static void
gcab_folder_init (GCabFolder *self)
{
    self->files = NULL;
    self->hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

static void
gcab_folder_finalize (GObject *object)
{
    GCabFolder *self = GCAB_FOLDER (object);

    g_slist_free_full (self->files, g_object_unref);
    g_hash_table_unref (self->hash);
    if (self->reserved)
        g_byte_array_unref (self->reserved);
    if (self->stream)
        g_object_unref (self->stream);

    G_OBJECT_CLASS (gcab_folder_parent_class)->finalize (object);
}

static void
gcab_folder_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FOLDER (object));
    GCabFolder *self = GCAB_FOLDER (object);

    switch (prop_id) {
    case PROP_COMPTYPE:
        self->comptype = g_value_get_int (value);
        break;
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
gcab_folder_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FOLDER (object));
    GCabFolder *self = GCAB_FOLDER (object);

    switch (prop_id) {
    case PROP_COMPRESSION:
        g_value_set_enum (value, self->comptype & GCAB_COMPRESSION_MASK);
        break;
    case PROP_COMPTYPE:
        g_value_set_int (value, self->comptype);
        break;
    case PROP_RESERVED:
        g_value_set_boxed (value, self->reserved);
	break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_folder_class_init (GCabFolderClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcab_folder_finalize;
    object_class->set_property = gcab_folder_set_property;
    object_class->get_property = gcab_folder_get_property;

    g_object_class_install_property (object_class, PROP_COMPRESSION,
        g_param_spec_enum ("compression", "compression", "compression",
                           GCAB_TYPE_COMPRESSION, 0,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_COMPTYPE,
        g_param_spec_int ("comptype", "comptype", "comptype",
                          0, G_MAXINT, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_RESERVED,
         g_param_spec_boxed ("reserved", "Reserved", "Reserved",
                            G_TYPE_BYTE_ARRAY,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

/* calculate the number of datablocks we will need:
   cabinet files are written in blocks of 32768 bytes */
G_GNUC_INTERNAL gsize
gcab_folder_get_ndatablocks (GCabFolder *self)
{
    gsize total_size = 0;
    GSList *l;

    for (l = self->files; l != NULL; l = l->next)
        total_size += GCAB_FILE (l->data)->cfile.usize;

    return total_size / DATABLOCKSIZE + 1 ;
}

static gboolean
add_file (GCabFolder *self, GCabFile *file, GError **error)
{
    if (g_hash_table_lookup (self->hash, (gpointer)gcab_file_get_name (file))) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                     "File '%s' has already been added", gcab_file_get_name (file));
        return FALSE;
    }

    g_hash_table_insert (self->hash,
                         (gpointer)gcab_file_get_name (file), g_object_ref (file));
    self->files = g_slist_prepend (self->files, g_object_ref (file));

    return TRUE;
}

#define FILE_ATTRS "standard::*,time::modified"

static gboolean
add_file_info (GCabFolder *self, GCabFile *file, GFileInfo *info,
               const gchar *name, gboolean recurse, GError **error)
{
    GFileType file_type = g_file_info_get_file_type (info);

    if (file_type == G_FILE_TYPE_DIRECTORY) {
        if (!recurse)
            return TRUE;

        GFileEnumerator *dir = g_file_enumerate_children (file->file, FILE_ATTRS, 0, NULL, error);
        if (*error) {
            g_warning ("Couldn't enumerate directory %s: %s", name, (*error)->message);
            g_clear_error (error);
            return TRUE;
        }

        while ((info = g_file_enumerator_next_file (dir, NULL, error)) != NULL) {
            GFile *child = g_file_get_child (file->file, g_file_info_get_name (info));
            gchar *child_name = g_build_path ("\\", name, g_file_info_get_name (info), NULL);
            GCabFile *child_file = gcab_file_new_with_file (child_name, child);

            add_file_info (self, child_file, info, child_name, recurse, error);
            if (*error) {
                g_warning ("Couldn't add file %s: %s",
                           child_name, (*error)->message);
                g_clear_error (error);
            }

            g_object_unref (child_file);
            g_free (child_name);
            g_object_unref (child);
            g_object_unref (info);
        }

        g_object_unref (dir);

    } else if (file_type == G_FILE_TYPE_REGULAR) {
        gcab_file_update_info (file, info);
        if (!add_file (self, file, error))
            return FALSE;

    } else {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "Unhandled file type: %d", file_type);
        return FALSE;
    }

    return TRUE;
}

/**
 * gcab_folder_add_file:
 * @cabfolder: a #GCabFolder
 * @cabfile: file to be added
 * @recurse: whether to recurse through subdirectories
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Add @file to the #GCabFolder.
 *
 * Returns: %TRUE on succes
 **/
gboolean
gcab_folder_add_file (GCabFolder *self, GCabFile *file,
                      gboolean recurse, GCancellable *cancellable,
                      GError **error)
{
    gboolean success;

    g_return_val_if_fail (GCAB_IS_FOLDER (self), FALSE);
    g_return_val_if_fail (GCAB_IS_FILE (file), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    GFile *gfile = gcab_file_get_file (file);
    if (gfile) {
        g_return_val_if_fail (G_IS_FILE (gfile), FALSE);

        GFileInfo *info = g_file_query_info (gfile, FILE_ATTRS, 0, NULL, error);
        if (*error)
            return FALSE;

        success = add_file_info (self, file, info,
                                 gcab_file_get_name (file), recurse, error);
        g_object_unref (info);
    } else {
        success = add_file (self, file, error);
    }

    return success;
}

/**
 * gcab_folder_get_nfiles:
 * @cabfolder: a #GCabFolder
 *
 * Get the number of files in this @folder.
 *
 * Returns: a #guint
 **/
guint
gcab_folder_get_nfiles (GCabFolder *self)
{
    g_return_val_if_fail (GCAB_IS_FOLDER (self), 0);

    return g_hash_table_size (self->hash);
}

/**
 * gcab_folder_new:
 * @comptype: compression to used in this folder
 *
 * Creates a new empty Cabinet folder. Use gcab_folder_add_file() to
 * add files to an archive.
 *
 * A Cabinet folder is not a file path, it is a container for files.
 *
 * Returns: a new #GCabFolder
 **/
GCabFolder *
gcab_folder_new (gint comptype)
{
    return g_object_new (GCAB_TYPE_FOLDER,
                         "comptype", comptype,
                         NULL);
}

G_GNUC_INTERNAL GCabFolder *
gcab_folder_new_with_cfolder (const cfolder_t *folder, GInputStream *stream)
{
    GCabFolder *self = g_object_new (GCAB_TYPE_FOLDER,
                                     "comptype", folder->typecomp,
                                     NULL);
    self->stream = g_object_ref (stream);
    self->cfolder = *folder;

    return self;
}

/**
 * gcab_folder_get_files:
 * @cabfolder: a #GCabFolder
 *
 * Get the list of #GCabFile files contained in the @cabfolder.
 *
 * Returns: (element-type GCabFile) (transfer full): list of files
 **/
GSList *
gcab_folder_get_files (GCabFolder *self)
{
    g_return_val_if_fail (GCAB_IS_FOLDER (self), 0);

    return g_slist_reverse (g_slist_copy (self->files));
}

static gint
sort_by_offset (GCabFile *a, GCabFile *b)
{
    g_return_val_if_fail (a != NULL, 0);
    g_return_val_if_fail (b != NULL, 0);

    return (gint64)a->cfile.uoffset - (gint64)b->cfile.uoffset;
}

G_GNUC_INTERNAL gboolean
gcab_folder_extract (GCabFolder *self,
                     GFile *path,
                     u1 res_data,
                     GCabFileCallback file_callback,
                     GFileProgressCallback progress_callback,
                     gpointer callback_data,
                     GCancellable *cancellable,
                     GError **error)
{
    GError *my_error = NULL;
    gboolean success = FALSE;
    GDataInputStream *data = NULL;
    GFileOutputStream *out = NULL;
    GSList *f, *files = NULL;
    cdata_t cdata = { 0, };
    u4 nubytes = 0;

    data = g_data_input_stream_new (self->stream);
    g_data_input_stream_set_byte_order (data, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
    g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (data), FALSE);

    if (!g_seekable_seek (G_SEEKABLE (data), self->cfolder.offsetdata, G_SEEK_SET, cancellable, error))
        goto end;

    files = g_slist_sort (g_slist_copy (self->files), (GCompareFunc)sort_by_offset);

    for (f = files; f != NULL; f = f->next) {
        GCabFile *file = f->data;

        if (file_callback && !file_callback (file, callback_data))
            continue;

        gchar *fname = g_strdup (gcab_file_get_extract_name (file));
        int i = 0, len = strlen (fname);
        for (i = 0; i < len; i++)
            if (fname[i] == '\\')
                fname[i] = '/';

        GFile *gfile = g_file_resolve_relative_path (path, fname);
        g_free (fname);

        if (!g_file_has_prefix (gfile, path)) {
            // "Rebase" the file in the given path, to ensure we never escape it
            char *rawpath = g_file_get_path (gfile);
            if (rawpath != NULL) {
                char *newpath = rawpath;
                while (*newpath != 0 && *newpath == G_DIR_SEPARATOR) {
                    newpath++;
                }
                GFile *newgfile = g_file_resolve_relative_path (path, newpath);
                g_free (rawpath);
                g_object_unref (gfile);
                gfile = newgfile;
            }
        }

        GFile *parent = g_file_get_parent (gfile);

        if (!g_file_make_directory_with_parents (parent, cancellable, &my_error)) {
            if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
                g_clear_error (&my_error);
            else {
                g_object_unref (gfile);
                g_object_unref (parent);
                g_propagate_error (error, my_error);
                goto end;
            }
        }
        g_object_unref (parent);

        g_clear_object (&out);
        out = g_file_replace (gfile, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, error);
        g_object_unref (gfile);
        if (!out)
            goto end;

        u4 usize = file->cfile.usize;
        u4 uoffset = file->cfile.uoffset;

        /* let's rewind if need be */
        if (uoffset < nubytes) {
            if (!g_seekable_seek (G_SEEKABLE (data), self->cfolder.offsetdata,
                                  G_SEEK_SET, cancellable, error))
                goto end;
            bzero(&cdata, sizeof(cdata));
            nubytes = 0;
        }

        while (usize > 0) {
            if ((nubytes + cdata.nubytes) <= uoffset) {
                nubytes += cdata.nubytes;
                if (!cdata_read (&cdata, res_data, self->comptype,
                                 data, cancellable, error))
                    goto end;
                continue;
            } else {
                gsize offset = file->cfile.uoffset > nubytes ?
                    file->cfile.uoffset - nubytes : 0;
                const void *p = &cdata.out[offset];
                gsize count = MIN (usize, cdata.nubytes - offset);
                if (!g_output_stream_write_all (G_OUTPUT_STREAM (out), p, count,
                                                NULL, cancellable, error))
                    goto end;
                usize -= count;
                uoffset += count;
            }
        }
    }

    success = TRUE;

end:
    if (files)
        g_slist_free (files);

    g_clear_object (&data);
    g_clear_object (&out);
    cdata_finish (&cdata, NULL);

    return success;
}
