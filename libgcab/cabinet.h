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

#ifndef CABINET_H
#define CABINET_H

#include <glib.h>
#include <gio/gio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <zlib.h>

#include "gcab-folder.h"
#include "decomp.h"

/* based on the spec
   http://msdn.microsoft.com/en-us/library/bb417343.aspx */

#define CAB_MAX_BLOCK_SIZE                (32768)
#define CAB_MAX_MSZIP_BLOCK_SIZE          (32768 + 12)
#define CAB_MAX_LZX_BLOCK_SIZE            (32768 + 6144)
#define CAB_MAX_COMPRESSED_BLOCK_SIZE     (CAB_MAX_LZX_BLOCK_SIZE)

#define CFO_START               0x24    /* folder offset */
#define CFI_START               0x2C    /* file offset */

typedef struct
{
    guint32 res1;
    guint32 size;
    guint32 res2;
    guint32 offsetfiles;
    guint32 res3;
    guint8 versionMIN;
    guint8 versionMAJ;
    guint16 nfolders;
    guint16 nfiles;
    guint16 flags;
    guint16 setID;
    guint16 cabID;
    guint16 res_header;
    guint8 res_folder;
    guint8 res_data;
    guint8 *reserved;
    gchar *cab_prev;
    gchar *disk_prev;
    gchar *cab_next;
    gchar *disk_next;
} cheader_t;

typedef enum {
    CABINET_HEADER_PREV = 0x0001,
    CABINET_HEADER_NEXT = 0x0002,
    CABINET_HEADER_RESERVE = 0x0004,
} CabinetHeaderFlags;

typedef struct
{
    guint32 offsetdata;
    guint16 ndatab;
    guint16 typecomp;
    guint8 *reserved;
} cfolder_t;

typedef struct
{
    guint32 usize;
    guint32 uoffset;
    guint16 index;
    guint16 date;
    guint16 time;
    guint16 fattr;
    gchar *name;
} cfile_t;

typedef struct
{
    guint32 checksum;
    guint16 ncbytes;
    guint16 nubytes;
    guint8 *reserved;
    guint8 in[CAB_MAX_COMPRESSED_BLOCK_SIZE];
    guint8 out[CAB_MAX_BLOCK_SIZE];
    /* using zlib */
    z_stream z;
    /* using wine decomp.h */
    FDI_Int fdi;
    fdi_decomp_state decomp;
} cdata_t;

gboolean     cheader_write                      (cheader_t *ch,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cheader_read                       (cheader_t *ch,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
void         cheader_free                       (cheader_t *ch);

gboolean     cfolder_write                      (cfolder_t *cf,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfolder_read                       (cfolder_t *cf,
                                                 guint8 res_folder,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
void         cfolder_free                       (cfolder_t *cf);

gboolean     cfile_write                        (cfile_t *cf,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfile_read                         (cfile_t *cf,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
void         cfile_free                         (cfile_t *cf);

gboolean     cdata_write                        (cdata_t *cd,
                                                 GDataOutputStream *out,
                                                 int type,
                                                 guint8 *data,
                                                 size_t size,
                                                 gsize *bytes_written,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cdata_read                         (cdata_t *cd,
                                                 guint8 res_data,
                                                 gint comptype,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
void         cdata_free                         (cdata_t *cd);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(cfolder_t, cfolder_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(cfile_t, cfile_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(cheader_t, cheader_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(cdata_t, cdata_free)

#endif /* CABINET_H */
