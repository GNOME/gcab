#include "gcab-priv.h"

struct _GCabFolderClass
{
    GObjectClass parent_class;
};

enum {
    PROP_0,

    PROP_COMPRESSION,
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

    G_OBJECT_CLASS (gcab_folder_parent_class)->finalize (object);
}

static void
gcab_folder_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (GCAB_IS_FOLDER (object));
    GCabFolder *self = GCAB_FOLDER (object);

    switch (prop_id) {
    case PROP_COMPRESSION:
        self->compression = g_value_get_enum (value);
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
        g_value_set_enum (value, self->compression);
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
add_file (GCabFolder *self, GCabFile *file)
{
    if (g_hash_table_lookup (self->hash, (gpointer)gcab_file_get_name (file)))
        return FALSE;

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
        if (!add_file (self, file))
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
        success = add_file (self, file);
    }

    return success;
}

/**
 * gcab_folder_get_nfiles:
 * @cabfolder: a #GCabFolder
 *
 * Returns: Number of files in this @folder.
 **/
guint
gcab_folder_get_nfiles (GCabFolder *self)
{
    g_return_val_if_fail (GCAB_IS_FOLDER (self), 0);

    return g_hash_table_size (self->hash);
}

/**
 * gcab_folder_new:
 * @compression: compression used by the folder
 *
 * Returns: a new #GCabFolder
 **/
GCabFolder *
gcab_folder_new (GCabCompression compression)
{
    return g_object_new (GCAB_TYPE_FOLDER,
                         "compression", compression,
                         NULL);
}

G_GNUC_INTERNAL GCabFolder *
gcab_folder_new_with_cfolder (const cfolder_t *folder)
{
    return g_object_new (GCAB_TYPE_FOLDER,
                         "compression", folder->typecomp,
                         NULL);
}

/**
 * gcab_folder_get_files:
 * @cabfolder: a #GCabFolder
 *
 * Returns: (element-type GCabFile) (transfer full): list of files
 **/
GSList *
gcab_folder_get_files (GCabFolder *self)
{
    g_return_val_if_fail (GCAB_IS_FOLDER (self), 0);

    return g_slist_reverse (g_slist_copy (self->files));
}
