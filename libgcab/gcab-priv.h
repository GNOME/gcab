/*
 * LibGCab
 * Copyright (c) 2012, Marc-André Lureau <marcandre.lureau@gmail.com>
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

#pragma once

#include "config.h"

#include <glib-object.h>

#include "cabinet.h"
#include "gcab-cabinet.h"
#include "gcab-enums.h"
#include "gcab-file.h"
#include "gcab-folder.h"

/* Copied from GStream, FIXME that should be in GLib imho */
#define _GCAB_GET(__data, __idx, __size, __shift) \
    (((guint##__size) (((const guint8 *) (__data))[__idx])) << (__shift))

#define GCAB_READ_UINT32_LE(data)        (_GCAB_GET (data, 3, 32, 24) | \
                                          _GCAB_GET (data, 2, 32, 16) |  \
                                          _GCAB_GET (data, 1, 32,  8) |  \
                                          _GCAB_GET (data, 0, 32,  0))

static inline gboolean
_gcab_folder_get_cdata_range (guint64 nubytes,
                              guint64 uoffset,
                              guint64 usize,
                              guint16 cdata_nubytes,
                              gsize *offset,
                              gsize *count)
{
    guint64 cdata_offset;
    guint64 cdata_count;

    if (uoffset < nubytes || cdata_nubytes == 0)
        return FALSE;

    cdata_offset = uoffset - nubytes;
    if (cdata_offset >= cdata_nubytes)
        return FALSE;

    cdata_count = MIN (usize, (guint64) cdata_nubytes - cdata_offset);
    if (cdata_count == 0)
        return FALSE;

    *offset = (gsize) cdata_offset;
    *count = (gsize) cdata_count;
    return TRUE;
}

GCabFolder *     gcab_folder_new_steal_cfolder       (cfolder_t **cfolder);
GCabFile *       gcab_file_new_steal_cfile           (cfile_t   **cfile);

gboolean         gcab_file_update_info               (GCabFile *file, GFileInfo *info);
guint32          gcab_file_get_uoffset               (GCabFile *file);
gboolean         gcab_file_set_uoffset               (GCabFile *file, guint32 uoffset);
guint32          gcab_file_get_usize                 (GCabFile *file);
GFile           *gcab_file_get_gfile                 (GCabFile *file);
cfile_t         *gcab_file_get_cfile                 (GCabFile *file);
void             gcab_file_add_attribute             (GCabFile *file, guint32 attribute);

gboolean         gcab_file_is_continued              (GCabFile *file);

typedef struct {
    GDataInputStream *data;
    guint32 offsetdata;
    guint16 ndatab;
    guint8 res_data;
} GCabFolderFragment;

typedef struct {
    guint64 max_size;
    guint64 max_ratio;
    guint64 input_size;
    guint64 written;
} GCabExtractContext;

cfolder_t       *gcab_folder_get_cfolder             (GCabFolder *folder);
gboolean         gcab_extract_check_limits           (GCabExtractContext *ctx,
                                                      guint64 count,
                                                      GError **error);

gboolean         gcab_folder_extract                 (GCabFolder *self,
                                                      GDataInputStream *data,
                                                      GFile *path,
                                                      guint8 res_data,
                                                      GPtrArray *continuations,
                                                      GCabFileCallback file_callback,
                                                      GFileProgressCallback progress_callback,
                                                      GCabExtractContext *extract_ctx,
                                                      gpointer callback_data,
                                                      GCancellable *cancellable,
                                                      GError **error);
GInputStream    *gcab_file_get_input_stream          (GCabFile *file,
                                                      GCancellable *cancellable,
                                                      GError **error);
GOutputStream   *gcab_file_get_output_stream         (GCabFile *cabfile,
                                                      GFile *path_extract,
                                                      GCancellable *cancellable,
                                                      GError **error);
