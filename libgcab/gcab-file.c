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

#include "gcab-priv.h"
#include "gcab-file.h"

/**
 * SECTION:gcab-file
 * @title: GCabFile
 * @short_description: A file contained in the Cabinet
 * @see_also: #GCabFolder
 * @stability: Stable
 * @include: libgcab.h
 *
 * A GCabFile is a handle to a file inside a Cabinet archive.
 * It can either be a file that is already within an exisiting
 * archive, or a file that reference a file on disk that will be used
 * for a new archive creation. In the later case, gcab_file_get_file()
 * must return a valid handle.
 */

struct _GCabFile
{
    GObject parent_instance;

    gchar *extract_name;
    GFile *file;
    GBytes *bytes;
    cfile_t *cfile;
};

enum {
    PROP_0,

    PROP_NAME,
    PROP_FILE,
    PROP_BYTES,
};

G_DEFINE_TYPE (GCabFile, gcab_file, G_TYPE_OBJECT);

static void
gcab_file_init (GCabFile *self)
{
}

static void
gcab_file_finalize (GObject *object)
{
    GCabFile *self = GCAB_FILE (object);

    if (self->file != NULL)
        g_object_unref (self->file);
    if (self->bytes != NULL)
        g_bytes_unref (self->bytes);
    cfile_free (self->cfile);
    g_free (self->extract_name);

    G_OBJECT_CLASS (gcab_file_parent_class)->finalize (object);
}

static void
gcab_file_set_name (GCabFile *self, const gchar *name)
{
    gchar *fname = g_strdup (name);

    g_return_if_fail (self->cfile != NULL);

    /* assuming that on win32 we don't get unix paths */
#ifndef G_OS_WIN32
    if (fname) {
        int i, len = strlen (fname);
        for (i = 0; i < len; i++)
            if (fname[i] == '/')
                fname[i] = '\\';
    }
#endif

    g_free (self->cfile->name);
    self->cfile->name = fname;
}

/**
 * gcab_file_set_bytes:
 * @file: a #GCabFile
 * @bytes: a #GBytes
 *
 * Replace the #GBytes associated with @self.
 * This is most usefule when the #GCabFile has been created using
 * gcab_file_new_with_bytes() and the data needs to be modified.
 *
 * Since: 1.5
 **/
void
gcab_file_set_bytes (GCabFile *self, GBytes *bytes)
{
    if (self->bytes != NULL)
        g_bytes_unref (self->bytes);
    self->bytes = g_bytes_ref (bytes);

    /* this is embedded into the archive */
    self->cfile->usize = g_bytes_get_size (bytes);
}

static void
gcab_file_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FILE (object));
    GCabFile *self = GCAB_FILE (object);

    switch (prop_id) {
    case PROP_NAME:
        gcab_file_set_name (self, g_value_get_string (value));
        break;
    case PROP_FILE:
        self->file = g_value_dup_object (value);
        break;
    case PROP_BYTES:
        gcab_file_set_bytes (self, g_value_get_boxed (value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
gcab_file_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FILE (object));
    GCabFile *self = GCAB_FILE(object);

    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string (value, self->cfile->name);
        break;
    case PROP_FILE:
        g_value_set_object (value, self->file);
        break;
    case PROP_BYTES:
        g_value_set_boxed (value, self->bytes);
        break;
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

    g_object_class_install_property (object_class, PROP_NAME,
        g_param_spec_string ("name", "name", "name", NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_FILE,
        g_param_spec_object ("file", "file", "file", G_TYPE_FILE,
                             G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_BYTES,
        g_param_spec_boxed ("bytes", "bytes", "bytes", G_TYPE_BYTES,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS));
}

/**
 * gcab_file_set_date_time:
 * @file: a #GCabFile
 * @dt: a #GDateTime
 *
 * Sets the file modification date (instead of the date provided by the GFile)
 *
 * Since: 1.4
 **/
void
gcab_file_set_date_time (GCabFile *self, GDateTime *dt)
{
    g_return_if_fail (GCAB_IS_FILE (self));
    g_return_if_fail (dt != NULL);

    self->cfile->date = ((g_date_time_get_year (dt) - 1980 ) << 9 ) +
        ((g_date_time_get_month (dt)) << 5) +
        (g_date_time_get_day_of_month (dt));
    self->cfile->time = ((g_date_time_get_hour (dt)) << 11) +
        (g_date_time_get_minute (dt) << 5) +
        (g_date_time_get_second (dt) / 2);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/**
 * gcab_file_set_date:
 * @file: a #GCabFile
 * @tv: a #GTimeVal
 *
 * Sets the file modification date, instead of the value provided by the GFile.
 *
 * Since: 1.0
 **/
void
gcab_file_set_date (GCabFile *self, const GTimeVal *tv)
{
    g_autoptr(GDateTime) dt = g_date_time_new_from_timeval_utc (tv);

    gcab_file_set_date_time (self, dt);
}
G_GNUC_END_IGNORE_DEPRECATIONS

G_GNUC_INTERNAL gboolean
gcab_file_update_info (GCabFile *self, GFileInfo *info)
{
    g_autoptr(GDateTime) dt = NULL;

    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);
    g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

    dt = g_file_info_get_modification_date_time (info);
    if (self->cfile->date == 0)
        gcab_file_set_date_time (self, dt);
    self->cfile->usize = g_file_info_get_size (info);
    self->cfile->fattr = GCAB_FILE_ATTRIBUTE_ARCH;

    return TRUE;
}

G_GNUC_INTERNAL gboolean
gcab_file_set_uoffset (GCabFile *self, guint32 uoffset)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);

    self->cfile->uoffset = uoffset;

    return TRUE;
}

G_GNUC_INTERNAL guint32
gcab_file_get_uoffset (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), 0);
    return self->cfile->uoffset;
}

G_GNUC_INTERNAL guint32
gcab_file_get_usize (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), 0);
    return self->cfile->usize;
}

G_GNUC_INTERNAL GFile *
gcab_file_get_gfile (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);
    return self->file;
}

G_GNUC_INTERNAL void
gcab_file_add_attribute (GCabFile *self, guint32 attribute)
{
    g_return_if_fail (GCAB_IS_FILE (self));
    self->cfile->fattr |= attribute;
}

G_GNUC_INTERNAL cfile_t *
gcab_file_get_cfile (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);
    return self->cfile;
}

/**
 * gcab_file_get_size:
 * @file: a #GCabFile
 *
 * Get the file size.
 *
 * Returns: the cabinet file size
 * Since: 0.6
 **/
guint32
gcab_file_get_size (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), 0);

    return self->cfile->usize;
}

/**
 * gcab_file_get_date_time:
 * @file: a #GCabFile
 *
 * Gets the file date and returns it as a #GDateTime..
 *
 * Since: 1.4
 *
 * Returns: file date, or NULL if unknown.
 **/
GDateTime *
gcab_file_get_date_time (GCabFile *self)
{
    guint16 date, time;
    g_autoptr(GDateTime) dt = NULL;

    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);

    date = self->cfile->date;
    time = self->cfile->time;
    dt = g_date_time_new_utc ((date >> 9) + 1980,
                              (date >> 5) & 0xf,
                              (date & 0x1f),
                              (time >> 11),
                              (time >> 5) & 0x3f,
                              (time & 0x1f) * 2);
    return g_steal_pointer (&dt);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/**
 * gcab_file_get_date:
 * @file: a #GCabFile
 * @result: a #GTimeVal to return date
 *
 * Get the file date, in @result.
 *
 * Since: 0.6
 *
 * Returns: %TRUE if @tv was set
 **/
gboolean
gcab_file_get_date (GCabFile *self, GTimeVal *tv)
{
    g_autoptr(GDateTime) dt = NULL;

    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);
    g_return_val_if_fail (tv != NULL, FALSE);

    dt = gcab_file_get_date_time (self);
    if (dt == NULL)
        return FALSE;

    return g_date_time_to_timeval (dt, tv);
}
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * gcab_file_get_attributes:
 * @file: a #GCabFile
 *
 * Get the file attributes.
 *
 * Returns: the cabinet file attributes
 * Since: 0.6
 **/
guint32
gcab_file_get_attributes (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), 0);

    return self->cfile->fattr;
}

/**
 * gcab_file_set_attributes:
 * @file: a #GCabFile
 * @attr: the attributes, e.g. %GCAB_FILE_ATTRIBUTE_RDONLY
 *
 * Set the file attributes.
 *
 * Since: 1.0
 **/
void
gcab_file_set_attributes (GCabFile *self, guint32 attr)
{
    g_return_if_fail (GCAB_IS_FILE (self));
    self->cfile->fattr = attr;
}

/**
 * gcab_file_get_name:
 * @file: a #GCabFile
 *
 * Get the file name within the cabinet.
 *
 * Returns: the cabinet file name
 **/
const gchar *
gcab_file_get_name (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);

    return self->cfile->name;
}

/**
 * gcab_file_get_file:
 * @file: a #GCabFile
 *
 * If the cabinet is being created, get the #GFile associated with
 * @file. This must be an exisiting file that can be read, in order to
 * be added to the archive during cabinet creation.
 *
 * If @file is from an existing cabinet, the fuction will return
 * %NULL.
 *
 * Returns: (transfer none): the associated #GFile or %NULL
 **/
GFile *
gcab_file_get_file (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);

    return self->file;
}

/**
 * gcab_file_get_bytes:
 * @file: a #GCabFile
 *
 * Get the #GFile associated with @file. This will only be non-%NULL if the
 * #GCabFile has been created using gcab_file_new_with_bytes().
 *
 * Returns: (transfer none): the associated #GBytes or %NULL
 *
 * Since: 1.0
 **/
GBytes *
gcab_file_get_bytes (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);
    return self->bytes;
}

/**
 * gcab_file_new_with_file:
 * @name: name of the file within the cabinet
 * @file: a #GFile to be added to the cabinet
 *
 * Create a #GCabFile from a given #GFile, to be added to a
 * #GCabCabinet for archive creation.
 *
 * Returns: a new #GCabFile
 **/
GCabFile *
gcab_file_new_with_file (const gchar *name, GFile *file)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (G_IS_FILE (file), NULL);

    GCabFile *self = g_object_new (GCAB_TYPE_FILE,
                                   "file", file,
                                   NULL);
    self->cfile = g_new0 (cfile_t, 1);
    gcab_file_set_name (self, name);
    return self;
}

/**
 * gcab_file_new_with_bytes:
 * @name: name of the file within the cabinet
 * @bytes: a #GBytes to be added to the cabinet
 *
 * Create a #GCabFile from a given #GBytes.
 *
 * If this file is to be added to an archive you should also probably use
 * gcab_file_set_date() and gcab_file_set_attributes() to set sensible values.
 *
 * Returns: a new #GCabFile
 *
 * Since: 1.0
 **/
GCabFile *
gcab_file_new_with_bytes (const gchar *name, GBytes *bytes)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (bytes != NULL, NULL);

    GCabFile *self = g_object_new (GCAB_TYPE_FILE, NULL);
    self->cfile = g_new0 (cfile_t, 1);
    gcab_file_set_bytes (self, bytes);
    gcab_file_set_name (self, name);
    return self;
}

G_GNUC_INTERNAL GCabFile *
gcab_file_new_steal_cfile (cfile_t **cfile)
{
    g_return_val_if_fail (cfile != NULL, NULL);

    GCabFile *file = g_object_new (GCAB_TYPE_FILE, NULL);
    file->cfile = g_steal_pointer (cfile);

    return file;
}

G_GNUC_INTERNAL GInputStream *
gcab_file_get_input_stream (GCabFile *self, GCancellable *cancellable, GError **error)
{
    /* backed by a GFile */
    if (self->file != NULL)
        return G_INPUT_STREAM (g_file_read (self->file, cancellable, error));

    /* backed by a GBytes */
    if (self->bytes != NULL)
        return g_memory_input_stream_new_from_bytes (self->bytes);

    /* nothing to do */
    g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                 "No GFile for %s", gcab_file_get_name (self));
    return NULL;
}

G_GNUC_INTERNAL GOutputStream *
gcab_file_get_output_stream (GCabFile *self,
                             GFile *path_extract,
                             GCancellable *cancellable,
                             GError **error)
{
    /* not writing to a GFile */
    if (path_extract == NULL)
        return g_memory_output_stream_new_resizable ();

    /* make path have UNIX directory slashes */
    g_autofree gchar *fname = g_strdup (gcab_file_get_extract_name (self));
    g_strdelimit (fname, "\\", '/');

    /* "Rebase" the file in the given path, to ensure we never escape it */
    g_autoptr(GFile) file = g_file_resolve_relative_path (path_extract, fname);
    if (!g_file_has_prefix (file, path_extract)) {
        g_autofree gchar *rawpath = g_file_get_path (file);
        if (rawpath != NULL) {
            char *newpath = rawpath;
            while (*newpath != 0 && *newpath == G_DIR_SEPARATOR) {
                newpath++;
            }
            g_autoptr(GFile) newfile = g_file_resolve_relative_path (path_extract, newpath);
            g_set_object (&file, newfile);
        }
    }

    /* create parent directories */
    g_autoptr(GFile) parent = g_file_get_parent (file);
    g_autoptr(GError) error_local = NULL;
    if (!g_file_make_directory_with_parents (parent, cancellable, &error_local)) {
        if (!g_error_matches (error_local, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
            g_propagate_error (error, g_steal_pointer (&error_local));
            return NULL;
        }
    }

    /* write to a file */
    return G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
                                            G_FILE_CREATE_REPLACE_DESTINATION,
                                            cancellable, error));
}

/**
 * gcab_file_get_extract_name:
 * @file: a #GCabFile
 *
 * Get the file name to use for extraction, or %NULL.
 *
 * Returns: (allow-none): a file name
 **/
const gchar *
gcab_file_get_extract_name (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);

    return self->extract_name ? self->extract_name : self->cfile->name;
}

/**
 * gcab_file_set_extract_name:
 * @file: a #GCabFile
 * @name: (allow-none): a file name or %NULL
 *
 * Sets the file name to use for extraction, instead of the name
 * provided by the Cabinet.
 **/
void
gcab_file_set_extract_name (GCabFile *self, const gchar *name)
{
    g_return_if_fail (GCAB_IS_FILE (self));

    g_free (self->extract_name);
    self->extract_name = g_strdup (name);
}
