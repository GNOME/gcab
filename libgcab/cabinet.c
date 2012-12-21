#include <zlib.h>
#include "gcab-priv.h"

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

#define W1(val) \
    g_data_output_stream_put_byte (out, val, cancellable, error)
#define W2(val) \
    g_data_output_stream_put_uint16 (out, val, cancellable, error)
#define W4(val) \
    g_data_output_stream_put_uint32 (out, val, cancellable, error)
#define WS(val) \
    g_data_output_stream_put_string (out, val, cancellable, error)

G_GNUC_INTERNAL gboolean
cheader_write (cheader_t *ch, GDataOutputStream *out,
               GCancellable *cancellable, GError **error)
{
    if (!W1 ('M') || !W1 ('S') || !W1 ('C') || !W1 ('F') ||
        !W4 (ch->res1) ||
        !W4 (ch->size) ||
        !W4 (ch->res2) ||
        !W4 (ch->offsetfiles) ||
        !W4 (ch->res3) ||
        !W1 (ch->versionMIN = 3) ||
        !W1 (ch->versionMAJ = 1) ||
        !W2 (ch->nfolders) ||
        !W2 (ch->nfiles) ||
        !W2 (ch->flags) ||
        !W2 (ch->setID) ||
        !W2 (ch->cabID))
        return FALSE;

    return TRUE;
}

G_GNUC_INTERNAL gboolean
cfolder_write (cfolder_t *cf, GDataOutputStream *out,
               GCancellable *cancellable, GError **error)
{
    if ((!W4 (cf->offsetdata)) ||
        (!W2 (cf->ndatab)) ||
        (!W2 (cf->typecomp)))
        return FALSE;

    return TRUE;
}


G_GNUC_INTERNAL gboolean
cfile_write (cfile_t *cf, GDataOutputStream *out,
             GCancellable *cancellable, GError **error)
{
    if ((!W4 (cf->usize)) ||
        (!W4 (cf->uoffset)) ||
        (!W2 (cf->index)) ||
        (!W2 (cf->date)) ||
        (!W2 (cf->time)) ||
        (!W2 (cf->fattr)) ||
        (!WS (cf->name) || !W1 (0)))
        return FALSE;

    return TRUE;
}

typedef unsigned long int CHECKSUM;

static CHECKSUM
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

G_GNUC_INTERNAL gboolean
cdata_write (cdata_t *cd, GDataOutputStream *out, int type,
             guint8 *data, size_t size, gsize *bytes_written,
             GCancellable *cancellable, GError **error)
{
    cdata_set(cd, type, data, size);
    CHECKSUM datacsum = compute_checksum(cd->data, cd->ncbytes, 0);
    cd->checksum = compute_checksum ((guint8*)&cd->ncbytes, 4, datacsum);
    GOutputStream *stream = g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (out));

    *bytes_written = 0;

    if ((!W4 (cd->checksum)) ||
        (!W2 (cd->ncbytes)) ||
        (!W2 (cd->nubytes)) ||
        (g_output_stream_write (stream, cd->data, cd->ncbytes, cancellable, error) == -1))
        return FALSE;

    *bytes_written = 4 + 2 + 2 + cd->ncbytes;

    return TRUE;
}
