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

#define  CABINET_RDONLY         0x01  /* file is read-only */
#define  CABINET_HIDDEN         0x02  /* file is hidden */
#define  CABINET_SYSTEM         0x04  /* file is a system file */
#define  CABINET_ARCH           0x20  /* file modified since last backup */
#define  CABINET_EXEC           0x40  /* run after extraction */
#define  CABINET_NAME_IS_UTF    0x80  /* szName[] contains UTF */

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
};

struct cfolder
{
    u4 offsetdata;
    u2 ndatab;
    u2 typecomp;
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
    guint8 data[DATABLOCKSIZE*2];
};

gboolean     cheader_write                      (cheader_t *ch,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);
gboolean     cfolder_write                      (cfolder_t *cf,
                                                 GDataOutputStream *out,
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
gboolean     cfile_write                        (cfile_t *cf,
                                                 GDataOutputStream *out,
                                                 GCancellable *cancellable,
                                                 GError **error);

#endif /* CABINET_H */
