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

typedef unsigned char u1;
typedef unsigned short int u2;
typedef unsigned long int u4;

typedef struct cheader cheader_t;
typedef struct cfolder cfolder_t;
typedef struct cfile cfile_t;
typedef struct cdata cdata_t;

#define DATABLOCKSIZE           32768

#define CFO_START               0x24		/* folder offset */
#define CFI_START               0x2C  	/* file offset */

struct cheader
{
    u4 res1;
    u4 size;
    u4 res2;
    u4 offsetfiles;
    u4 res3;
    u1 versionMIN;
    u1 versionMAJ;
    u2 nfolders;
    u2 nfiles;
    u2 flags;
    u2 setID;
    u2 cabID;
    u2 res_header;
    u1 res_folder;
    u1 res_data;
    guint8 *reserved;
    gchar *cab_prev;
    gchar *disk_prev;
    gchar *cab_next;
    gchar *disk_next;
};

typedef enum {
    CABINET_HEADER_PREV = 0x0001,
    CABINET_HEADER_NEXT = 0x0002,
    CABINET_HEADER_RESERVE = 0x0004,
} CabinetHeaderFlags;

struct cfolder
{
    u4 offsetdata;
    u2 ndatab;
    u2 typecomp;
    guint8 *reserved;
};

struct cfile
{
    u4 usize;
    u4 uoffset;
    u2 index;
    u2 date;
    u2 time;
    u2 fattr;
    gchar *name;
};

struct cdata
{
    u4 checksum;
    u2 ncbytes;
    u2 nubytes;
    guint8 *reserved;
    guint8 in[CAB_INPUTMAX+2];
    guint8 out[CAB_BLOCKMAX];
    /* using zlib */
    z_stream z;
    /* using wine decomp.h */
    FDI_Int fdi;
    fdi_decomp_state decomp;
};

gboolean     cheader_write                      (cheader_t *ch,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cheader_read                       (cheader_t *ch,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfolder_write                      (cfolder_t *cf,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfolder_read                       (cfolder_t *cf,
                                                 u1 res_folder,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfile_write                        (cfile_t *cf,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfile_read                         (cfile_t *cf,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cdata_write                        (cdata_t *cd,
                                                 GDataOutputStream *out,
                                                 int type,
                                                 guint8 *data,
                                                 size_t size,
                                                 gsize *bytes_written,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cdata_read                         (cdata_t *cd,
                                                 u1 res_data,
                                                 gint comptype,
                                                 GDataInputStream *in,
                                                 GCancellable *cancellable,
                                                 GError **error);
void         cdata_finish                       (cdata_t *cd, GError **error);

#endif /* CABINET_H */
