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

#include <strings.h>

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

struct _GCabFolder
{
    GObject parent_instance;

    GSList *files;
    GHashTable *hash;
    gint comptype;
    GByteArray *reserved;
    cfolder_t *cfolder;
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

    cfolder_free (self->cfolder);
    g_slist_free_full (self->files, g_object_unref);
    g_hash_table_unref (self->hash);
    if (self->reserved)
        g_byte_array_unref (self->reserved);

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

/**
 * gcab_folder_get_comptype:
 * @cabfolder: a #GCabFolder
 *
 * Returns the compression used in this folder.
 *
 * Returns: a #GCabCompression, e.g. %GCAB_COMPRESSION_MSZIP
 *
 * Since: 1.0
 **/
gint
gcab_folder_get_comptype (GCabFolder *self)
{
    return self->comptype;
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

static gint
_sort_cfiles (GCabFile *file_a, GCabFile *file_b)
{
    return g_strcmp0 (gcab_file_get_name (file_a), gcab_file_get_name (file_b));
}

static gboolean
add_file_info (GCabFolder *self, GCabFile *file, GFileInfo *info,
               const gchar *name, gboolean recurse, GError **error)
{
    GFileType file_type = g_file_info_get_file_type (info);

    if (file_type == G_FILE_TYPE_DIRECTORY) {
        if (!recurse)
            return TRUE;

        g_autoptr(GFileEnumerator) dir = g_file_enumerate_children (gcab_file_get_gfile (file), FILE_ATTRS, 0, NULL, error);
        if (dir == NULL) {
            g_warning ("Couldn't enumerate directory %s: %s", name, (*error)->message);
            g_clear_error (error);
            return TRUE;
        }

        while ((info = g_file_enumerator_next_file (dir, NULL, error)) != NULL) {
            g_autoptr(GFile) child = g_file_get_child (gcab_file_get_gfile (file), g_file_info_get_name (info));
            g_autofree gchar *child_name = g_build_path ("\\", name, g_file_info_get_name (info), NULL);
            g_autoptr(GCabFile) child_file = gcab_file_new_with_file (child_name, child);
            if (!add_file_info (self, child_file, info, child_name, recurse, error)) {
                g_object_unref (info);
                return FALSE;
            }

            /* sort the files to avoid depending on filesystem order */
            self->files = g_slist_sort (self->files, (GCompareFunc)_sort_cfiles);

            g_object_unref (info);
        }

    } else if (file_type == G_FILE_TYPE_REGULAR) {
        gcab_file_update_info (file, info);
        if (!add_file (self, file, error))
            return FALSE;

    } else {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     "Unhandled file type: %u", file_type);
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

        g_autoptr(GFileInfo) info = g_file_query_info (gfile, FILE_ATTRS, 0, NULL, error);
        if (info == NULL)
            return FALSE;

        success = add_file_info (self, file, info,
                                 gcab_file_get_name (file), recurse, error);
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
gcab_folder_new_steal_cfolder (cfolder_t **cfolder)
{
    g_return_val_if_fail (cfolder != NULL, NULL);

    GCabFolder *self = g_object_new (GCAB_TYPE_FOLDER,
                                     "comptype", (*cfolder)->typecomp,
                                     NULL);
    self->cfolder = g_steal_pointer (cfolder);

    return self;
}

/**
 * gcab_folder_get_files:
 * @cabfolder: a #GCabFolder
 *
 * Get the list of #GCabFile files contained in the @cabfolder.
 *
 * Returns: (element-type GCabFile) (transfer container): list of files
 **/
GSList *
gcab_folder_get_files (GCabFolder *self)
{
    g_return_val_if_fail (GCAB_IS_FOLDER (self), 0);

    return g_slist_reverse (g_slist_copy (self->files));
}

/**
 * gcab_folder_get_file_by_name:
 * @cabfolder: a #GCabFolder
 * @name: a file name
 *
 * Gets a specific #GCabFile files contained in the @cabfolder.
 *
 * Returns: (transfer none): A #GCabFile, or %NULL if not found
 **/
GCabFile *
gcab_folder_get_file_by_name (GCabFolder *self, const gchar *name)
{
    GCabFile *cabfile;

    g_return_val_if_fail (GCAB_IS_FOLDER (self), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    /* try the hash first */
    cabfile = g_hash_table_lookup (self->hash, name);
    if (cabfile != NULL)
        return cabfile;

    /* if the extract name is different, look for that too */
    for (GSList *l = self->files; l != NULL; l = l->next) {
        cabfile = GCAB_FILE (l->data);
        if (gcab_file_get_name (cabfile) != gcab_file_get_extract_name (cabfile)) {
            if (g_strcmp0 (gcab_file_get_extract_name (cabfile), name) == 0)
                return cabfile;
        }
    }

    /* nothing found */
    return NULL;
}

static gint
sort_by_offset (GCabFile *a, GCabFile *b)
{
    g_return_val_if_fail (a != NULL, 0);
    g_return_val_if_fail (b != NULL, 0);

    return (gint64) gcab_file_get_uoffset (a) - (gint64) gcab_file_get_uoffset (b);
}

G_GNUC_INTERNAL gboolean
gcab_folder_extract (GCabFolder *self,
                     GDataInputStream *data,
                     GFile *path_extract,
                     guint8 res_data,
                     GCabFileCallback file_callback,
                     GFileProgressCallback progress_callback,
                     gpointer callback_data,
                     GCancellable *cancellable,
                     GError **error)
{
    GSList *f = NULL;
    g_autoptr(GSList) files = NULL;
    g_autoptr(cdata_t) cdata = g_new0 (cdata_t, 1);
    guint32 nubytes = 0;
    guint8 *reserved;

    /* never loaded from a stream */
    g_assert (self->cfolder != NULL);

    if (!g_seekable_seek (G_SEEKABLE (data), self->cfolder->offsetdata, G_SEEK_SET, cancellable, error))
        return FALSE;

    files = g_slist_sort (g_slist_copy (self->files), (GCompareFunc)sort_by_offset);

    /* this is allocated for every block, but currently unused */
    cdata->reserved = reserved = g_malloc (res_data);

    for (f = files; f != NULL; f = f->next) {
        GCabFile *file = f->data;

        if (file_callback && !file_callback (file, callback_data))
            continue;

        g_autoptr(GOutputStream) out = NULL;
        out = gcab_file_get_output_stream (file, path_extract, cancellable, error);
        if (out == NULL)
            return FALSE;

        guint32 usize = gcab_file_get_usize (file);
        guint32 uoffset = gcab_file_get_uoffset (file);

        /* let's rewind if need be */
        if (uoffset < nubytes) {
            if (!g_seekable_seek (G_SEEKABLE (data), self->cfolder->offsetdata,
                                  G_SEEK_SET, cancellable, error))
                return FALSE;
            memset(cdata, 0x0, sizeof(cdata_t));
            cdata->reserved = reserved;
            nubytes = 0;
        }

        while (usize > 0) {
            if ((nubytes + cdata->nubytes) <= uoffset) {
                nubytes += cdata->nubytes;
                if (!cdata_read (cdata, res_data, self->comptype,
                                 data, cancellable, error))
                    return FALSE;
                continue;
            } else {
                gsize offset = gcab_file_get_uoffset (file) > nubytes ?
                    gcab_file_get_uoffset (file) - nubytes : 0;
                const void *p = &cdata->out[offset];
                gsize count = MIN (usize, cdata->nubytes - offset);
                if (!g_output_stream_write_all (G_OUTPUT_STREAM (out), p, count,
                                                NULL, cancellable, error))
                    return FALSE;
                usize -= count;
                uoffset += count;
            }
        }

        if (!g_output_stream_close (out, cancellable, error))
            return FALSE;

        /* no backing GFile */
        if (path_extract == NULL) {
            g_autoptr(GBytes) blob = NULL;
            blob = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
            gcab_file_set_bytes (file, blob);
        }

    }

    return TRUE;
}
