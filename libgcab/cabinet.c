#include "gcab-priv.h"
#include <glib/gi18n-lib.h>

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

static gboolean
cdata_set (cdata_t *cd, int type, guint8 *data, size_t size)
{
    if (type > GCAB_COMPRESSION_MSZIP) {
        g_critical (_("unsupported compression method %d"), type);
        return FALSE;
    }

    cd->nubytes = size;

    if (type == 0) {
        memcpy (cd->in, data, size);
        cd->ncbytes = size;
    }

    if (type == GCAB_COMPRESSION_MSZIP) {
        z_stream stream = { 0, };

        stream.zalloc = zalloc;
        stream.zfree = zfree;
        if (deflateInit2 (&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return FALSE;
        stream.next_in = data;
        stream.avail_in = size;
        stream.next_out = cd->in + 2;
        stream.avail_out = sizeof (cd->in) - 2;
        /* insert the signature */
        cd->in[0] = 'C';
        cd->in[1] = 'K';
        deflate (&stream, Z_FINISH);
        deflateEnd (&stream);
        cd->ncbytes = stream.total_out + 2;
    }

    return TRUE;
}

static char *
_data_input_stream_read_until (GDataInputStream  *stream,
                               const gchar        *stop_chars,
                               gsize              stop_len,
                               GCancellable       *cancellable,
                               GError            **error)
{
  GBufferedInputStream *bstream;
  gchar *result;

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  result = g_data_input_stream_read_upto (stream, stop_chars, stop_len,
                                          NULL, cancellable, error);

  /* If we're not at end of stream then we have a stop_char to consume. */
  if (result != NULL && g_buffered_input_stream_get_available (bstream) > 0)
    {
      gsize res;
      gchar b;

      res = g_input_stream_read (G_INPUT_STREAM (stream), &b, 1, NULL, NULL);
      g_assert (res == 1);
    }

  return result;
}

#define R1(val) G_STMT_START{                                           \
    val = g_data_input_stream_read_byte (in, cancellable, error);       \
    if (error && *error)                                                \
        goto end;                                                       \
}G_STMT_END
#define R2(val) G_STMT_START{                                           \
    val = g_data_input_stream_read_uint16 (in, cancellable, error);     \
    if (error && *error)                                                \
        goto end;                                                       \
}G_STMT_END
#define R4(val) G_STMT_START{                                           \
    val = g_data_input_stream_read_uint32 (in, cancellable, error);     \
    if (error && *error)                                                \
        goto end;                                                       \
}G_STMT_END
#define RS(val) G_STMT_START{                                   \
    val = _data_input_stream_read_until (in, "\0", 1,           \
                                         cancellable, error);   \
    if (error && *error)                                        \
        goto end;                                               \
    if (!val) {                                                 \
        g_set_error(error, GCAB_ERROR, GCAB_ERROR_FORMAT,       \
                    "Invalid contents");                        \
        goto end;                                               \
    }                                                           \
}G_STMT_END
#define RN(buff, size) G_STMT_START{                                    \
    if (size) {                                                         \
        gint _val = g_input_stream_read (G_INPUT_STREAM (in), buff, size, cancellable, error); \
        if (error && *error)                                            \
            goto end;                                                   \
        if (_val >= 0 && _val < size) {                                 \
            g_set_error(error, GCAB_ERROR, GCAB_ERROR_FORMAT,           \
                        "Expected %d bytes, got %d", size, _val);       \
            goto end;                                                   \
        }                                                               \
        if (_val == -1) {                                               \
            g_set_error(error, GCAB_ERROR, GCAB_ERROR_FORMAT,           \
                        "Invalid contents");                            \
            goto end;                                                   \
        }                                                               \
    }                                                                   \
}G_STMT_END

static void
hexdump (guchar *p, gsize s)
{
    gsize i;

    for (i = 0; i < s; i++) {
        if (i != 0) {
            if (i % 16 == 0)
                g_printerr ("\n");
            else if (i % 8 == 0)
                g_printerr ("  ");
            else
                g_printerr (" ");
        }

        if (i % 16 == 0)
            g_printerr ("%.8x  ", (guint)i);

        g_printerr ("%.2x", (guint)p[i]);
    }

    g_printerr ("\n");
}

#define P1(p, field) \
    g_debug ("%15s: %.2x", #field, (guint)p->field)
#define P2(p, field) \
    g_debug ("%15s: %.4x", #field, (guint)p->field)
#define P4(p, field) \
    g_debug ("%15s: %.8x", #field, (guint)p->field)
#define PS(p, field) \
    g_debug ("%15s: %s", #field, p->field)
#define PN(p, field, size) \
    g_debug ("%15s:", #field), hexdump (p->field, size)
#define PND(p, field, size) \
    g_debug ("%15s:", #field), hexdump (field, size)

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
    GOutputStream *stream = g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (out));

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

    if (ch->flags & CABINET_HEADER_RESERVE) {
        W2 (ch->res_header);
        W1 (ch->res_folder);
        W1 (ch->res_data);
        if (g_output_stream_write (stream, ch->reserved, ch->res_header,
                                   cancellable, error) == -1)
            return FALSE;
    }

    return TRUE;
}

G_GNUC_INTERNAL gboolean
cheader_read (cheader_t *ch, GDataInputStream *in,
              GCancellable *cancellable, GError **error)
{
    gboolean success = FALSE;
    guint8 sig[4];

    R1 (sig[0]);
    R1 (sig[1]);
    R1 (sig[2]);
    R1 (sig[3]);
    if (memcmp (sig, "MSCF", 4)) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FORMAT,
                     "The input is not of cabinet format");
        goto end;
    }

    memset (ch, 0, sizeof (cheader_t));
    R4 (ch->res1);
    R4 (ch->size);
    R4 (ch->res2);
    R4 (ch->offsetfiles);
    R4 (ch->res3);
    R1 (ch->versionMIN);
    R1 (ch->versionMAJ);
    R2 (ch->nfolders);
    R2 (ch->nfiles);
    R2 (ch->flags);
    R2 (ch->setID);
    R2 (ch->cabID);

    if (ch->flags & CABINET_HEADER_RESERVE) {
        R2 (ch->res_header);
        R1 (ch->res_folder);
        R1 (ch->res_data);
        ch->reserved = g_malloc (ch->res_header);
        RN (ch->reserved, ch->res_header);
    }

    if (ch->flags & CABINET_HEADER_PREV) {
        RS (ch->cab_prev);
        RS (ch->disk_prev);
    }

    if (ch->flags & CABINET_HEADER_NEXT) {
        RS (ch->cab_next);
        RS (ch->disk_next);
    }

    if (g_getenv ("GCAB_DEBUG")) {
        g_debug ("CFHEADER");
        P4 (ch, res1);
        P4 (ch, size);
        P4 (ch, res2);
        P4 (ch, offsetfiles);
        P4 (ch, res3);
        P1 (ch, versionMIN);
        P1 (ch, versionMAJ);
        P2 (ch, nfolders);
        P2 (ch, nfiles);
        P2 (ch, flags);
        P2 (ch, setID);
        P2 (ch, cabID);
        if (ch->flags & CABINET_HEADER_RESERVE) {
            P2 (ch, res_header);
            P1 (ch, res_folder);
            P1 (ch, res_data);
            if (ch->res_header)
                PN (ch, reserved, ch->res_header);
        }
        if (ch->flags & CABINET_HEADER_PREV) {
            PS (ch, cab_prev);
            PS (ch, disk_prev);
        }
        if (ch->flags & CABINET_HEADER_NEXT) {
            PS (ch, cab_next);
            PS (ch, disk_next);
        }

    }

    success = TRUE;

end:
    return success;
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
cfolder_read (cfolder_t *cf, u1 res_size, GDataInputStream *in,
              GCancellable *cancellable, GError **error)
{
    gboolean success = FALSE;

    R4 (cf->offsetdata);
    R2 (cf->ndatab);
    R2 (cf->typecomp);
    cf->reserved = g_malloc (res_size);
    RN (cf->reserved, res_size);

    if (g_getenv ("GCAB_DEBUG")) {
        g_debug ("CFOLDER");
        P4 (cf, offsetdata);
        P2 (cf, ndatab);
        P2 (cf, typecomp);
        if (res_size)
            PN (cf, reserved, res_size);
    }

    success = TRUE;

end:
    return success;
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


G_GNUC_INTERNAL gboolean
cfile_read (cfile_t *cf, GDataInputStream *in,
            GCancellable *cancellable, GError **error)
{
    gboolean success = FALSE;

    R4 (cf->usize);
    R4 (cf->uoffset);
    R2 (cf->index);
    R2 (cf->date);
    R2 (cf->time);
    R2 (cf->fattr);
    RS (cf->name);

    if (g_getenv ("GCAB_DEBUG")) {
        g_debug ("CFILE");
        P4 (cf, usize);
        P4 (cf, uoffset);
        P2 (cf, index);
        P2 (cf, date);
        P2 (cf, time);
        P2 (cf, fattr);
        PS (cf, name);
    }

    success = TRUE;

end:
    return success;
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
    if (!cdata_set(cd, type, data, size))
        return FALSE;

    CHECKSUM datacsum = compute_checksum(cd->in, cd->ncbytes, 0);
    cd->checksum = compute_checksum ((guint8*)&cd->ncbytes, 4, datacsum);
    GOutputStream *stream = g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (out));

    *bytes_written = 0;

    if ((!W4 (cd->checksum)) ||
        (!W2 (cd->ncbytes)) ||
        (!W2 (cd->nubytes)) ||
        (g_output_stream_write (stream, cd->in, cd->ncbytes, cancellable, error) == -1))
        return FALSE;

    *bytes_written = 4 + 2 + 2 + cd->ncbytes;

    return TRUE;
}

G_GNUC_INTERNAL void
cdata_finish (cdata_t *cd, GError **error)
{
    z_stream *z = &cd->z;
    int zret;

    if (!z->opaque)
        return;

    zret = inflateEnd (z);
    z->opaque = NULL;

    if (zret != Z_OK)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "zlib failed: %s", zError (zret));
}

G_GNUC_INTERNAL gboolean
cdata_read (cdata_t *cd, u1 res_data, gint comptype,
            GDataInputStream *in, GCancellable *cancellable, GError **error)

{
    gboolean success = FALSE;
    int ret, zret = Z_OK;
    gint compression = comptype & GCAB_COMPRESSION_MASK;
    guint8 *buf = compression == GCAB_COMPRESSION_NONE ? cd->out : cd->in;
    CHECKSUM datacsum;

    if (compression > GCAB_COMPRESSION_MSZIP &&
        compression != GCAB_COMPRESSION_LZX) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     _("unsupported compression method %d"), compression);
        return FALSE;
    }

    R4 (cd->checksum);
    R2 (cd->ncbytes);
    R2 (cd->nubytes);
    cd->reserved = g_malloc (res_data);
    RN (cd->reserved, res_data);
    RN (buf, cd->ncbytes);

    datacsum = compute_checksum(buf, cd->ncbytes, 0);
    if (cd->checksum != compute_checksum ((guint8*)&cd->ncbytes, 4, datacsum)) {
        g_set_error_literal (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                             _("incorrect checksum detected"));
        return FALSE;
    }

    if (g_getenv ("GCAB_DEBUG")) {
        g_debug ("CDATA");
        P4 (cd, checksum);
        P2 (cd, ncbytes);
        P2 (cd, nubytes);
        if (res_data)
            PN (cd, reserved, res_data);
        PND (cd, buf, 64);
    }

    if (compression == GCAB_COMPRESSION_LZX) {
        if (cd->fdi.alloc == NULL) {
            cd->fdi.alloc = g_malloc;
            cd->fdi.free = g_free;
            cd->decomp.fdi = &cd->fdi;
            cd->decomp.inbuf = cd->in;
            cd->decomp.outbuf = cd->out;

            ret = LZXfdi_init((comptype >> 8) & 0x1f, &cd->decomp);
            if (ret < 0)
                goto end;
        }

        ret = LZXfdi_decomp (cd->ncbytes, cd->nubytes, &cd->decomp);
        if (ret < 0)
            goto end;
    }

    if (compression == GCAB_COMPRESSION_MSZIP) {
        if (cd->in[0] != 'C' || cd->in[1] != 'K')
            goto end;

        z_stream *z = &cd->z;

        z->avail_in = cd->ncbytes - 2;
        z->next_in = cd->in + 2;
        z->avail_out = cd->nubytes;
        z->next_out = cd->out;
        z->total_out = 0;

        if (!z->opaque) {
            z->zalloc = zalloc;
            z->zfree = zfree;
            z->opaque = cd;

            zret = inflateInit2 (z, -MAX_WBITS);
            if (zret != Z_OK)
                goto end;
        }

        while (1) {
            zret = inflate (z, Z_BLOCK);
            if (zret == Z_STREAM_END)
                break;
            if (zret != Z_OK)
                goto end;
        }

        g_warn_if_fail (z->avail_in == 0);
        g_warn_if_fail (z->avail_out == 0);
        if (z->avail_in != 0 || z->avail_out != 0)
            goto end;

        zret = inflateReset (z);
        if (zret != Z_OK)
            goto end;

        zret = inflateSetDictionary (z, cd->out, cd->nubytes);
        if (zret != Z_OK)
            goto end;
    }

    success = TRUE;

end:
    if (zret != Z_OK)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "zlib failed: %s", zError (zret));

    if (!*error && !success)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Invalid cabinet chunk");

    return success;
}
