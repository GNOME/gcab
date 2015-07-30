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

struct _GCabFileClass
{
    GObjectClass parent_class;
};

enum {
    PROP_0,

    PROP_NAME,
    PROP_FILE,
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

    g_object_unref (self->file);
    g_free (self->name);
    g_free (self->extract_name);

    G_OBJECT_CLASS (gcab_file_parent_class)->finalize (object);
}

static void
gcab_file_set_name (GCabFile *self, const gchar *name)
{
    gchar *fname = g_strdup (name);

    /* assuming that on win32 we don't get unix paths */
#ifndef G_OS_WIN32
    if (fname) {
        int i, len = strlen (fname);
        for (i = 0; i < len; i++)
            if (fname[i] == '/')
                fname[i] = '\\';
    }
#endif

    g_free (self->name);
    self->name = fname;
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
        g_value_set_string (value, self->name);
        break;
    case PROP_FILE:
        g_value_set_object (value, self->file);
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
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (object_class, PROP_FILE,
        g_param_spec_object ("file", "file", "file", G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));
}

G_GNUC_INTERNAL gboolean
gcab_file_update_info (GCabFile *self, GFileInfo *info)
{
    GTimeVal tv;
    time_t time;
    struct tm *m; // Use GDateTime when 2.26 in RHEL6

    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);
    g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

    g_file_info_get_modification_time (info, &tv);
    time = tv.tv_sec;
    m = localtime (&time);

    self->cfile.name = self->name;
    self->cfile.usize = g_file_info_get_size (info);
    self->cfile.fattr = GCAB_FILE_ATTRIBUTE_ARCH;
    self->cfile.date = ((m->tm_year + 1900 - 1980 ) << 9 ) +
        ((m->tm_mon+1) << 5 ) + (m->tm_mday);
    self->cfile.time = (m->tm_hour << 11) + (m->tm_min << 5) + (m->tm_sec / 2);

    return TRUE;
}

G_GNUC_INTERNAL gboolean
gcab_file_set_uoffset (GCabFile *self, u4 uoffset)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), FALSE);

    self->cfile.uoffset = uoffset;

    return TRUE;
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

    return self->cfile.usize;
}

/**
 * gcab_file_get_date:
 * @file: a #GCabFile
 * @result: a #GTimeVal to return date
 *
 * Get the file date.
 *
 * Returns: the cabinet file date in @result
 * Since: 0.6
 **/
void
gcab_file_get_date (GCabFile *self, GTimeVal *tv)
{
    struct tm tm;
    guint16 date, time;

    g_return_if_fail (GCAB_IS_FILE (self));
    g_return_if_fail (tv != NULL);

    date = self->cfile.date;
    time = self->cfile.time;

    tm.tm_isdst = -1;
    tm.tm_year  = ((date >> 9) + 1980) - 1900;
    tm.tm_mon   = ((date >> 5) & 0xf) - 1;
    tm.tm_mday  = (date & 0x1f) - 1;

    tm.tm_hour  = (time >> 11);
    tm.tm_min   = ((time >> 5) & 0x3f);
    tm.tm_sec   = (time & 0x1f) * 2;

    tv->tv_sec  = mktime(&tm);
    tv->tv_usec = 0;
}

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

    return self->cfile.fattr;
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

    return self->name;
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
 * Returns: (transfer full): the associated #GFile or %NULL
 **/
GFile *
gcab_file_get_file (GCabFile *self)
{
    g_return_val_if_fail (GCAB_IS_FILE (self), NULL);

    return self->file;
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

    return g_object_new (GCAB_TYPE_FILE,
                         "name", name,
                         "file", file,
                         NULL);
}

G_GNUC_INTERNAL GCabFile *
gcab_file_new_with_cfile (const cfile_t *cfile)
{
    g_return_val_if_fail (cfile != NULL, NULL);

    GCabFile *file = g_object_new (GCAB_TYPE_FILE,
                                   "name", cfile->name,
                                   NULL);
    file->cfile = *cfile;

    return file;
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

    return self->extract_name ? self->extract_name : self->name;
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
