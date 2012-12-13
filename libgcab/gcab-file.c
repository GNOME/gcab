#include "cabinet.h"
#include "gcab-file.h"

struct _GCabFileClass
{
    GObjectClass parent_class;
};

struct _GCabFile
{
    GObject parent_instance;

    GArray *cfiles;
};


enum {
    PROP_0,
};

G_DEFINE_TYPE (GCabFile, gcab_file, G_TYPE_OBJECT);

static void
cfile_clear (gpointer data)
{
    cfile_t *f = data;

    g_free (f->name);
    f->name = NULL;

    if (f->file) {
        g_object_unref (f->file);
        f->file = NULL;
    }
}

static void
gcab_file_init (GCabFile *self)
{
    self->cfiles = g_array_new (FALSE, TRUE, sizeof (cfile_t));
    g_array_set_clear_func (self->cfiles, cfile_clear);
}

static void
gcab_file_finalize (GObject *object)
{
    GCabFile *self = GCAB_FILE (object);

    g_array_unref (self->cfiles);

    G_OBJECT_CLASS (gcab_file_parent_class)->finalize (object);
}

static void
gcab_file_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FILE (object));

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_file_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FILE (object));

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_file_class_init (GCabFileClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gcab_file_finalize;
    object_class->set_property = gcab_file_set_property;
    object_class->get_property = gcab_file_get_property;
}

/* calculate the number of datablocks we will need:
   cabinet files are written in blocks of 32768 bytes */
static size_t
number_of_datablocks (GArray *cfiles)
{
    size_t total_size = 0;
    int i;

    for (i = 0; i < cfiles->len; i++)
        total_size += g_array_index (cfiles, cfile_t, i).usize;

    return total_size / DATABLOCKSIZE + 1 ;
}

#define FILE_ATTRS "standard::*,time::modified"

static gboolean
add_file_info (GArray *cfiles, GFile *file, GFileInfo *info,
               const gchar *name, gboolean recurse, GError **error)
{
    GFileType file_type = g_file_info_get_file_type (info);

    if (file_type == G_FILE_TYPE_DIRECTORY) {
        if (!recurse)
            return TRUE;

        GFileEnumerator *dir = g_file_enumerate_children (file, FILE_ATTRS, 0, NULL, error);
        if (*error) {
            g_warning ("Couldn't enumerate directory %s: %s", name, (*error)->message);
            g_clear_error (error);
            return TRUE;
        }

        while ((info = g_file_enumerator_next_file (dir, NULL, error)) != NULL) {
            GFile *child = g_file_get_child (file, g_file_info_get_name (info));
            gchar *child_name = g_build_filename (name, g_file_info_get_name (info), NULL);
            add_file_info (cfiles, child, info, child_name, recurse, error);

            if (*error) {
                g_warning ("Couldn't add file %s: %s",
                           child_name, (*error)->message);
                g_clear_error (error);
            }

            g_free (child_name);
            g_object_unref (info);
            g_object_unref (child);
        }

        g_object_unref (dir);

    } else if (file_type == G_FILE_TYPE_REGULAR) {
        cfile_t f = { 0, };
        GTimeVal tv;
        time_t time;
        struct tm *m; // Use GDateTime when 2.26 in RHEL6

        g_file_info_get_modification_time (info, &tv);
        time = tv.tv_sec;
        m = localtime (&time);

        f.name = g_strdup (name);
#ifndef G_OS_WIN32
        int i, len = strlen (f.name);
        for (i = 0; i < len; i++)
            if (f.name[i] == G_DIR_SEPARATOR)
                f.name[i] = '\\';
#endif
        f.usize = g_file_info_get_size (info);
        f.fattr = CABINET_ARCH;
        f.date = ((m->tm_year + 1900 - 1980 ) << 9 ) +
            ((m->tm_mon+1) << 5 ) + (m->tm_mday);
        f.time = (m->tm_hour << 11) + (m->tm_min << 5) + (m->tm_sec / 2);
        f.file = g_object_ref (file);
        if (cfiles->len != 0) {
            cfile_t *prevf = &g_array_index (cfiles, cfile_t, cfiles->len - 1);
            f.uoffset = prevf->uoffset + prevf->usize;
        }

        g_array_append_val (cfiles, f);

    } else {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "Unhandled file type: %d", file_type);
        return FALSE;
    }

    return TRUE;
}

static gchar *
remove_leading_path (gchar *name)
{
    int i;

    i = 0;
    if (name[0] == G_DIR_SEPARATOR)
        i = 1;
    while (name[i] == '.' && name[i + 1] == '.' && name[i + 2] == G_DIR_SEPARATOR)
        i += 3;

    if (i != 0) {
        gchar c = name[i];

        name[i] = '\0';
        g_warning ("Removing leading '%s' from member names", name);
        name[i] = c;
    }

    return name + i;
}

/**
 * gcab_file_add:
 * @cabfile: a #GCabfile
 * @file: file to be added
 * @name: name given in the archive file
 * @recurse: whether to recurse through subdirectories
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Add @file to the #GCabFile.
 *
 * Returns: %TRUE on succes
 **/
gboolean
gcab_file_add (GCabFile *self, GFile *file, const gchar *name,
               gboolean recurse, GCancellable *cancellable,
               GError **error)
{
    gboolean success;

    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);
    g_return_val_if_fail (G_IS_FILE (file), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    GFileInfo *info = g_file_query_info (file, FILE_ATTRS, 0, NULL, error);
    if (*error)
        return FALSE;

    gchar *tmpname = strdup (name);

    success = add_file_info (self->cfiles, file, info,
                             remove_leading_path (tmpname), recurse, error);

    g_free (tmpname);
    g_object_unref (info);

    return success;
}


/**
 * gcab_file_save:
 * @cabfile: a #GCabFile
 * @out: a #GOutputStream #GSeekable
 * @compression: compression method
 * @file_callback: (allow-none) (scope call): report current file being saved
 * @progress_callback: (allow-none) (scope call): report saving progress
 * @callback_data: (closure): user data to pass to callbacks
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Save @cabfile to the output stream @out. @out must be a #GSeekable.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gcab_file_save (GCabFile *self,
                GOutputStream *out,
                GCabCompression compression,
                GCabFileSaveCallback file_callback,
                GFileProgressCallback progress_callback,
                gpointer callback_data,
                GCancellable *cancellable,
                GError **error)
{
    int i;
    cheader_t header = {
        .offsetfiles = CFI_START, // CFHEADER + 1 * CFFOLDER
        .nfolders = 1, // a single CAB folder is enough
    };
    cfolder_t folder = { 0, };
    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);
    g_return_val_if_fail (G_IS_OUTPUT_STREAM (out), FALSE);
    g_return_val_if_fail (G_IS_SEEKABLE (out), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    GArray *cfiles = self->cfiles;
    if (cfiles->len == 0) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "No file");
        return FALSE;
    }

    size_t sumstr = 0;
    for (i = 0; i < cfiles->len; ++i) {
        const gchar *name = g_array_index (cfiles, cfile_t, i).name;
        sumstr += strlen (name) + 1;
    }

    folder.typecomp = compression;
    folder.offsetdata = CFI_START + cfiles->len * 16 + sumstr;
    folder.ndatab = number_of_datablocks (cfiles);

    if (!g_seekable_seek (G_SEEKABLE (out), folder.offsetdata,
                          G_SEEK_SET, NULL, error))
        return FALSE;

    gssize len, offset = 0;
    cdata_t block = { 0, };
    guint8 data[DATABLOCKSIZE];
    for (i = 0; i < cfiles->len; ++i) {
        cfile_t *file = &g_array_index (cfiles, cfile_t, i);
        if (file_callback)
            file_callback (file->file, callback_data);

        GInputStream *in = G_INPUT_STREAM (g_file_read (file->file, cancellable, error));
        if (*error) {
            g_object_unref (in);
            return FALSE;
        }

        while ((len = g_input_stream_read (in,
                                           &data[offset], DATABLOCKSIZE - offset,
                                           cancellable, error)) == (DATABLOCKSIZE - offset)) {
            ssize_t written = cdata_write (&block, out, compression, data, DATABLOCKSIZE, error);
            header.size += written;
            offset = 0;
        }

        if (len == -1) {
            g_object_unref (in);
            return FALSE;
        }

        offset += len;
        g_object_unref (in);
    }
    if (offset != 0) {
        ssize_t written = cdata_write (&block, out, compression, data, offset, error);
        if (written == -1)
            return FALSE;
        header.size += written;
    }

    if (!g_output_stream_flush (out, cancellable, error))
        return FALSE;

    if (!g_seekable_seek (G_SEEKABLE (out), 0,
                          G_SEEK_SET, cancellable, error))
        return FALSE;

    header.nfiles = cfiles->len;
    header.size += CFI_START + cfiles->len * 16; /* 1st part cfile struct = 16 bytes */
    header.size += sumstr;

    if (cheader_write (&header, out, error) == -1)
        return FALSE;

    if (cfolder_write (&folder, out, error) == -1)
        return FALSE;

    for (i = 0; i < cfiles->len; ++i)
        if (cfile_write (&g_array_index (cfiles, cfile_t, i), out, error) == -1)
            return FALSE;

    return TRUE;
}

GCabFile *
gcab_file_new (void)
{
    return g_object_new (GCAB_TYPE_FILE, NULL);
}
