#ifndef GCAB_PRIV_H
#define GCAB_PRIV_H

#include <glib-object.h>

#include "cabinet.h"
#include "gcab-file.h"
#include "gcab-folder.h"
#include "gcab-cabinet.h"
#include "gcab-enums.h"

struct _GCabFile
{
    GObject parent_instance;

    gchar *name;
    GFile *file;
    cfile_t cfile;
};

struct _GCabFolder
{
    GObject parent_instance;

    GSList *files;
    GHashTable *hash;
    GCabCompression compression;
    GByteArray *reserved;
};

GCabFolder *     gcab_folder_new_with_cfolder        (const cfolder_t *folder);
GCabFile *       gcab_file_new_with_cfile            (const cfile_t *file);

gboolean         gcab_file_update_info               (GCabFile *file, GFileInfo *info);
gboolean         gcab_file_set_uoffset               (GCabFile *file, u4 uoffset);

gsize            gcab_folder_get_ndatablocks         (GCabFolder *folder);

#endif /* GCAB_PRIV_H */
