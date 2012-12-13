#include <zlib.h>
#include "gcab-file.h"
#include "cabinet.h"

static voidpf
zalloc (voidpf opaque, uInt items, uInt size)
{
    return g_malloc (items *size);
}
static void
zfree (voidpf opaque, voidpf address)
{
    if (address)
        g_free (address);
}

static void
cdata_set (cdata_t *cd, int type, guint8 *data, size_t size)
{
    cd->nubytes = size;

    if (type == 0) {
        memcpy (cd->data, data, size);
        cd->ncbytes = size;
    }

    if (type == GCAB_COMPRESSION_MSZIP) {
        z_stream stream;

        stream.zalloc = zalloc;
        stream.zfree = zfree;
        if (deflateInit2 (&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            exit(1);
        stream.next_in = data;
        stream.avail_in = size;
        stream.next_out = cd->data + 2;
        stream.avail_out = sizeof (cd->data) - 2;
        /* insert the signature */
        cd->data[0] = 'C';
        cd->data[1] = 'K';
        deflate (&stream, Z_FINISH);
        deflateEnd (&stream);
        cd->ncbytes = stream.total_out + 2;
    }
}

#define W(data, count) \
    g_output_stream_write (out, data, count, NULL, error)

gssize
cheader_write (cheader_t *ch, GOutputStream *out, GError **error)
{
    ch->versionMIN = 3;
    ch->versionMAJ = 1;

    if ((W ("MSCF", 4) == -1) ||
        (W (&ch->res1, 4) == -1) ||
        (W (&ch->size, 4) == -1) ||
        (W (&ch->res2, 4) == -1) ||
        (W (&ch->offsetfiles, 4) == -1) ||
        (W (&ch->res3, 4) == -1) ||
        (W (&ch->versionMIN, 1) == -1) ||
        (W (&ch->versionMAJ, 1) == -1) ||
        (W (&ch->nfolders, 2) == -1) ||
        (W (&ch->nfiles, 2) == -1) ||
        (W (&ch->flags, 2) == -1) ||
        (W (&ch->setID, 2) == -1) ||
        (W (&ch->cabID, 2) == -1))
        return -1;

    return 4 + 4 + 4 + 4 + 4 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 2;
}

gssize
cfolder_write (cfolder_t *cf, GOutputStream *out, GError **error)
{
    if ((W (&cf->offsetdata, 4) == -1) ||
        (W (&cf->ndatab, 2) == -1) ||
        (W (&cf->typecomp, 2) == -1))
        return -1;

    return 4 + 2 + 2;
}


gssize
cfile_write (cfile_t *cf, GOutputStream *out, GError **error)
{
    const gchar *name = cf->name;

    if ((W (&cf->usize, 4) == -1) ||
        (W (&cf->uoffset, 4) == -1) ||
        (W (&cf->index, 2) == -1) ||
        (W (&cf->date, 2) == -1) ||
        (W (&cf->time, 2) == -1) ||
        (W (&cf->fattr, 2) == -1) ||
        (W (name, strlen (name) + 1) == -1))
        return -1;

    return 4 + 4 + 2 + 2 + 2 + 2 + strlen (name) + 1;
}

typedef unsigned long int CHECKSUM;

CHECKSUM
compute_checksum (guint8 *in, u2 ncbytes, CHECKSUM seed)
{
    int no_ulongs;
    CHECKSUM csum=0;
    CHECKSUM temp;

    no_ulongs = ncbytes / 4;
    csum = seed;

    while (no_ulongs-- > 0) {
        temp = ((CHECKSUM) (*in++));
        temp |= (((CHECKSUM) (*in++)) << 8);
        temp |= (((CHECKSUM) (*in++)) << 16);
        temp |= (((CHECKSUM) (*in++)) << 24);

        csum ^= temp;
    }

    temp = 0;
    switch (ncbytes % 4) {
    case 3: temp |= (((CHECKSUM) (*in++)) << 16);
    case 2: temp |= (((CHECKSUM) (*in++)) << 8);
    case 1: temp |= ((CHECKSUM) (*in++));
    default: break;
    }

    csum ^= temp;

    return csum;
}

gssize
cdata_write(cdata_t *cd, GOutputStream *out, int type, guint8 *data, size_t size, GError **error)
{
    cdata_set(cd, type, data, size);
    CHECKSUM datacsum = compute_checksum(cd->data, cd->ncbytes, 0);
    cd->checksum = compute_checksum ((guint8*)&cd->ncbytes, 4, datacsum);

    if ((W (&cd->checksum, 4) == -1) ||
        (W (&cd->ncbytes, 2) == -1) ||
        (W (&cd->nubytes, 2) == -1) ||
        (W (cd->data, cd->ncbytes) == -1))
        return -1;

    return 4 + 2 + 2 + cd->ncbytes;
}
