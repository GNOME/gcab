/*
 * LibGCab
 * Copyright (c) 2026, Marc-André Lureau <marcandre.lureau@gmail.com>
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

/*
 * Unit tests for the LZX decoder.
 *
 * The decoder has no corresponding encoder, so this file contains a small
 * test-only LZX bitstream writer.  It writes 16-bit little-endian words whose
 * bits are consumed MSB-first, matching the READ_BITS() macros in decomp.c.
 */

#include "config.h"

#include <string.h>

#include <glib.h>

#include "decomp.h"

#define TEST_WINDOW_BITS 15
#define TEST_WINDOW_SIZE (1u << TEST_WINDOW_BITS)
#define TEST_MAIN_NSYMS (LZX_NUM_CHARS + ((TEST_WINDOW_BITS << 1) << 3))

/* Main-tree symbols used by the tests. */
enum {
    SYM_A = 'A',
    SYM_B = 'B',
    SYM_C = 'C',
    SYM_D = 'D',
    SYM_E = 'E',
    SYM_S0 = LZX_NUM_CHARS + 0 * 8,
    SYM_S1 = LZX_NUM_CHARS + 1 * 8,
    SYM_S2 = LZX_NUM_CHARS + 2 * 8,
    SYM_S3 = LZX_NUM_CHARS + 3 * 8,
    SYM_S4 = LZX_NUM_CHARS + 4 * 8,
    SYM_S6 = LZX_NUM_CHARS + 6 * 8,
    SYM_S8 = LZX_NUM_CHARS + 8 * 8,
    SYM_S9 = LZX_NUM_CHARS + 9 * 8,
    SYM_S10 = LZX_NUM_CHARS + 10 * 8,
    SYM_S0_LH7 = LZX_NUM_CHARS + 0 * 8 + 7,
    SYM_S4_LH7 = LZX_NUM_CHARS + 4 * 8 + 7,
};

typedef struct {
    GByteArray *data;
    guint16 word;
    guint bits;
} BitWriter;

typedef struct {
    guint16 code[656];
    guint8 len[656];
} Huffman;

typedef struct {
    Huffman pre;
    Huffman main;
    Huffman length;
    Huffman aligned;
    guint8 pre_lens[20];
    guint8 main_lens[656];
    guint8 length_lens[LZX_NUM_SECONDARY_LENGTHS];
} TestTrees;

static gpointer
test_alloc (gsize size)
{
    return g_malloc0 (size);
}

static gpointer
test_alloc_fail (gsize size)
{
    (void) size;
    return NULL;
}

static void
test_free (gpointer data)
{
    g_free (data);
}

static BitWriter *
bw_new (void)
{
    BitWriter *bw = g_new0 (BitWriter, 1);
    bw->data = g_byte_array_new ();
    return bw;
}

static void
bw_put_bits (BitWriter *bw, guint32 value, guint nbits)
{
    for (guint i = nbits; i > 0; i--) {
        guint bit = (value >> (i - 1)) & 1;

        bw->word |= (guint16) (bit << (15 - bw->bits));
        bw->bits++;
        if (bw->bits == 16) {
            guint8 bytes[2] = { bw->word & 0xff, bw->word >> 8 };
            g_byte_array_append (bw->data, bytes, 2);
            bw->word = 0;
            bw->bits = 0;
        }
    }
}

static void
bw_align_word (BitWriter *bw)
{
    if (bw->bits == 0)
        return;

    guint8 bytes[2] = { bw->word & 0xff, bw->word >> 8 };
    g_byte_array_append (bw->data, bytes, 2);
    bw->word = 0;
    bw->bits = 0;
}

static void
bw_put_raw (BitWriter *bw, const guint8 *data, gsize size)
{
    g_assert_cmpuint (bw->bits, ==, 0);
    g_byte_array_append (bw->data, data, size);
}

static GBytes *
bw_finish (BitWriter *bw, gboolean lookahead_word)
{
    bw_align_word (bw);

    /* The Huffman decoder may peek a full word past the last symbol. */
    if (lookahead_word) {
        const guint8 zeros[2] = { 0, 0 };
        g_byte_array_append (bw->data, zeros, 2);
    }

    GBytes *bytes = g_byte_array_free_to_bytes (bw->data);

    g_free (bw);
    return bytes;
}

static void
huffman_build (Huffman *h, const guint8 *lens, guint nsyms)
{
    guint count[17] = { 0 };
    guint next_code[17] = { 0 };
    guint code = 0;

    for (guint i = 0; i < nsyms; i++)
        count[lens[i]]++;
    count[0] = 0;

    for (guint bits = 1; bits <= 16; bits++) {
        code = (code + count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    for (guint i = 0; i < nsyms; i++) {
        guint len = lens[i];

        h->len[i] = len;
        h->code[i] = 0;
        if (len != 0)
            h->code[i] = next_code[len]++;
    }
}

static void
huffman_put (BitWriter *bw, const Huffman *h, guint sym)
{
    g_assert_cmpuint (h->len[sym], >, 0);
    bw_put_bits (bw, h->code[sym], h->len[sym]);
}

/* A complete 20-symbol pretree with codes longer than the 6-bit direct table,
 * so the test also exercises the long-code path in make_decode_table(). */
static void
make_pre_lens (guint8 lens[20])
{
    static const guint order[] = {
        0, 17, 18, 19, 1, 2, 3, 4, 5, 6,
        7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    };

    memset (lens, 0, 20);
    for (guint i = 0; i < 7; i++)
        lens[order[i]] = 3;
    for (guint i = 7; i < 10; i++)
        lens[order[i]] = 6;
    for (guint i = 10; i < G_N_ELEMENTS (order); i++)
        lens[order[i]] = 7;
}

static void
make_main_lens (guint8 lens[656])
{
    static const guint used[] = {
        SYM_A, SYM_B, SYM_C, SYM_D, SYM_E,
        SYM_S0, SYM_S1, SYM_S2, SYM_S3, SYM_S4,
        SYM_S6, SYM_S8, SYM_S9, SYM_S10,
        SYM_S0_LH7, SYM_S4_LH7,
    };

    memset (lens, 0, 656);
    for (guint i = 0; i < G_N_ELEMENTS (used); i++)
        lens[used[i]] = 4;
}

static void
make_length_lens (guint8 lens[LZX_NUM_SECONDARY_LENGTHS])
{
    /* Complete 249-symbol tree: 7 codes of length 7 and 242 of length 8. */
    for (guint i = 0; i < LZX_NUM_SECONDARY_LENGTHS; i++)
        lens[i] = i < 7 ? 7 : 8;
}

static void
trees_init (TestTrees *trees)
{
    make_pre_lens (trees->pre_lens);
    huffman_build (&trees->pre, trees->pre_lens, G_N_ELEMENTS (trees->pre_lens));

    make_main_lens (trees->main_lens);
    huffman_build (&trees->main, trees->main_lens, TEST_MAIN_NSYMS);

    make_length_lens (trees->length_lens);
    huffman_build (&trees->length, trees->length_lens,
                   LZX_NUM_SECONDARY_LENGTHS);

    guint8 aligned_lens[8] = { 3, 3, 3, 3, 3, 3, 3, 3 };
    huffman_build (&trees->aligned, aligned_lens, G_N_ELEMENTS (aligned_lens));
}

/* Emit one range of code lengths using the LZX pretree run codes. */
static void
write_length_range (BitWriter *bw,
                    const TestTrees *trees,
                    const guint8 *lens,
                    guint first,
                    guint last)
{
    guint x = first;

    /* Every READ_LENGTHS() call carries a fresh pretree. */
    for (guint i = 0; i < 20; i++)
        bw_put_bits (bw, trees->pre_lens[i], 4);

    while (x < last) {
        if (lens[x] == 0) {
            guint run = 0;

            while (x + run < last && lens[x + run] == 0)
                run++;

            while (run >= 20) {
                guint n = MIN (run, 51);
                huffman_put (bw, &trees->pre, 18);
                bw_put_bits (bw, n - 20, 5);
                x += n;
                run -= n;
            }
            if (run >= 4) {
                guint n = MIN (run, 19);
                huffman_put (bw, &trees->pre, 17);
                bw_put_bits (bw, n - 4, 4);
                x += n;
                run -= n;
            }
            while (run-- > 0) {
                huffman_put (bw, &trees->pre, 0);
                x++;
            }
            continue;
        }

        guint len = lens[x];
        guint run = 1;

        while (x + run < last && lens[x + run] == len)
            run++;

        if (run >= 4) {
            guint n = run >= 5 ? 5 : 4;
            huffman_put (bw, &trees->pre, 19);
            bw_put_bits (bw, n - 4, 1);
            huffman_put (bw, &trees->pre, 17 - len);
            x += n;
        } else {
            huffman_put (bw, &trees->pre, 17 - len);
            x++;
        }
    }
}

static void
write_intel_header (BitWriter *bw, gboolean intel, guint32 filesize)
{
    bw_put_bits (bw, intel ? 1 : 0, 1);
    if (intel) {
        bw_put_bits (bw, filesize >> 16, 16);
        bw_put_bits (bw, filesize & 0xffff, 16);
    }
}

static void
write_block_header (BitWriter *bw, guint type, guint block_len)
{
    bw_put_bits (bw, type, 3);
    bw_put_bits (bw, block_len >> 8, 16);
    bw_put_bits (bw, block_len & 0xff, 8);
}

static void
write_compressed_trees (BitWriter *bw,
                        const TestTrees *trees,
                        gboolean nonzero_length_tree)
{
    static const guint8 zero_lengths[LZX_NUM_SECONDARY_LENGTHS] = { 0 };

    write_length_range (bw, trees, trees->main_lens, 0, 256);
    write_length_range (bw, trees, trees->main_lens, 256, TEST_MAIN_NSYMS);
    write_length_range (bw, trees,
                        nonzero_length_tree ? trees->length_lens : zero_lengths,
                        0, LZX_NUM_SECONDARY_LENGTHS);
}

static void
write_match (BitWriter *bw,
             const TestTrees *trees,
             guint slot,
             guint length_header,
             guint footer,
             guint verbatim_bits,
             guint aligned_bits,
             gboolean aligned)
{
    guint sym = LZX_NUM_CHARS + slot * 8 + length_header;

    huffman_put (bw, &trees->main, sym);
    if (length_header == LZX_NUM_PRIMARY_LENGTHS)
        huffman_put (bw, &trees->length, footer);

    if (slot <= 2)
        return;

    if (aligned) {
        guint extra = (slot == 3) ? 0 :
                      (slot <= 5) ? 1 :
                      (slot <= 7) ? 2 :
                      (slot <= 9) ? 3 : 4;

        if (extra > 3) {
            bw_put_bits (bw, verbatim_bits, extra - 3);
            huffman_put (bw, &trees->aligned, aligned_bits);
        } else if (extra == 3) {
            huffman_put (bw, &trees->aligned, aligned_bits);
        } else if (extra > 0) {
            bw_put_bits (bw, verbatim_bits, extra);
        }
    } else if (slot != 3) {
        guint extra = (slot <= 5) ? 1 :
                      (slot <= 7) ? 2 :
                      (slot <= 9) ? 3 : 4;
        bw_put_bits (bw, verbatim_bits, extra);
    }
}

static void
decomp_state_init (fdi_decomp_state *state, FDI_Int *fdi, guint8 *in, guint8 *out)
{
    memset (state, 0, sizeof (*state));
    fdi->alloc = test_alloc;
    fdi->free = test_free;
    state->fdi = fdi;
    state->inbuf = in;
    state->outbuf = out;
    g_assert_cmpint (LZXfdi_init (TEST_WINDOW_BITS, state), ==, DECR_OK);
}

static void
decomp_test_init (void)
{
    fdi_decomp_state state;
    FDI_Int fdi;
    guint8 dummy;

    memset (&state, 0, sizeof (state));
    fdi.alloc = test_alloc;
    fdi.free = test_free;
    state.fdi = &fdi;
    state.inbuf = &dummy;
    state.outbuf = &dummy;

    /* invalid window sizes */
    g_assert_cmpint (LZXfdi_init (14, &state), ==, DECR_DATAFORMAT);
    g_assert_cmpint (LZXfdi_init (22, &state), ==, DECR_DATAFORMAT);

    /* allocate, then reuse the existing window */
    g_assert_cmpint (LZXfdi_init (15, &state), ==, DECR_OK);
    g_assert_cmpint (LZXfdi_init (15, &state), ==, DECR_OK);

    /* grow the window and exercise the reallocation path */
    g_assert_cmpint (LZXfdi_init (16, &state), ==, DECR_OK);
    g_assert_cmpint (LZXfdi_init (15, &state), ==, DECR_OK);

    /* the special position-slot counts for 1 MiB and 2 MiB windows */
    g_assert_cmpint (LZXfdi_init (20, &state), ==, DECR_OK);
    g_assert_cmpint (LZXfdi_init (21, &state), ==, DECR_OK);

    LZXfdi_clear (&state);

    /* allocation failure */
    memset (&state, 0, sizeof (state));
    state.fdi = &fdi;
    state.inbuf = &dummy;
    state.outbuf = &dummy;
    fdi.alloc = test_alloc_fail;
    g_assert_cmpint (LZXfdi_init (15, &state), ==, DECR_NOMEMORY);
    fdi.alloc = test_alloc;
}

static void
decomp_test_uncompressed (void)
{
    const guint8 payload[] = { 'h', 'e', 'l', 'l', 'o' };
    const guint8 truncated[] = { 1, 2, 3, 4 };
    guint8 out[16];
    fdi_decomp_state state;
    FDI_Int fdi;

    /* complete block */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, sizeof (payload));
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, sizeof (payload));
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (payload),
                                        &state), ==, DECR_OK);
        g_assert_cmpmem (out, sizeof (payload), payload, sizeof (payload));
        LZXfdi_clear (&state);
    }

    /* an odd-length block that ends before the next block header */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, 1);
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, 1);
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), 2, &state),
                         ==, DECR_ILLEGALDATA);
        LZXfdi_clear (&state);
    }

    /* truncated R0/R1/R2 header */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, 1);
        bw_align_word (bw);
        bw_put_raw (bw, truncated, sizeof (truncated));
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), 1, &state),
                         ==, DECR_ILLEGALDATA);
        LZXfdi_clear (&state);
    }

    /* declared block length exceeds the available data */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, 5);
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, 2);
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), 5, &state),
                         ==, DECR_ILLEGALDATA);
        LZXfdi_clear (&state);
    }

    /* invalid block type */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, 0, 1);
        g_autoptr(GBytes) bytes = bw_finish (bw, TRUE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), 1, &state),
                         ==, DECR_ILLEGALDATA);
        LZXfdi_clear (&state);
    }

    /* window_filled is clamped when an uncompressed block crosses the end */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, FALSE, 0);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, sizeof (payload));
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, sizeof (payload));
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        state.lzx.window_filled = TEST_WINDOW_SIZE - 1;
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (payload),
                                        &state), ==, DECR_OK);
        g_assert_cmpuint (state.lzx.window_filled, ==, TEST_WINDOW_SIZE);
        LZXfdi_clear (&state);
    }

    /* an odd-sized uncompressed block is padded before the next block */
    {
        BitWriter *first = bw_new ();
        write_intel_header (first, FALSE, 0);
        write_block_header (first, LZX_BLOCKTYPE_UNCOMPRESSED, 1);
        bw_align_word (first);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (first, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (first, payload, 1);
        const guint8 padding = 0;
        bw_put_raw (first, &padding, 1);
        g_autoptr(GBytes) first_bytes = bw_finish (first, FALSE);

        BitWriter *second = bw_new ();
        /* header_read is already set, so no Intel/header bit here */
        write_block_header (second, LZX_BLOCKTYPE_UNCOMPRESSED, 2);
        bw_align_word (second);
        bw_put_raw (second, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (second, payload, 2);
        g_autoptr(GBytes) second_bytes = bw_finish (second, FALSE);

        g_autoptr(GByteArray) stream = g_byte_array_new ();
        g_byte_array_append (stream, g_bytes_get_data (first_bytes, NULL),
                             g_bytes_get_size (first_bytes));
        g_byte_array_append (stream, g_bytes_get_data (second_bytes, NULL),
                             g_bytes_get_size (second_bytes));

        decomp_state_init (&state, &fdi, stream->data, out);
        g_assert_cmpint (LZXfdi_decomp (stream->len, 3, &state), ==, DECR_OK);
        g_assert_cmphex (out[0], ==, 'h');
        g_assert_cmphex (out[1], ==, 'h');
        g_assert_cmphex (out[2], ==, 'e');
        LZXfdi_clear (&state);
    }
}

static void
decomp_test_frame_bounds (void)
{
    guint8 out[16];
    guint8 in[2] = { 0, 0 };
    fdi_decomp_state state;
    FDI_Int fdi;

    decomp_state_init (&state, &fdi, in, out);
    state.lzx.window_posn = TEST_WINDOW_SIZE - 2;
    g_assert_cmpint (LZXfdi_decomp (0, 3, &state), ==, DECR_DATAFORMAT);
    LZXfdi_clear (&state);
}

static void
decomp_test_intel_e8 (void)
{
    const guint32 filesize = 0x100000;
    guint8 payload[32] = { 0 };
    guint8 out[32];
    fdi_decomp_state state;
    FDI_Int fdi;

    /* positive offset, negative offset, out-of-range offset and a non-E8 byte */
    payload[0] = 0xE8;
    payload[1] = 0x00;
    payload[2] = 0x03;
    payload[3] = 0x00;
    payload[4] = 0x00;
    payload[5] = 0xE8;
    payload[6] = 0xFF;
    payload[7] = 0xFF;
    payload[8] = 0xFF;
    payload[9] = 0xFF;
    payload[10] = 0xE8;
    payload[11] = 0x00;
    payload[12] = 0x00;
    payload[13] = 0x20;
    payload[14] = 0x00;
    payload[15] = 'x';

    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, TRUE, filesize);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, sizeof (payload));
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, sizeof (payload));
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                        &state), ==, DECR_OK);
        /* the negative absolute offset is converted relative to the file size */
        g_assert_cmphex (out[6], ==, (filesize - 1) & 0xff);
        g_assert_cmphex (out[7], ==, ((filesize - 1) >> 8) & 0xff);
        LZXfdi_clear (&state);
    }

    /* outlen <= 6 only advances the Intel cursor */
    {
        BitWriter *bw = bw_new ();
        write_intel_header (bw, TRUE, filesize);
        write_block_header (bw, LZX_BLOCKTYPE_UNCOMPRESSED, 4);
        bw_align_word (bw);
        guint8 r0r1r2[12] = { 0 };
        bw_put_raw (bw, r0r1r2, sizeof (r0r1r2));
        bw_put_raw (bw, payload, 4);
        g_autoptr(GBytes) bytes = bw_finish (bw, FALSE);

        decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), 4, &state),
                         ==, DECR_OK);
        g_assert_cmpint (state.lzx.intel_curpos, ==, 4);
        LZXfdi_clear (&state);
    }
}

static GBytes *
build_verbatim_main_stream (const TestTrees *trees, gboolean intel, guint32 filesize)
{
    BitWriter *bw = bw_new ();

    write_intel_header (bw, intel, filesize);
    write_block_header (bw, LZX_BLOCKTYPE_VERBATIM, 23);
    write_compressed_trees (bw, trees, TRUE);

    /* 4 literals, then matches covering the repeated-offset LRU and the
     * verbatim extra-bit and length-footer paths. */
    huffman_put (bw, &trees->main, SYM_A);
    huffman_put (bw, &trees->main, SYM_B);
    huffman_put (bw, &trees->main, SYM_C);
    huffman_put (bw, &trees->main, SYM_D);
    write_match (bw, trees, 0, 0, 0, 0, 0, FALSE); /* R0 */
    write_match (bw, trees, 1, 0, 0, 0, 0, FALSE); /* R1 */
    write_match (bw, trees, 2, 0, 0, 0, 0, FALSE); /* R2 */
    write_match (bw, trees, 3, 0, 0, 0, 0, FALSE); /* fixed offset 1 */
    write_match (bw, trees, 4, 0, 0, 0, 0, FALSE); /* offset 2 */
    write_match (bw, trees, 0, 7, 0, 0, 0, FALSE); /* length 9 via footer */

    return bw_finish (bw, TRUE);
}

static GBytes *
build_aligned_main_stream (const TestTrees *trees)
{
    BitWriter *bw = bw_new ();

    write_intel_header (bw, FALSE, 0);
    write_block_header (bw, LZX_BLOCKTYPE_ALIGNED, 50);

    /* all aligned symbols have a 3-bit code */
    for (guint i = 0; i < 8; i++)
        bw_put_bits (bw, 3, 3);

    write_compressed_trees (bw, trees, FALSE);

    /* enough history for the larger aligned offsets */
    for (guint i = 0; i < 32; i++)
        huffman_put (bw, &trees->main, SYM_A);

    write_match (bw, trees, 3, 0, 0, 0, 0, TRUE);  /* extra == 0 */
    write_match (bw, trees, 0, 0, 0, 0, 0, TRUE);  /* R0 */
    write_match (bw, trees, 1, 0, 0, 0, 0, TRUE);  /* R1 */
    write_match (bw, trees, 2, 0, 0, 0, 0, TRUE);  /* R2 */
    write_match (bw, trees, 4, 0, 0, 0, 0, TRUE);  /* extra == 1 */
    write_match (bw, trees, 6, 0, 0, 0, 0, TRUE);  /* extra == 2 */
    write_match (bw, trees, 8, 0, 0, 0, 0, TRUE);  /* extra == 3, aligned only */
    write_match (bw, trees, 9, 0, 0, 0, 0, TRUE);  /* extra == 3, aligned only */
    write_match (bw, trees, 10, 0, 0, 0, 0, TRUE); /* extra > 3, verbatim + aligned */

    return bw_finish (bw, TRUE);
}

static void
decomp_test_verbatim (void)
{
    TestTrees trees;
    guint8 out[23];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);
    g_autoptr(GBytes) bytes = build_verbatim_main_stream (&trees, FALSE, 0);

    decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
    g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                    &state), ==, DECR_OK);
    g_assert_cmpmem (out, 4, "ABCD", 4);
    for (guint i = 4; i < sizeof (out); i++)
        g_assert_cmphex (out[i], ==, 'D');
    LZXfdi_clear (&state);

    /* An Intel header without any 0xE8 code in the main tree must not
     * attempt the E8 transform. */
    g_clear_pointer (&bytes, g_bytes_unref);
    bytes = build_verbatim_main_stream (&trees, TRUE, 0x100000);
    decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
    g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                    &state), ==, DECR_OK);
    g_assert_cmpmem (out, 4, "ABCD", 4);
    g_assert_cmpint (state.lzx.intel_curpos, ==, sizeof (out));
    LZXfdi_clear (&state);
}

static void
decomp_test_aligned (void)
{
    TestTrees trees;
    guint8 out[50];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);
    g_autoptr(GBytes) bytes = build_aligned_main_stream (&trees);

    decomp_state_init (&state, &fdi, (guint8 *) g_bytes_get_data (bytes, NULL), out);
    g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                    &state), ==, DECR_OK);
    for (guint i = 0; i < sizeof (out); i++)
        g_assert_cmphex (out[i], ==, 'A');
    LZXfdi_clear (&state);
}

static GBytes *
build_one_match_stream (const TestTrees *trees, gboolean aligned, guint slot)
{
    BitWriter *bw = bw_new ();

    write_intel_header (bw, FALSE, 0);
    write_block_header (bw, aligned ? LZX_BLOCKTYPE_ALIGNED : LZX_BLOCKTYPE_VERBATIM, 9);
    if (aligned) {
        for (guint i = 0; i < 8; i++)
            bw_put_bits (bw, 3, 3);
    }
    write_compressed_trees (bw, trees, TRUE);
    write_match (bw, trees, slot, 7, 0, 0, 0, aligned);

    return bw_finish (bw, TRUE);
}

static void
decomp_test_match_window_wrap (void)
{
    TestTrees trees;
    guint8 out[9];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);

    for (guint aligned = 0; aligned < 2; aligned++) {
        g_autoptr(GBytes) bytes = build_one_match_stream (&trees, aligned, 4);

        decomp_state_init (&state, &fdi,
                           (guint8 *) g_bytes_get_data (bytes, NULL), out);
        /* Pretend the window is full and the next frame starts at offset 0.
         * The 9-byte match at offset 2 wraps around the window end and also
         * clamps window_filled. */
        state.lzx.window_posn = 0;
        state.lzx.window_filled = TEST_WINDOW_SIZE - 1;
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                        &state), ==, DECR_OK);
        g_assert_cmpuint (state.lzx.window_filled, ==, TEST_WINDOW_SIZE);
        LZXfdi_clear (&state);
    }
}

static void
decomp_test_match_window_overflow (void)
{
    TestTrees trees;
    guint8 out[2];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);

    for (guint aligned = 0; aligned < 2; aligned++) {
        g_autoptr(GBytes) bytes = build_one_match_stream (&trees, aligned, 4);

        decomp_state_init (&state, &fdi,
                           (guint8 *) g_bytes_get_data (bytes, NULL), out);
        /* The frame itself fits, but the match writes past the window. */
        state.lzx.window_posn = TEST_WINDOW_SIZE - 2;
        state.lzx.window_filled = TEST_WINDOW_SIZE;
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                        &state), ==, DECR_DATAFORMAT);
        LZXfdi_clear (&state);
    }
}

static void
decomp_test_match_beyond_history (void)
{
    TestTrees trees;
    guint8 out[2];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);

    for (guint aligned = 0; aligned < 2; aligned++) {
        g_autoptr(GBytes) bytes = build_one_match_stream (&trees, aligned, 4);

        decomp_state_init (&state, &fdi,
                           (guint8 *) g_bytes_get_data (bytes, NULL), out);
        state.lzx.window_posn = 0;
        state.lzx.window_filled = 0;
        g_assert_cmpint (LZXfdi_decomp (g_bytes_get_size (bytes), sizeof (out),
                                        &state), ==, DECR_DATAFORMAT);
        LZXfdi_clear (&state);
    }
}

static void
decomp_test_truncated (void)
{
    TestTrees trees;
    guint8 out[64];
    fdi_decomp_state state;
    FDI_Int fdi;

    trees_init (&trees);

    struct {
        GBytes *bytes;
        guint outlen;
    } tests[] = {
        { build_verbatim_main_stream (&trees, FALSE, 0), 23 },
        { build_aligned_main_stream (&trees), 50 },
    };

    for (guint i = 0; i < G_N_ELEMENTS (tests); i++) {
        const guint8 *data = g_bytes_get_data (tests[i].bytes, NULL);
        gsize size = g_bytes_get_size (tests[i].bytes);

        for (gsize len = 0; len <= size; len++) {
            /* Every prefix must be rejected or decoded without crashing. */
            decomp_state_init (&state, &fdi, (guint8 *) data, out);
            (void) LZXfdi_decomp (len, tests[i].outlen, &state);
            LZXfdi_clear (&state);
        }
        g_bytes_unref (tests[i].bytes);
    }
}

gint
main (gint argc, gchar **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/LZX/init", decomp_test_init);
    g_test_add_func ("/LZX/uncompressed", decomp_test_uncompressed);
    g_test_add_func ("/LZX/frame-bounds", decomp_test_frame_bounds);
    g_test_add_func ("/LZX/intel-e8", decomp_test_intel_e8);
    g_test_add_func ("/LZX/verbatim", decomp_test_verbatim);
    g_test_add_func ("/LZX/aligned", decomp_test_aligned);
    g_test_add_func ("/LZX/match-window-wrap", decomp_test_match_window_wrap);
    g_test_add_func ("/LZX/match-window-overflow", decomp_test_match_window_overflow);
    g_test_add_func ("/LZX/match-beyond-history", decomp_test_match_beyond_history);
    g_test_add_func ("/LZX/truncated", decomp_test_truncated);

    return g_test_run ();
}
