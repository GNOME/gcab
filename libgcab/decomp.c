/*
 * Adapted from Wine fdi.c: File Decompression Interface
 *
 * Copyright 2000-2002 Stuart Caie
 * Copyright 2002 Patrik Stridvall
 * Copyright 2003 Greg Turner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include "decomp.h"

#include <string.h>

#define CAB(x) (decomp_state->x)
#define LZX(x) (decomp_state->lzx.x)

/*************************************************************************
 * make_decode_table (internal)
 *
 * This function was coded by David Tritscher. It builds a fast huffman
 * decoding table out of just a canonical huffman code lengths table.
 *
 * PARAMS
 *   nsyms:  total number of symbols in this huffman tree.
 *   nbits:  any symbols with a code length of nbits or less can be decoded
 *           in one lookup of the table.
 *   length: A table to get code lengths from [0 to syms-1]
 *   table:  The table to fill up with decoded symbols and pointers.
 *
 * RETURNS
 *   OK:    0
 *   error: 1
 */
static gint make_decode_table(cab_ULONG nsyms, cab_ULONG nbits,
                             const cab_UBYTE *length, cab_UWORD *table) {
  register cab_UWORD sym;
  register cab_ULONG leaf;
  register cab_UBYTE bit_num = 1;
  cab_ULONG fill;
  cab_ULONG pos         = 0; /* the current position in the decode table */
  cab_ULONG table_mask  = 1 << nbits;
  cab_ULONG bit_mask    = table_mask >> 1; /* don't do 0 length codes */
  cab_ULONG next_symbol = bit_mask; /* base of allocation for long codes */

  /* fill entries for codes short enough for a direct mapping */
  while (bit_num <= nbits) {
    for (sym = 0; sym < nsyms; sym++) {
      if (length[sym] == bit_num) {
        leaf = pos;

        if((pos += bit_mask) > table_mask) return 1; /* table overrun */

        /* fill all possible lookups of this symbol with the symbol itself */
        fill = bit_mask;
        while (fill-- > 0) table[leaf++] = sym;
      }
    }
    bit_mask >>= 1;
    bit_num++;
  }

  /* if there are any codes longer than nbits */
  if (pos != table_mask) {
    /* clear the remainder of the table */
    for (sym = pos; sym < table_mask; sym++) table[sym] = 0;

    /* give ourselves room for codes to grow by up to 16 more bits */
    pos <<= 16;
    table_mask <<= 16;
    bit_mask = 1 << 15;

    while (bit_num <= 16) {
      for (sym = 0; sym < nsyms; sym++) {
        if (length[sym] == bit_num) {
          leaf = pos >> 16;
          for (fill = 0; fill < bit_num - nbits; fill++) {
            /* if this path hasn't been taken yet, 'allocate' two entries */
            if (table[leaf] == 0) {
              table[(next_symbol << 1)] = 0;
              table[(next_symbol << 1) + 1] = 0;
              table[leaf] = next_symbol++;
            }
            /* follow the path and select either left or right for next bit */
            leaf = table[leaf] << 1;
            if ((pos >> (15-fill)) & 1) leaf++;
          }
          table[leaf] = sym;

          if ((pos += bit_mask) > table_mask) return 1; /* table overflow */
        }
      }
      bit_mask >>= 1;
      bit_num++;
    }
  }

  /* full table? */
  if (pos == table_mask) return 0;

  /* either erroneous table, or all elements are 0 - let's find out. */
  for (sym = 0; sym < nsyms; sym++) if (length[sym]) return 1;
  return 0;
}

/************************************************************
 * fdi_lzx_read_lens (internal)
 */
static gint fdi_lzx_read_lens(cab_UBYTE *lens, cab_ULONG first, cab_ULONG last, struct lzx_bits *lb,
                  fdi_decomp_state *decomp_state) {
  cab_ULONG i,j, x,y;
  gint z;

  register cab_ULONG bitbuf = lb->bb;
  register int bitsleft = lb->bl;
  cab_UBYTE *inpos = lb->ip;
  const cab_UBYTE *endinp = lb->end;
  gint pad_words = lb->pad_words;
  cab_UWORD *hufftbl;

  for (x = 0; x < 20; x++) {
    READ_BITS(y, 4);
    LENTABLE(PRETREE)[x] = y;
  }
  BUILD_TABLE(PRETREE);

  for (x = first; x < last; ) {
    READ_HUFFSYM(PRETREE, z);
    if (z == 17) {
      READ_BITS(y, 4); y += 4;
      while (y--) lens[x++] = 0;
    }
    else if (z == 18) {
      READ_BITS(y, 5); y += 20;
      while (y--) lens[x++] = 0;
    }
    else if (z == 19) {
      READ_BITS(y, 1); y += 4;
      READ_HUFFSYM(PRETREE, z);
      z = lens[x] - z; if (z < 0) z += 17;
      while (y--) lens[x++] = z;
    }
    else {
      z = lens[x] - z; if (z < 0) z += 17;
      lens[x++] = z;
    }
  }

  lb->bb = bitbuf;
  lb->bl = bitsleft;
  lb->ip = inpos;
  lb->pad_words = pad_words;
  return 0;
}

/************************************************************
 * LZXfdi_init (internal)
 */
gint LZXfdi_init(gint window, fdi_decomp_state *decomp_state) {
  static const cab_UBYTE bits[]  =
                        { 0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
                          7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
                         15, 15, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
                         17, 17, 17};
  static const cab_ULONG base[] =
                {      0,       1,       2,       3,       4,       6,       8,      12,
                      16,      24,      32,      48,      64,      96,     128,     192,
                     256,     384,     512,     768,    1024,    1536,    2048,    3072,
                    4096,    6144,    8192,   12288,   16384,   24576,   32768,   49152,
                   65536,   98304,  131072,  196608,  262144,  393216,  524288,  655360,
                  786432,  917504, 1048576, 1179648, 1310720, 1441792, 1572864, 1703936,
                 1835008, 1966080, 2097152};
  cab_ULONG wndsize;
  gint posn_slots;

  /* LZX supports window sizes of 2^15 (32Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it     */
  if (window < 15 || window > 21) return DECR_DATAFORMAT;
  wndsize = (cab_ULONG) 1 << window;
  if (LZX(actual_size) < wndsize) {
    if (LZX(window)) CAB(fdi)->free(LZX(window));
    LZX(window) = NULL;
  }
  if (!LZX(window)) {
    if (!(LZX(window) = CAB(fdi)->alloc(wndsize))) return DECR_NOMEMORY;
    LZX(actual_size) = wndsize;
  }
  LZX(window_size) = wndsize;

  /* initialize static tables */
  memcpy(CAB(extra_bits), bits, sizeof(bits));
  memcpy(CAB(lzx_position_base), base, sizeof(base));

  /* calculate required position slots */
  if (window == 20) posn_slots = 42;
  else if (window == 21) posn_slots = 50;
  else posn_slots = window << 1;

  /*posn_slots=i=0; while (i < wndsize) i += 1 << CAB(extra_bits)[posn_slots++]; */

  LZX(R0)  =  LZX(R1)  = LZX(R2) = 1;
  LZX(main_elements)   = LZX_NUM_CHARS + (posn_slots << 3);
  LZX(header_read)     = 0;
  LZX(frames_read)     = 0;
  LZX(block_remaining) = 0;
  LZX(block_type)      = LZX_BLOCKTYPE_INVALID;
  LZX(intel_curpos)    = 0;
  LZX(intel_started)   = 0;
  LZX(window_posn)     = 0;
  LZX(window_filled)   = 0;

  /* initialize tables to 0 (because deltas will be applied to them) */
  memset(LZX(MAINTREE_len), 0, sizeof(LZX(MAINTREE_len)));
  memset(LZX(LENGTH_len), 0, sizeof(LZX(LENGTH_len)));

  return DECR_OK;
}

void LZXfdi_clear(fdi_decomp_state *decomp_state) {
  cab_UBYTE *window = LZX(window);
  CAB(fdi)->free(window);
}

/*******************************************************
 * LZXfdi_decomp(internal)
 */
gint LZXfdi_decomp(gint inlen, gint outlen, fdi_decomp_state *decomp_state) {
  cab_UBYTE *inpos  = CAB(inbuf);
  const cab_UBYTE *endinp = inpos + inlen;
  cab_UBYTE *window = LZX(window);
  cab_UBYTE *runsrc, *rundest;
  cab_UWORD *hufftbl; /* used in READ_HUFFSYM macro as chosen decoding table */

  cab_ULONG window_posn = LZX(window_posn);
  cab_ULONG window_size = LZX(window_size);
  cab_ULONG window_filled = LZX(window_filled);
  cab_ULONG frame_start = window_posn & (window_size - 1);
  cab_ULONG R0 = LZX(R0);
  cab_ULONG R1 = LZX(R1);
  cab_ULONG R2 = LZX(R2);

  register cab_ULONG bitbuf;
  register int bitsleft;
  gint pad_words = 0;
  cab_ULONG match_offset, i,j,k; /* ijk used in READ_HUFFSYM macro */
  struct lzx_bits lb; /* used in READ_LENGTHS macro */

  gint togo = outlen, this_run, main_element, aligned_bits;
  gint match_length, copy_length, length_footer, extra, verbatim_bits;

  /* a frame must fit in one pass through the window; individual runs are
   * already prevented from crossing the wrap */
  if ((cab_ULONG) outlen > window_size - frame_start)
    return DECR_DATAFORMAT;

  INIT_BITSTREAM;

  /* read header if necessary */
  if (!LZX(header_read)) {
    i = j = 0;
    READ_BITS(k, 1); if (k) { READ_BITS(i,16); READ_BITS(j,16); }
    LZX(intel_filesize) = (i << 16) | j; /* or 0 if not encoded */
    LZX(header_read) = 1;
  }

  /* main decoding loop */
  while (togo > 0) {
    /* last block finished, new block expected */
    if (LZX(block_remaining) == 0) {
      if (LZX(block_type) == LZX_BLOCKTYPE_UNCOMPRESSED) {
        if (LZX(block_length) & 1) { /* realign bitstream to word */
          if (inpos >= endinp) return DECR_ILLEGALDATA;
          inpos++;
        }
        INIT_BITSTREAM;
      }

      READ_BITS(LZX(block_type), 3);
      READ_BITS(i, 16);
      READ_BITS(j, 8);
      LZX(block_remaining) = LZX(block_length) = (i << 8) | j;

      switch (LZX(block_type)) {
      case LZX_BLOCKTYPE_ALIGNED:
        for (i = 0; i < 8; i++) { READ_BITS(j, 3); LENTABLE(ALIGNED)[i] = j; }
        BUILD_TABLE(ALIGNED);
        /* rest of aligned header is same as verbatim */
        /* fall-thru */

      case LZX_BLOCKTYPE_VERBATIM:
        READ_LENGTHS(MAINTREE, 0, 256, fdi_lzx_read_lens);
        READ_LENGTHS(MAINTREE, 256, LZX(main_elements), fdi_lzx_read_lens);
        BUILD_TABLE(MAINTREE);
        if (LENTABLE(MAINTREE)[0xE8] != 0) LZX(intel_started) = 1;

        READ_LENGTHS(LENGTH, 0, LZX_NUM_SECONDARY_LENGTHS, fdi_lzx_read_lens);
        BUILD_TABLE(LENGTH);
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        LZX(intel_started) = 1; /* because we can't assume otherwise */
        ENSURE_BITS(16); /* get up to 16 pad bits into the buffer */
        if (bitsleft > 16) inpos -= 2; /* and align the bitstream! */
        if (endinp - inpos < 12) return DECR_ILLEGALDATA;
        R0 = inpos[0]|((cab_ULONG)inpos[1]<<8)|((cab_ULONG)inpos[2]<<16)|((cab_ULONG)inpos[3]<<24);inpos+=4;
        R1 = inpos[0]|((cab_ULONG)inpos[1]<<8)|((cab_ULONG)inpos[2]<<16)|((cab_ULONG)inpos[3]<<24);inpos+=4;
        R2 = inpos[0]|((cab_ULONG)inpos[1]<<8)|((cab_ULONG)inpos[2]<<16)|((cab_ULONG)inpos[3]<<24);inpos+=4;
        break;

      default:
        return DECR_ILLEGALDATA;
      }
    }

    /* buffer exhaustion check: a synthesized padding word may be used for
     * Huffman look-ahead, but its bits must not have been consumed */
    if (pad_words > 0 && bitsleft < 16)
      return DECR_ILLEGALDATA;

    while ((this_run = LZX(block_remaining)) > 0 && togo > 0) {
      if (this_run > togo) this_run = togo;
      togo -= this_run;
      LZX(block_remaining) -= this_run;

      /* apply 2^x-1 mask */
      window_posn &= window_size - 1;
      /* runs can't straddle the window wraparound */
      if ((window_posn + this_run) > window_size)
        return DECR_DATAFORMAT;

      switch (LZX(block_type)) {

      case LZX_BLOCKTYPE_VERBATIM:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);

          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            if (window_filled < window_size)
              window_filled++;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;

            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;

            /* a match may extend beyond the current run, but it must not
             * write past the end of the window */
            if (window_posn + match_length > window_size)
              return DECR_DATAFORMAT;

            match_offset = main_element >> 3;

            if (match_offset > 2) {
              /* not repeated offset */
              if (match_offset != 3) {
                extra = CAB(extra_bits)[match_offset];
                READ_BITS(verbatim_bits, extra);
                match_offset = CAB(lzx_position_base)[match_offset]
                               - 2 + verbatim_bits;
              }
              else {
                match_offset = 1;
              }

              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            /* reject offsets outside the window or beyond decoded history */
            if (match_offset == 0 ||
                match_offset > window_size ||
                match_offset > window_filled)
              return DECR_DATAFORMAT;

            window_filled += match_length;
            if (window_filled > window_size)
              window_filled = window_size;

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
              /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
                while (copy_length-- > 0) *rundest++ = *runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_ALIGNED:
        while (this_run > 0) {
          READ_HUFFSYM(MAINTREE, main_element);

          if (main_element < LZX_NUM_CHARS) {
            /* literal: 0 to LZX_NUM_CHARS-1 */
            window[window_posn++] = main_element;
            if (window_filled < window_size)
              window_filled++;
            this_run--;
          }
          else {
            /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
            main_element -= LZX_NUM_CHARS;

            match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
            if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              READ_HUFFSYM(LENGTH, length_footer);
              match_length += length_footer;
            }
            match_length += LZX_MIN_MATCH;

            /* a match may extend beyond the current run, but it must not
             * write past the end of the window */
            if (window_posn + match_length > window_size)
              return DECR_DATAFORMAT;

            match_offset = main_element >> 3;

            if (match_offset > 2) {
              /* not repeated offset */
              extra = CAB(extra_bits)[match_offset];
              match_offset = CAB(lzx_position_base)[match_offset] - 2;
              if (extra > 3) {
                /* verbatim and aligned bits */
                extra -= 3;
                READ_BITS(verbatim_bits, extra);
                match_offset += (verbatim_bits << 3);
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra == 3) {
                /* aligned bits only */
                READ_HUFFSYM(ALIGNED, aligned_bits);
                match_offset += aligned_bits;
              }
              else if (extra > 0) { /* extra==1, extra==2 */
                /* verbatim bits only */
                READ_BITS(verbatim_bits, extra);
                match_offset += verbatim_bits;
              }
              else /* extra == 0 */ {
                /* ??? */
                match_offset = 1;
              }

              /* update repeated offset LRU queue */
              R2 = R1; R1 = R0; R0 = match_offset;
            }
            else if (match_offset == 0) {
              match_offset = R0;
            }
            else if (match_offset == 1) {
              match_offset = R1;
              R1 = R0; R0 = match_offset;
            }
            else /* match_offset == 2 */ {
              match_offset = R2;
              R2 = R0; R0 = match_offset;
            }

            /* reject offsets outside the window or beyond decoded history */
            if (match_offset == 0 ||
                match_offset > window_size ||
                match_offset > window_filled)
              return DECR_DATAFORMAT;

            window_filled += match_length;
            if (window_filled > window_size)
              window_filled = window_size;

            rundest = window + window_posn;
            this_run -= match_length;

            /* copy any wrapped around source data */
            if (window_posn >= match_offset) {
              /* no wrap */
              runsrc = rundest - match_offset;
            } else {
              runsrc = rundest + (window_size - match_offset);
              copy_length = match_offset - window_posn;
              if (copy_length < match_length) {
                match_length -= copy_length;
                window_posn += copy_length;
                while (copy_length-- > 0) *rundest++ = *runsrc++;
                runsrc = window;
              }
            }
            window_posn += match_length;

            /* copy match data - no worries about destination wraps */
            while (match_length-- > 0) *rundest++ = *runsrc++;
          }
        }
        break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
        if (this_run > endinp - inpos) return DECR_ILLEGALDATA;
        memcpy(window + window_posn, inpos, (size_t) this_run);
        inpos += this_run; window_posn += this_run;
        window_filled += this_run;
        if (window_filled > window_size)
          window_filled = window_size;
        break;

      default:
        return DECR_ILLEGALDATA; /* might as well */
      }

    }
  }

  if (togo != 0) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), window + frame_start, (size_t) outlen);

  LZX(window_posn) = window_posn;
  LZX(window_filled) = window_filled;
  LZX(R0) = R0;
  LZX(R1) = R1;
  LZX(R2) = R2;

  /* intel E8 decoding */
  if ((LZX(frames_read)++ < 32768) && LZX(intel_filesize) != 0) {
    if (outlen <= 6 || !LZX(intel_started)) {
      LZX(intel_curpos) += outlen;
    }
    else {
      cab_UBYTE *data    = CAB(outbuf);
      cab_UBYTE *dataend = data + outlen - 10;
      cab_LONG curpos    = LZX(intel_curpos);
      cab_LONG filesize  = LZX(intel_filesize);
      cab_LONG abs_off, rel_off;

      LZX(intel_curpos) = curpos + outlen;

      while (data < dataend) {
        if (*data++ != 0xE8) { curpos++; continue; }
        abs_off = (cab_LONG) ((cab_ULONG) data[0] |
                              ((cab_ULONG) data[1] << 8) |
                              ((cab_ULONG) data[2] << 16) |
                              ((cab_ULONG) data[3] << 24));
        if ((abs_off >= -curpos) && (abs_off < filesize)) {
          rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
          data[0] = (cab_UBYTE) rel_off;
          data[1] = (cab_UBYTE) (rel_off >> 8);
          data[2] = (cab_UBYTE) (rel_off >> 16);
          data[3] = (cab_UBYTE) (rel_off >> 24);
        }
        data += 4;
        curpos += 5;
      }
    }
  }
  return DECR_OK;
}
