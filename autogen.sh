#!/bin/sh

set -e # exit on errors

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

olddir=`pwd`

mkdir -p "$srcdir/m4"

cd "$srcdir"

gtkdocize
autoreconf -v --force --install
intltoolize -f

cd "$olddir"

if [ -z "$NOCONFIGURE" ]; then
    "$srcdir"/configure --enable-gtk-doc "$@"
fi
