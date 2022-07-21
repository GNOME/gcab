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

#ifndef MSZIP_H_
# define MSZIP_H_

#include <glib.h>

#define DECR_ILLEGALDATA -1
#define DECR_DATAFORMAT -2
#define DECR_NOMEMORY -3
#define DECR_OK 1

/* Bitstream reading macros (LZX / intel little-endian byte order)
 *
 * INIT_BITSTREAM    should be used first to set up the system
 * READ_BITS(var,n)  takes N bits from the buffer and puts them in var
 *
 * ENSURE_BITS(n)    ensures there are at least N bits in the bit buffer.
 *                   it can guarantee up to 17 bits (i.e. it can read in
 *                   16 new bits when there is down to 1 bit in the buffer,
 *                   and it can read 32 bits when there are 0 bits in the
 *                   buffer).
 * PEEK_BITS(n)      extracts (without removing) N bits from the bit buffer
 * REMOVE_BITS(n)    removes N bits from the bit buffer
 *
 * These bit access routines work by using the area beyond the MSB and the
 * LSB as a free source of zeroes. This avoids having to mask any bits.
 * So we have to know the bit width of the bitbuffer variable.
 */

#define INIT_BITSTREAM do { bitsleft = 0; bitbuf = 0; } while (0)

/* Quantum reads bytes in normal order; LZX is little-endian order */
#define ENSURE_BITS(n)                                                    \
  while (bitsleft < (n)) {                                                \
    bitbuf |= ((inpos[1]<<8)|inpos[0]) << (CAB_ULONG_BITS-16 - bitsleft); \
    bitsleft += 16; inpos+=2;                                             \
  }

#define PEEK_BITS(n)   (bitbuf >> (CAB_ULONG_BITS - (n)))
#define REMOVE_BITS(n) ((bitbuf <<= (n)), (bitsleft -= (n)))

#define READ_BITS(v,n) do {                                             \
  if (n) {                                                              \
    ENSURE_BITS(n);                                                     \
    (v) = PEEK_BITS(n);                                                 \
    REMOVE_BITS(n);                                                     \
  }                                                                     \
  else {                                                                \
    (v) = 0;                                                            \
  }                                                                     \
} while (0)

/* Huffman macros */

#define TABLEBITS(tbl)   (LZX_##tbl##_TABLEBITS)
#define MAXSYMBOLS(tbl)  (LZX_##tbl##_MAXSYMBOLS)
#define SYMTABLE(tbl)    (LZX(tbl##_table))
#define LENTABLE(tbl)    (LZX(tbl##_len))

/* BUILD_TABLE(tablename) builds a huffman lookup table from code lengths.
 * In reality, it just calls make_decode_table() with the appropriate
 * values - they're all fixed by some #defines anyway, so there's no point
 * writing each call out in full by hand.
 */
#define BUILD_TABLE(tbl)                                                \
  if (make_decode_table(                                                \
    MAXSYMBOLS(tbl), TABLEBITS(tbl), LENTABLE(tbl), SYMTABLE(tbl)       \
  )) { return DECR_ILLEGALDATA; }

/* READ_HUFFSYM(tablename, var) decodes one huffman symbol from the
 * bitstream using the stated table and puts it in var.
 */
#define READ_HUFFSYM(tbl,var) do {                                      \
  ENSURE_BITS(16);                                                      \
  hufftbl = SYMTABLE(tbl);                                              \
  if ((i = hufftbl[PEEK_BITS(TABLEBITS(tbl))]) >= MAXSYMBOLS(tbl)) {    \
    j = 1 << (CAB_ULONG_BITS - TABLEBITS(tbl));                         \
    do {                                                                \
      j >>= 1; i <<= 1; i |= (bitbuf & j) ? 1 : 0;                      \
      if (!j) { return DECR_ILLEGALDATA; }                              \
    } while ((i = hufftbl[i]) >= MAXSYMBOLS(tbl));                      \
  }                                                                     \
  j = LENTABLE(tbl)[(var) = i];                                         \
  REMOVE_BITS(j);                                                       \
} while (0)

/* READ_LENGTHS(tablename, first, last) reads in code lengths for symbols
 * first to last in the given table. The code lengths are stored in their
 * own special LZX way.
 */
#define READ_LENGTHS(tbl,first,last,fn) do { \
  lb.bb = bitbuf; lb.bl = bitsleft; lb.ip = inpos; \
  if (fn(LENTABLE(tbl),(first),(last),&lb,decomp_state)) { \
    return DECR_ILLEGALDATA; \
  } \
  bitbuf = lb.bb; bitsleft = lb.bl; inpos = lb.ip; \
} while (0)

typedef guint8        cab_UBYTE; /* 8 bits  */
typedef guint16       cab_UWORD; /* 16 bits */
typedef guint32       cab_ULONG; /* 32 bits */
typedef gint32        cab_LONG;  /* 32 bits */

/* number of bits in a ULONG */
#ifndef CHAR_BIT
# define CHAR_BIT (8)
#endif
#define CAB_ULONG_BITS (sizeof(cab_ULONG) * CHAR_BIT)

/* MSZIP stuff */
#define ZIPWSIZE 	0x8000  /* window size */
#define ZIPLBITS	9	/* bits in base literal/length lookup table */
#define ZIPDBITS	6	/* bits in base distance lookup table */
#define ZIPBMAX		16      /* maximum bit length of any code */
#define ZIPN_MAX	288     /* maximum number of codes in any set */

struct Ziphuft {
    cab_UBYTE e;                /* number of extra bits or operation */
    cab_UBYTE b;                /* number of bits in this code or subcode */
    union {
        cab_UWORD n;              /* literal, length base, or distance base */
        struct Ziphuft *t;        /* pointer to next level of table */
    } v;
};
struct ZIPstate {
    cab_ULONG window_posn;      /* current offset within the window        */
    cab_ULONG bb;               /* bit buffer */
    cab_ULONG bk;               /* bits in bit buffer */
    cab_ULONG ll[288+32];       /* literal/length and distance code lengths */
    cab_ULONG c[ZIPBMAX+1];     /* bit length count table */
    cab_LONG  lx[ZIPBMAX+1];    /* memory for l[-1..ZIPBMAX-1] */
    struct Ziphuft *u[ZIPBMAX];	/* table stack */
    cab_ULONG v[ZIPN_MAX];      /* values in order of bit length */
    cab_ULONG x[ZIPBMAX+1];     /* bit offsets, then code stack */
    cab_UBYTE *inpos;
};

/* LZX stuff */

/* some constants defined by the LZX specification */
#define LZX_MIN_MATCH                (2)
#define LZX_MAX_MATCH                (257)
#define LZX_NUM_CHARS                (256)
#define LZX_BLOCKTYPE_INVALID        (0)   /* also blocktypes 4-7 invalid */
#define LZX_BLOCKTYPE_VERBATIM       (1)
#define LZX_BLOCKTYPE_ALIGNED        (2)
#define LZX_BLOCKTYPE_UNCOMPRESSED   (3)
#define LZX_PRETREE_NUM_ELEMENTS     (20)
#define LZX_ALIGNED_NUM_ELEMENTS     (8)   /* aligned offset tree #elements */
#define LZX_NUM_PRIMARY_LENGTHS      (7)   /* this one missing from spec! */
#define LZX_NUM_SECONDARY_LENGTHS    (249) /* length tree #elements */

/* LZX huffman defines: tweak tablebits as desired */
#define LZX_PRETREE_MAXSYMBOLS  (LZX_PRETREE_NUM_ELEMENTS)
#define LZX_PRETREE_TABLEBITS   (6)
#define LZX_MAINTREE_MAXSYMBOLS (LZX_NUM_CHARS + 50*8)
#define LZX_MAINTREE_TABLEBITS  (12)
#define LZX_LENGTH_MAXSYMBOLS   (LZX_NUM_SECONDARY_LENGTHS+1)
#define LZX_LENGTH_TABLEBITS    (12)
#define LZX_ALIGNED_MAXSYMBOLS  (LZX_ALIGNED_NUM_ELEMENTS)
#define LZX_ALIGNED_TABLEBITS   (7)

#define LZX_LENTABLE_SAFETY (64) /* we allow length table decoding overruns */

#define LZX_DECLARE_TABLE(tbl) \
  cab_UWORD tbl##_table[(1<<LZX_##tbl##_TABLEBITS) + (LZX_##tbl##_MAXSYMBOLS<<1)];\
  cab_UBYTE tbl##_len  [LZX_##tbl##_MAXSYMBOLS + LZX_LENTABLE_SAFETY]

struct LZXstate {
    cab_UBYTE *window;         /* the actual decoding window              */
    cab_ULONG window_size;     /* window size (32Kb through 2Mb)          */
    cab_ULONG actual_size;     /* window size when it was first allocated */
    cab_ULONG window_posn;     /* current offset within the window        */
    cab_ULONG R0, R1, R2;      /* for the LRU offset system               */
    cab_UWORD main_elements;   /* number of main tree elements            */
    int   header_read;         /* have we started decoding at all yet?    */
    cab_UWORD block_type;      /* type of this block                      */
    cab_ULONG block_length;    /* uncompressed length of this block       */
    cab_ULONG block_remaining; /* uncompressed bytes still left to decode */
    cab_ULONG frames_read;     /* the number of CFDATA blocks processed   */
    cab_LONG  intel_filesize;  /* magic header value used for transform   */
    cab_LONG  intel_curpos;    /* current offset in transform space       */
    int   intel_started;       /* have we seen any translatable data yet? */

    LZX_DECLARE_TABLE(PRETREE);
    LZX_DECLARE_TABLE(MAINTREE);
    LZX_DECLARE_TABLE(LENGTH);
    LZX_DECLARE_TABLE(ALIGNED);
};

struct lzx_bits {
  cab_ULONG bb;
  int bl;
  cab_UBYTE *ip;
};

typedef struct
{
    gpointer (*alloc) (gsize nbyte);
    void (*free) (gpointer mem);
} FDI_Int;

typedef struct fdi_cds_fwd {
    FDI_Int *fdi;                    /* the hfdi we are using                 */
    cab_UBYTE *inbuf; /* +2 for lzx bitbuffer overflows!       */
    cab_UBYTE *outbuf;

    cab_ULONG lzx_position_base[51];
    cab_UBYTE extra_bits[51];

    union {
        struct ZIPstate zip;
        struct LZXstate lzx;
    } methods;
    int comptype;
} fdi_decomp_state;

int ZIPfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state);
int LZXfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state);
int LZXfdi_init(int window, fdi_decomp_state *decomp_state);
void LZXfdi_clear(fdi_decomp_state *decomp_state);

#endif
