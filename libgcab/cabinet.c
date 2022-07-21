/*
 * LibGCab
 * Copyright (c) 2012, Marc-André Lureau <marcandre.lureau@gmail.com>
 * Copyright (c) 2017, Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include "gcab-priv.h"

static voidpf
zalloc (voidpf opaque, uInt items, uInt size)
{
    return g_malloc (items *size);
}
static void
zfree (voidpf opaque, voidpf address)
{
    g_free (address);
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
        if (!W2 (ch->res_header) ||
            !W1 (ch->res_folder) ||
            !W1 (ch->res_data))
                return FALSE;
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

void
cheader_free (cheader_t *ch)
{
    if (ch == NULL)
        return;
    g_free (ch->reserved);
    g_free (ch->cab_prev);
    g_free (ch->disk_prev);
    g_free (ch->cab_next);
    g_free (ch->disk_next);
    g_free (ch);
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
cfolder_read (cfolder_t *cf, guint8 res_size, GDataInputStream *in,
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

void
cfolder_free (cfolder_t *cf)
{
    if (cf == NULL)
        return;
    g_free (cf->reserved);
    g_free (cf);
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

void
cfile_free (cfile_t *cf)
{
    if (cf == NULL)
        return;
    g_free (cf->name);
    g_free (cf);
}

static guint32
compute_checksum (guint8 *in, guint16 ncbytes, guint32 seed)
{
    int no_ulongs;
    guint32 csum=0;
    guint32 temp;

    no_ulongs = ncbytes / 4;
    csum = seed;

    while (no_ulongs-- > 0) {
        temp = ((guint32) (*in++));
        temp |= (((guint32) (*in++)) << 8);
        temp |= (((guint32) (*in++)) << 16);
        temp |= (((guint32) (*in++)) << 24);

        csum ^= temp;
    }

    temp = 0;
    switch (ncbytes % 4) {
    case 3: temp |= (((guint32) (*in++)) << 16);
    /* fall-thru */
    case 2: temp |= (((guint32) (*in++)) << 8);
    /* fall-thru */
    case 1: temp |= ((guint32) (*in++));
    /* fall-thru */
    default: break;
    }

    csum ^= temp;

    return csum;
}

static gboolean
cdata_deflate_block (cdata_t *cd, guint8 *data, size_t size, int level,
                     GError **error)
{
    z_stream deflate_stream = { 0, };
    int zret;

    deflate_stream.zalloc = zalloc;
    deflate_stream.zfree = zfree;
    zret = deflateInit2 (&deflate_stream, level, Z_DEFLATED, -15, 8,
                         Z_DEFAULT_STRATEGY);
    if (zret != Z_OK) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "zlib deflateInit2 failed: %s", zError (zret));
        return FALSE;
    }
    deflate_stream.next_in = data;
    deflate_stream.avail_in = size;
    deflate_stream.next_out = cd->in + 2;
    deflate_stream.avail_out = sizeof (cd->in) - 2;
    /* insert the signature */
    cd->in[0] = 'C';
    cd->in[1] = 'K';
    zret = deflate (&deflate_stream, Z_FINISH);
    if (zret != Z_OK && zret != Z_STREAM_END) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "zlib deflate failed: %s", zError (zret));
        return FALSE;
    }
    deflateEnd (&deflate_stream);

    cd->ncbytes = deflate_stream.total_out + 2;
    cd->nubytes = size;

    return TRUE;
}

G_GNUC_INTERNAL gboolean
cdata_write (cdata_t *cd, GDataOutputStream *out, int type,
             guint8 *data, size_t size, gsize *bytes_written,
             GCancellable *cancellable, GError **error)
{
    switch (type) {
    case GCAB_COMPRESSION_NONE:
        memcpy (cd->in, data, size);
        cd->ncbytes = size;
        cd->nubytes = size;
        break;

    case GCAB_COMPRESSION_MSZIP:
        if (!cdata_deflate_block (cd, data, size, Z_DEFAULT_COMPRESSION, error))
            return FALSE;

        if (cd->ncbytes >= CAB_MAX_MSZIP_BLOCK_SIZE) {
            /* if the compressor inflated the data, store it uncompressed */
            if (!cdata_deflate_block (cd, data, size, Z_NO_COMPRESSION, error))
                return FALSE;
        }

        break;

    default:
        g_critical ("unsupported compression method %d", type);
        return FALSE;
    }

    guint32 datacsum = compute_checksum(cd->in, cd->ncbytes, 0);
    guint8 sizecsum[4];
    guint16 nbytes_le;

    nbytes_le = GUINT16_TO_LE (cd->ncbytes);
    memcpy (&sizecsum[0], &nbytes_le, 2);
    nbytes_le = GUINT16_TO_LE (cd->nubytes);
    memcpy (&sizecsum[2], &nbytes_le, 2);
    cd->checksum = compute_checksum (sizecsum, sizeof(sizecsum), datacsum);
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
cdata_free (cdata_t *cd)
{
    z_stream *z = &cd->z;

    if (cd->decomp.comptype == GCAB_COMPRESSION_LZX) {
        LZXfdi_clear (&cd->decomp);
    }

    if (cd->decomp.comptype == GCAB_COMPRESSION_MSZIP) {
        if (z->opaque) {
            inflateEnd (z);
            z->opaque = NULL;
        }
    }
    g_free (cd->reserved);
    g_free (cd);
}

static gint
_enforce_checksum (void)
{
    static gint enforce = -1;
    if (enforce == -1)
        enforce = g_getenv ("GCAB_SKIP_CHECKSUM") == NULL ? 1 : 0;
    return enforce;
}

G_GNUC_INTERNAL gboolean
cdata_read (cdata_t *cd, guint8 res_data, gint comptype,
            GDataInputStream *in, GCancellable *cancellable, GError **error)

{
    gboolean success = FALSE;
    int ret, zret = Z_OK;
    gint compression = comptype & GCAB_COMPRESSION_MASK;
    gsize buf_sz;
    guint8 *buf = NULL;
    guint32 datacsum;
    guint32 checksum_tmp;
    guint8 sizecsum[4];
    guint16 nbytes_le;

    /* decompress directly into ->out for no decompression */
    switch (compression) {
    case GCAB_COMPRESSION_NONE:
        buf = cd->out;
        buf_sz = sizeof(cd->out);
        break;
    case GCAB_COMPRESSION_MSZIP:
    case GCAB_COMPRESSION_LZX:
        buf = cd->in;
        buf_sz = sizeof(cd->in);
        break;
    default:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED,
                     "unsupported compression method %d", compression);
        break;
    }
    if (buf == NULL)
        return FALSE;

    R4 (cd->checksum);
    R2 (cd->ncbytes);
    if (cd->ncbytes > buf_sz) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "tried to decompress %" G_GUINT16_FORMAT " bytes "
                     "into buffer of size %" G_GSIZE_FORMAT,
                     cd->ncbytes, buf_sz);
        return FALSE;
    }
    R2 (cd->nubytes);
    if (cd->nubytes > CAB_MAX_BLOCK_SIZE) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "CDATA block of %" G_GUINT16_FORMAT " bytes "
                     "was bigger than maximum size %i",
                     cd->nubytes, CAB_MAX_BLOCK_SIZE);
        return FALSE;
    }
    RN (cd->reserved, res_data);
    RN (buf, cd->ncbytes);

    datacsum = compute_checksum(buf, cd->ncbytes, 0);
    nbytes_le = GUINT16_TO_LE (cd->ncbytes);
    memcpy (&sizecsum[0], &nbytes_le, 2);
    nbytes_le = GUINT16_TO_LE (cd->nubytes);
    memcpy (&sizecsum[2], &nbytes_le, 2);
    checksum_tmp = compute_checksum (sizecsum, sizeof(sizecsum), datacsum);
    if (cd->checksum != checksum_tmp) {
        if (_enforce_checksum ()) {
            g_set_error_literal (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                                 "incorrect checksum detected");
            return FALSE;
        }
        if (g_getenv ("GCAB_DEBUG"))
            g_debug ("CDATA checksum 0x%08x", (guint) checksum_tmp);
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
            cd->decomp.comptype = compression;

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

        cd->decomp.comptype = compression;
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

    if (error != NULL && *error == NULL && !success)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Invalid cabinet chunk");

    return success;
}
