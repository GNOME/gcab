#!/bin/sh
# Integration tests for the gcab command line tool.
#
# The build system sets GCAB to the freshly built executable.

set -eu

: "${GCAB:?GCAB must point at the gcab executable}"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/gcab-cli-XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cd "$tmp"

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

mkdir -p input/sub
printf 'hello\n' > input/hello.txt
printf 'world\n' > input/sub/world.txt
: > input/empty.txt

# ---------------------------------------------------------------------------
# version and basic option validation
# ---------------------------------------------------------------------------
"$GCAB" --version | grep -q '^gcab ' || fail "--version output"
"$GCAB" --help >/dev/null 2>&1 || fail "--help"

if "$GCAB" --create --extract archive-none.cab input/hello.txt >/dev/null 2>&1; then
    fail "conflicting operations accepted"
fi

if "$GCAB" --create archive-none.cab >/dev/null 2>&1; then
    fail "create without input files accepted"
fi

if "$GCAB" --list missing.cab >/dev/null 2>&1; then
    fail "listing a missing archive succeeded"
fi

# ---------------------------------------------------------------------------
# uncompressed create / list / extract roundtrip
# ---------------------------------------------------------------------------
"$GCAB" --create --nopath archive-none.cab \
    input/hello.txt input/sub/world.txt input/empty.txt

"$GCAB" --list archive-none.cab > list.txt
grep -Fqx 'hello.txt' list.txt || fail "hello.txt not listed"
grep -Fqx 'world.txt' list.txt || fail "world.txt not listed"
grep -Fqx 'empty.txt' list.txt || fail "empty.txt not listed"

"$GCAB" --list-details archive-none.cab > details.txt
grep -q 'hello.txt 6 ' details.txt || fail "list-details size"
grep -q 'empty.txt 0 ' details.txt || fail "list-details empty size"

mkdir out-none
"$GCAB" --extract --directory out-none archive-none.cab
cmp input/hello.txt out-none/hello.txt
cmp input/sub/world.txt out-none/world.txt
test ! -s out-none/empty.txt || fail "empty file is not empty"

# extraction must not escape the destination directory
mkdir out-safe
(
    cd out-safe
    "$GCAB" --extract --max-extract-size 0 ../archive-none.cab
)
cmp input/hello.txt out-safe/hello.txt

# ---------------------------------------------------------------------------
# MSZIP create / list / extract roundtrip
# ---------------------------------------------------------------------------
"$GCAB" --create --nopath --zip archive-mszip.cab \
    input/hello.txt input/sub/world.txt input/empty.txt

"$GCAB" --list archive-mszip.cab > list-mszip.txt
grep -Fqx 'hello.txt' list-mszip.txt || fail "MSZIP hello.txt not listed"

mkdir out-mszip
"$GCAB" --extract --directory out-mszip archive-mszip.cab
cmp input/hello.txt out-mszip/hello.txt
cmp input/sub/world.txt out-mszip/world.txt
test ! -s out-mszip/empty.txt || fail "MSZIP empty file is not empty"

# ---------------------------------------------------------------------------
# recursive paths are stored with cabinet separators and restored
# ---------------------------------------------------------------------------
"$GCAB" --create archive-path.cab input
"$GCAB" --list archive-path.cab | grep -Fqx 'input\hello.txt' ||
    fail "recursive file path not listed"
"$GCAB" --list archive-path.cab | grep -Fqx 'input\sub\world.txt' ||
    fail "recursive nested file path not listed"

mkdir out-path
"$GCAB" --extract --directory out-path archive-path.cab
cmp input/hello.txt out-path/input/hello.txt
cmp input/sub/world.txt out-path/input/sub/world.txt

# ---------------------------------------------------------------------------
# extraction limits and error handling
# ---------------------------------------------------------------------------
mkdir out-small
if "$GCAB" --extract -C out-small --max-extract-size 1 archive-none.cab \
        >/dev/null 2>&1; then
    fail "max-extract-size was not enforced"
fi

if "$GCAB" --extract --max-extract-size -1 -C out-small archive-none.cab \
        >/dev/null 2>&1; then
    fail "negative max-extract-size accepted"
fi

if "$GCAB" --create -s 70000 --nopath invalid-space.cab input/hello.txt \
        >/dev/null 2>&1; then
    fail "reserved size larger than the header field was accepted"
fi

if "$GCAB" --create empty.cab missing-input.txt >/dev/null 2>&1; then
    fail "creating from a missing input succeeded"
fi

# ---------------------------------------------------------------------------
# reserved area and signature dumping of an unsigned archive
# ---------------------------------------------------------------------------
"$GCAB" --create -s 64 signed-space.cab --nopath input/hello.txt
if "$GCAB" --dump-reserved signed-space.cab >/dev/null 2>&1; then
    fail "unsigned archive signature dump was expected to fail"
fi
test -f signed-space.cab.header || fail "reserved header was not dumped"

echo "CLI tests passed"
