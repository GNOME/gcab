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

/* CAB data blocks are <= 32768 bytes in uncompressed form. Uncompressed
 * blocks have zero growth. MSZIP guarantees that it won't grow above
 * uncompressed size by more than 12 bytes. LZX guarantees it won't grow
 * more than 6144 bytes.
 */
#define CAB_BLOCKMAX (32768)
#define CAB_INPUTMAX (CAB_BLOCKMAX+6144)

typedef unsigned char cab_UBYTE; /* 8 bits  */
typedef guint16       cab_UWORD; /* 16 bits */
typedef guint32       cab_ULONG; /* 32 bits */
typedef gint32        cab_LONG;  /* 32 bits */

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

typedef struct
{
    gpointer (*alloc) (gsize nbyte);
    void (*free) (gpointer mem);
} FDI_Int;

typedef struct fdi_cds_fwd {
    FDI_Int *fdi;                    /* the hfdi we are using                 */
    cab_UBYTE *inbuf; /* +2 for lzx bitbuffer overflows!       */
    cab_UBYTE *outbuf;
    union {
        struct ZIPstate zip;
    } methods;
} fdi_decomp_state;

int ZIPfdi_decomp(int inlen, int outlen, fdi_decomp_state *decomp_state);

#endif
