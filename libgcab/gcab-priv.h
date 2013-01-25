#ifndef GCAB_PRIV_H
#define GCAB_PRIV_H

#include <glib-object.h>
#include "glib-compat.h"

#include "cabinet.h"
#include "gcab-file.h"
#include "gcab-folder.h"
#include "gcab-cabinet.h"
#include "gcab-enums.h"

struct _GCabFile
{
    GObject parent_instance;

    gchar *name;
    gchar *extract_name;
    GFile *file;
    cfile_t cfile;
};

struct _GCabFolder
{
    GObject parent_instance;

    GSList *files;
    GHashTable *hash;
    gint comptype;
    GByteArray *reserved;
    cfolder_t cfolder;
    GInputStream *stream;
};

GCabFolder *     gcab_folder_new_with_cfolder        (const cfolder_t *folder, GInputStream *stream);
GCabFile *       gcab_file_new_with_cfile            (const cfile_t *file);

gboolean         gcab_file_update_info               (GCabFile *file, GFileInfo *info);
gboolean         gcab_file_set_uoffset               (GCabFile *file, u4 uoffset);

gsize            gcab_folder_get_ndatablocks         (GCabFolder *folder);
gboolean         gcab_folder_extract                 (GCabFolder *self,
                                                      GFile *path,
                                                      u1 res_data,
                                                      GCabFileCallback file_callback,
                                                      GFileProgressCallback progress_callback,
                                                      gpointer callback_data,
                                                      GCancellable *cancellable,
                                                      GError **error);

#endif /* GCAB_PRIV_H */
