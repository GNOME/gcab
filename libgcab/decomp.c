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

#include <string.h>
#include "decomp.h"

#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif

/* Tables for deflate from PKZIP's appnote.txt. */

#define THOSE_ZIP_CONSTS                                                           \
static const cab_UBYTE Zipborder[] = /* Order of the bit length code lengths */    \
{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};               \
static const cab_UWORD Zipcplens[] = /* Copy lengths for literal codes 257..285 */ \
{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,             \
 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};                              \
static const cab_UWORD Zipcplext[] = /* Extra bits for literal codes 257..285 */   \
{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,             \
  4, 5, 5, 5, 5, 0, 99, 99}; /* 99==invalid */                                     \
static const cab_UWORD Zipcpdist[] = /* Copy offsets for distance codes 0..29 */   \
{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,             \
513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};          \
static const cab_UWORD Zipcpdext[] = /* Extra bits for distance codes */           \
{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,            \
10, 11, 11, 12, 12, 13, 13};                                                       \
/* And'ing with Zipmask[n] masks the lower n bits */                               \
static const cab_UWORD Zipmask[17] = {                                             \
 0x0000, 0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,           \
 0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff                    \
}

THOSE_ZIP_CONSTS;

#define CAB(x) (decomp_state->x)
#define ZIP(x) (decomp_state->methods.zip.x)
#define LZX(x) (decomp_state->methods.lzx.x)

#define ZIPNEEDBITS(n) {while(k<(n)){cab_LONG c=*(ZIP(inpos)++);\
    b|=((cab_ULONG)c)<<k;k+=8;}}
#define ZIPDUMPBITS(n) {b>>=(n);k-=(n);}

/********************************************************
 * Ziphuft_free (internal)
 */
static void fdi_Ziphuft_free(FDI_Int *fdi, struct Ziphuft *t)
{
  register struct Ziphuft *p, *q;

  /* Go through linked list, freeing from the allocated (t[-1]) address. */
  p = t;
  while (p != NULL)
  {
    q = (--p)->v.t;
    fdi->free(p);
    p = q;
  }
}

/*********************************************************
 * fdi_Ziphuft_build (internal)
 */
static cab_LONG fdi_Ziphuft_build(cab_ULONG *b, cab_ULONG n, cab_ULONG s, const cab_UWORD *d, const cab_UWORD *e,
struct Ziphuft **t, cab_LONG *m, fdi_decomp_state *decomp_state)
{
  cab_ULONG a;                   	/* counter for codes of length k */
  cab_ULONG el;                  	/* length of EOB code (value 256) */
  cab_ULONG f;                   	/* i repeats in table every f entries */
  cab_LONG g;                    	/* maximum code length */
  cab_LONG h;                    	/* table level */
  register cab_ULONG i;          	/* counter, current code */
  register cab_ULONG j;          	/* counter */
  register cab_LONG k;           	/* number of bits in current code */
  cab_LONG *l;                  	/* stack of bits per table */
  register cab_ULONG *p;         	/* pointer into ZIP(c)[],ZIP(b)[],ZIP(v)[] */
  register struct Ziphuft *q;           /* points to current table */
  struct Ziphuft r = {0x0};             /* table entry for structure assignment */
  register cab_LONG w;                  /* bits before this table == (l * h) */
  cab_ULONG *xp;                 	/* pointer into x */
  cab_LONG y;                           /* number of dummy codes added */
  cab_ULONG z;                   	/* number of entries in current table */

  l = ZIP(lx)+1;

  /* Generate counts for each bit length */
  el = n > 256 ? b[256] : ZIPBMAX; /* set length of EOB code, if any */

  for(i = 0; i < ZIPBMAX+1; ++i)
    ZIP(c)[i] = 0;
  p = b;  i = n;
  do
  {
    ZIP(c)[*p]++; p++;               /* assume all entries <= ZIPBMAX */
  } while (--i);
  if (ZIP(c)[0] == n)                /* null input--all zero length codes */
  {
    *t = NULL;
    *m = 0;
    return 0;
  }

  /* Find minimum and maximum length, bound *m by those */
  for (j = 1; j <= ZIPBMAX; j++)
    if (ZIP(c)[j])
      break;
  k = j;                        /* minimum code length */
  if ((cab_ULONG)*m < j)
    *m = j;
  for (i = ZIPBMAX; i; i--)
    if (ZIP(c)[i])
      break;
  g = i;                        /* maximum code length */
  if ((cab_ULONG)*m > i)
    *m = i;

  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= ZIP(c)[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= ZIP(c)[i]) < 0)
    return 2;
  ZIP(c)[i] += y;

  /* Generate starting offsets LONGo the value table for each length */
  ZIP(x)[1] = j = 0;
  p = ZIP(c) + 1;  xp = ZIP(x) + 2;
  while (--i)
  {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }

  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do{
    if ((j = *p++) != 0)
      ZIP(v)[ZIP(x)[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  ZIP(x)[0] = i = 0;                 /* first Huffman code is zero */
  p = ZIP(v);                        /* grab values in bit order */
  h = -1;                       /* no tables yet--level -1 */
  w = l[-1] = 0;                /* no bits decoded yet */
  ZIP(u)[0] = NULL;             /* just to keep compilers happy */
  q = NULL;                     /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = ZIP(c)[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l[h])
      {
        w += l[h++];            /* add bits already decoded */

        /* compute minimum size table less than or equal to *m bits */
        if ((z = g - w) > (cab_ULONG)*m)    /* upper limit */
          z = *m;
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = ZIP(c) + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if (*++xp > ZIPBMAX)
              return 2;         /* corrupt */
            if ((f <<= 1) <= *xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        if ((cab_ULONG)w + j > el && (cab_ULONG)w < el)
          j = el - w;           /* make EOB code end at table */
        z = 1 << j;             /* table entries for j-bit table */
        l[h] = j;               /* set table size in stack */

        /* allocate and link in new table */
        if (!(q = CAB(fdi)->alloc((z + 1)*sizeof(struct Ziphuft))))
        {
          if(h)
            fdi_Ziphuft_free(CAB(fdi), ZIP(u)[0]);
          return 3;             /* not enough memory */
        }
        *t = q + 1;             /* link to list for Ziphuft_free() */
        *(t = &(q->v.t)) = NULL;
        ZIP(u)[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          ZIP(x)[h] = i;              /* save pattern for backing up */
          r.b = (cab_UBYTE)l[h-1];    /* bits to dump before this table */
          r.e = (cab_UBYTE)(16 + j);  /* bits in this table */
          r.v.t = q;                  /* pointer to this table */
          j = (i & ((1 << w) - 1)) >> (w - l[h-1]);
          ZIP(u)[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (cab_UBYTE)(k - w);
      if (p >= ZIP(v) + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (cab_UBYTE)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = *p++;           /* simple code is just the value */
      }
      else
      {
        r.e = (cab_UBYTE)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* no tables */
      if (h < 0)
        return 2;               /* corrupt */

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != ZIP(x)[h])
        w -= l[--h];            /* don't need to update q */
    }
  }

  /* return actual size of base table */
  *m = l[0];

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}

/*********************************************************
 * fdi_Zipinflate_codes (internal)
 */
static cab_LONG fdi_Zipinflate_codes(const struct Ziphuft *tl, const struct Ziphuft *td,
  cab_LONG bl, cab_LONG bd, fdi_decomp_state *decomp_state)
{
  register cab_ULONG e;     /* table entry flag/number of extra bits */
  cab_ULONG n, d;           /* length and index for copy */
  cab_ULONG w;              /* current window position */
  const struct Ziphuft *t;  /* pointer to table entry */
  cab_ULONG ml, md;         /* masks for bl and bd bits */
  register cab_ULONG b;     /* bit buffer */
  register cab_ULONG k;     /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);                       /* initialize window position */

  /* inflate the coded data */
  ml = Zipmask[bl];           	/* precompute masks for speed */
  md = Zipmask[bd];

  for(;;)
  {
    ZIPNEEDBITS((cab_ULONG)bl)
    if((e = (t = tl + (b & ml))->e) > 16)
      do
      {
        if (e == 99)
          return 1;
        ZIPDUMPBITS(t->b)
        e -= 16;
        ZIPNEEDBITS(e)
      } while ((e = (t = t->v.t + (b & Zipmask[e]))->e) > 16);
    ZIPDUMPBITS(t->b)
    if (e == 16)                /* then it's a literal */
      CAB(outbuf)[w++] = (cab_UBYTE)t->v.n;
    else                        /* it's an EOB or a length */
    {
      /* exit if end of block */
      if(e == 15)
        break;

      /* get length of block to copy */
      ZIPNEEDBITS(e)
      n = t->v.n + (b & Zipmask[e]);
      ZIPDUMPBITS(e);

      /* decode distance of block to copy */
      ZIPNEEDBITS((cab_ULONG)bd)
      if ((e = (t = td + (b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          ZIPDUMPBITS(t->b)
          e -= 16;
          ZIPNEEDBITS(e)
        } while ((e = (t = t->v.t + (b & Zipmask[e]))->e) > 16);
      ZIPDUMPBITS(t->b)
      ZIPNEEDBITS(e)
      d = w - t->v.n - (b & Zipmask[e]);
      ZIPDUMPBITS(e)
      do
      {
        d &= ZIPWSIZE - 1;
        e = ZIPWSIZE - max(d, w);
        e = min(e, n);
        n -= e;
        do
        {
          CAB(outbuf)[w++] = CAB(outbuf)[d++];
        } while (--e);
      } while (n);
    }
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;

  /* done */
  return 0;
}

/***********************************************************
 * Zipinflate_stored (internal)
 */
static cab_LONG fdi_Zipinflate_stored(fdi_decomp_state *decomp_state)
/* "decompress" an inflated type 0 (stored) block. */
{
  cab_ULONG n;           /* number of bytes in block */
  cab_ULONG w;           /* current window position */
  register cab_ULONG b;  /* bit buffer */
  register cab_ULONG k;  /* number of bits in bit buffer */

  /* make local copies of globals */
  b = ZIP(bb);                       /* initialize bit buffer */
  k = ZIP(bk);
  w = ZIP(window_posn);              /* initialize window position */

  /* go to byte boundary */
  n = k & 7;
  ZIPDUMPBITS(n);

  /* get the length and its complement */
  ZIPNEEDBITS(16)
  n = (b & 0xffff);
  ZIPDUMPBITS(16)
  ZIPNEEDBITS(16)
  if (n != ((~b) & 0xffff))
    return 1;                   /* error in compressed data */
  ZIPDUMPBITS(16)

  /* read and output the compressed data */
  while(n--)
  {
    ZIPNEEDBITS(8)
    CAB(outbuf)[w++] = (cab_UBYTE)b;
    ZIPDUMPBITS(8)
  }

  /* restore the globals from the locals */
  ZIP(window_posn) = w;              /* restore global window pointer */
  ZIP(bb) = b;                       /* restore global bit buffer */
  ZIP(bk) = k;
  return 0;
}

/******************************************************
 * fdi_Zipinflate_fixed (internal)
 */
static cab_LONG fdi_Zipinflate_fixed(fdi_decomp_state *decomp_state)
{
  struct Ziphuft *fixed_tl;
  struct Ziphuft *fixed_td;
  cab_LONG fixed_bl, fixed_bd;
  cab_LONG i;                /* temporary variable */
  cab_ULONG *l;

  l = ZIP(ll);

  /* literal table */
  for(i = 0; i < 144; i++)
    l[i] = 8;
  for(; i < 256; i++)
    l[i] = 9;
  for(; i < 280; i++)
    l[i] = 7;
  for(; i < 288; i++)          /* make a complete, but wrong code set */
    l[i] = 8;
  fixed_bl = 7;
  if((i = fdi_Ziphuft_build(l, 288, 257, Zipcplens, Zipcplext, &fixed_tl, &fixed_bl, decomp_state)))
    return i;

  /* distance table */
  for(i = 0; i < 30; i++)      /* make an incomplete code set */
    l[i] = 5;
  fixed_bd = 5;
  if((i = fdi_Ziphuft_build(l, 30, 0, Zipcpdist, Zipcpdext, &fixed_td, &fixed_bd, decomp_state)) > 1)
  {
    fdi_Ziphuft_free(CAB(fdi), fixed_tl);
    return i;
  }

  /* decompress until an end-of-block code */
  i = fdi_Zipinflate_codes(fixed_tl, fixed_td, fixed_bl, fixed_bd, decomp_state);

  fdi_Ziphuft_free(CAB(fdi), fixed_td);
  fdi_Ziphuft_free(CAB(fdi), fixed_tl);
  return i;
}

/**************************************************************
 * fdi_Zipinflate_dynamic (internal)
 */
static cab_LONG fdi_Zipinflate_dynamic(fdi_decomp_state *decomp_state)
 /* decompress an inflated type 2 (dynamic Huffman codes) block. */
{
  cab_LONG i;          	/* temporary variables */
  cab_ULONG j;
  cab_ULONG *ll;
  cab_ULONG l;           	/* last length */
  cab_ULONG m;           	/* mask for bit lengths table */
  cab_ULONG n;           	/* number of lengths to get */
  struct Ziphuft *tl;           /* literal/length code table */
  struct Ziphuft *td;           /* distance code table */
  cab_LONG bl;                  /* lookup bits for tl */
  cab_LONG bd;                  /* lookup bits for td */
  cab_ULONG nb;          	/* number of bit length codes */
  cab_ULONG nl;          	/* number of literal/length codes */
  cab_ULONG nd;          	/* number of distance codes */
  register cab_ULONG b;         /* bit buffer */
  register cab_ULONG k;	        /* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);
  ll = ZIP(ll);

  /* read in table lengths */
  ZIPNEEDBITS(5)
  nl = 257 + (b & 0x1f);      /* number of literal/length codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(5)
  nd = 1 + (b & 0x1f);        /* number of distance codes */
  ZIPDUMPBITS(5)
  ZIPNEEDBITS(4)
  nb = 4 + (b & 0xf);         /* number of bit length codes */
  ZIPDUMPBITS(4)
  if(nl > 288 || nd > 32)
    return 1;                   /* bad lengths */

  /* read in bit-length-code lengths */
  for(j = 0; j < nb; j++)
  {
    ZIPNEEDBITS(3)
    ll[Zipborder[j]] = b & 7;
    ZIPDUMPBITS(3)
  }
  for(; j < 19; j++)
    ll[Zipborder[j]] = 0;

  /* build decoding table for trees--single level, 7 bit lookup */
  bl = 7;
  if((i = fdi_Ziphuft_build(ll, 19, 19, NULL, NULL, &tl, &bl, decomp_state)) != 0)
  {
    if(i == 1)
      fdi_Ziphuft_free(CAB(fdi), tl);
    return i;                   /* incomplete code set */
  }

  /* read in literal and distance code lengths */
  n = nl + nd;
  m = Zipmask[bl];
  i = l = 0;
  while((cab_ULONG)i < n)
  {
    ZIPNEEDBITS((cab_ULONG)bl)
    j = (td = tl + (b & m))->b;
    ZIPDUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 /* length of code in bits (0..15) */
      ll[i++] = l = j;          /* save last length in l */
    else if (j == 16)           /* repeat last length 3 to 6 times */
    {
      ZIPNEEDBITS(2)
      j = 3 + (b & 3);
      ZIPDUMPBITS(2)
      if((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           /* 3 to 10 zero length codes */
    {
      ZIPNEEDBITS(3)
      j = 3 + (b & 7);
      ZIPDUMPBITS(3)
      if ((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        /* j == 18: 11 to 138 zero length codes */
    {
      ZIPNEEDBITS(7)
      j = 11 + (b & 0x7f);
      ZIPDUMPBITS(7)
      if ((cab_ULONG)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }

  /* free decoding table for trees */
  fdi_Ziphuft_free(CAB(fdi), tl);

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* build the decoding tables for literal/length and distance codes */
  bl = ZIPLBITS;
  if((i = fdi_Ziphuft_build(ll, nl, 257, Zipcplens, Zipcplext, &tl, &bl, decomp_state)) != 0)
  {
    if(i == 1)
      fdi_Ziphuft_free(CAB(fdi), tl);
    return i;                   /* incomplete code set */
  }
  bd = ZIPDBITS;
  fdi_Ziphuft_build(ll + nl, nd, 0, Zipcpdist, Zipcpdext, &td, &bd, decomp_state);

  /* decompress until an end-of-block code */
  if(fdi_Zipinflate_codes(tl, td, bl, bd, decomp_state))
    return 1;

  /* free the decoding tables, return */
  fdi_Ziphuft_free(CAB(fdi), tl);
  fdi_Ziphuft_free(CAB(fdi), td);
  return 0;
}

/*****************************************************
 * fdi_Zipinflate_block (internal)
 */
static cab_LONG fdi_Zipinflate_block(cab_LONG *e, fdi_decomp_state *decomp_state) /* e == last block flag */
{ /* decompress an inflated block */
  cab_ULONG t;           	/* block type */
  register cab_ULONG b;     /* bit buffer */
  register cab_ULONG k;     /* number of bits in bit buffer */

  /* make local bit buffer */
  b = ZIP(bb);
  k = ZIP(bk);

  /* read in last block bit */
  ZIPNEEDBITS(1)
  *e = (cab_LONG)b & 1;
  ZIPDUMPBITS(1)

  /* read in block type */
  ZIPNEEDBITS(2)
  t = b & 3;
  ZIPDUMPBITS(2)

  /* restore the global bit buffer */
  ZIP(bb) = b;
  ZIP(bk) = k;

  /* inflate that block type */
  if(t == 2)
    return fdi_Zipinflate_dynamic(decomp_state);
  if(t == 0)
    return fdi_Zipinflate_stored(decomp_state);
  if(t == 1)
    return fdi_Zipinflate_fixed(decomp_state);
  /* bad block type */
  return 2;
}

/****************************************************
 * ZIPfdi_decomp(internal)
 */
int ZIPfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state)
{
  cab_LONG e;               /* last block flag */

  ZIP(inpos) = CAB(inbuf);
  ZIP(bb) = ZIP(bk) = ZIP(window_posn) = 0;
  if(outlen > ZIPWSIZE)
      return -1;

  /* CK = Chris Kirmse, official Microsoft purloiner */
  if(ZIP(inpos)[0] != 0x43 || ZIP(inpos)[1] != 0x4B)
    return -1;
  ZIP(inpos) += 2;

  do {
    if(fdi_Zipinflate_block(&e, decomp_state))
      return -1;
  } while(!e);

  /* return success */
  return 1;
}

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
static int make_decode_table(cab_ULONG nsyms, cab_ULONG nbits,
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
static int fdi_lzx_read_lens(cab_UBYTE *lens, cab_ULONG first, cab_ULONG last, struct lzx_bits *lb,
                  fdi_decomp_state *decomp_state) {
  cab_ULONG i,j, x,y;
  int z;

  register cab_ULONG bitbuf = lb->bb;
  register int bitsleft = lb->bl;
  cab_UBYTE *inpos = lb->ip;
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
  return 0;
}

/************************************************************
 * LZXfdi_init (internal)
 */
int LZXfdi_init(int window, fdi_decomp_state *decomp_state) {
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
  cab_ULONG wndsize = 1 << window;
  int posn_slots;

  /* LZX supports window sizes of 2^15 (32Kb) through 2^21 (2Mb) */
  /* if a previously allocated window is big enough, keep it     */
  if (window < 15 || window > 21) return DECR_DATAFORMAT;
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
int LZXfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state) {
  cab_UBYTE *inpos  = CAB(inbuf);
  const cab_UBYTE *endinp = inpos + inlen;
  cab_UBYTE *window = LZX(window);
  cab_UBYTE *runsrc, *rundest;
  cab_UWORD *hufftbl; /* used in READ_HUFFSYM macro as chosen decoding table */

  cab_ULONG window_posn = LZX(window_posn);
  cab_ULONG window_size = LZX(window_size);
  cab_ULONG R0 = LZX(R0);
  cab_ULONG R1 = LZX(R1);
  cab_ULONG R2 = LZX(R2);

  register cab_ULONG bitbuf;
  register int bitsleft;
  cab_ULONG match_offset, i,j,k; /* ijk used in READ_HUFFSYM macro */
  struct lzx_bits lb; /* used in READ_LENGTHS macro */

  int togo = outlen, this_run, main_element, aligned_bits;
  int match_length, copy_length, length_footer, extra, verbatim_bits;

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
        if (LZX(block_length) & 1) inpos++; /* realign bitstream to word */
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
        R0 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R1 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        R2 = inpos[0]|(inpos[1]<<8)|(inpos[2]<<16)|(inpos[3]<<24);inpos+=4;
        break;

      default:
        return DECR_ILLEGALDATA;
      }
    }

    /* buffer exhaustion check */
    if (inpos > endinp) {
      /* it's possible to have a file where the next run is less than
       * 16 bits in size. In this case, the READ_HUFFSYM() macro used
       * in building the tables will exhaust the buffer, so we should
       * allow for this, but not allow those accidentally read bits to
       * be used (so we check that there are at least 16 bits
       * remaining - in this boundary case they aren't really part of
       * the compressed data)
       */
      if (inpos > (endinp+2) || bitsleft < 16) return DECR_ILLEGALDATA;
    }

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
        if ((inpos + this_run) > endinp) return DECR_ILLEGALDATA;
        memcpy(window + window_posn, inpos, (size_t) this_run);
        inpos += this_run; window_posn += this_run;
        break;

      default:
        return DECR_ILLEGALDATA; /* might as well */
      }

    }
  }

  if (togo != 0) return DECR_ILLEGALDATA;
  memcpy(CAB(outbuf), window + ((!window_posn) ? window_size : window_posn) -
    outlen, (size_t) outlen);

  LZX(window_posn) = window_posn;
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
        abs_off = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
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
