#!/bin/sh
# Integration tests for the gcab command line tool.
#
# The build system sets GCAB to the freshly built executable. If GCAB_WRAPPER
# is set, it is prepended to the command, for example to run a Windows build
# under Wine. The wrapper's output is normalized to LF because Wine programs
# write CRLF line endings. GCAB_RETRIES controls how often a command that was
# killed with SIGKILL is retried (Wine occasionally does this while tearing
# down a process).

set -eu

: "${GCAB:?GCAB must point at the gcab executable}"

gcab_retries=${GCAB_RETRIES:-0}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/gcab-cli-XXXXXX")
gcab_out="$tmp/.gcab-output"
trap 'rm -f "$gcab_out"; rm -rf "$tmp"' EXIT HUP INT TERM

cd "$tmp"

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

run_gcab()
{
    if [ -n "${GCAB_WRAPPER:-}" ]; then
        attempts=0
        while :; do
            status=0
            # shellcheck disable=SC2086
            $GCAB_WRAPPER "$GCAB" "$@" >"$gcab_out" || status=$?
            if [ "$status" -ne 137 ] || [ "$attempts" -ge "$gcab_retries" ]; then
                break
            fi
            attempts=$((attempts + 1))
            echo "gcab was killed, retrying ($attempts/$gcab_retries)" >&2
        done
        tr -d '\r' <"$gcab_out"
        return "$status"
    fi

    "$GCAB" "$@"
}

mkdir -p input/sub
printf 'hello\n' > input/hello.txt
printf 'world\n' > input/sub/world.txt
: > input/empty.txt

# ---------------------------------------------------------------------------
# version and basic option validation
# ---------------------------------------------------------------------------
run_gcab --version | grep -q '^gcab ' || fail "--version output"
run_gcab --help >/dev/null 2>&1 || fail "--help"

if run_gcab --create --extract archive-none.cab input/hello.txt >/dev/null 2>&1; then
    fail "conflicting operations accepted"
fi

if run_gcab --create archive-none.cab >/dev/null 2>&1; then
    fail "create without input files accepted"
fi

if run_gcab --list missing.cab >/dev/null 2>&1; then
    fail "listing a missing archive succeeded"
fi

# ---------------------------------------------------------------------------
# uncompressed create / list / extract roundtrip
# ---------------------------------------------------------------------------
run_gcab --create --nopath archive-none.cab \
    input/hello.txt input/sub/world.txt input/empty.txt

run_gcab --list archive-none.cab > list.txt
grep -Fqx 'hello.txt' list.txt || fail "hello.txt not listed"
grep -Fqx 'world.txt' list.txt || fail "world.txt not listed"
grep -Fqx 'empty.txt' list.txt || fail "empty.txt not listed"

run_gcab --list-details archive-none.cab > details.txt
grep -q 'hello.txt 6 ' details.txt || fail "list-details size"
grep -q 'empty.txt 0 ' details.txt || fail "list-details empty size"

mkdir out-none
run_gcab --extract --directory out-none archive-none.cab
cmp input/hello.txt out-none/hello.txt
cmp input/sub/world.txt out-none/world.txt
test ! -s out-none/empty.txt || fail "empty file is not empty"

# extracting over existing files must replace them
run_gcab --extract --directory out-none archive-none.cab
cmp input/hello.txt out-none/hello.txt

# extraction must not escape the destination directory
mkdir out-safe
(
    cd out-safe
    run_gcab --extract --max-extract-size 0 ../archive-none.cab
)
cmp input/hello.txt out-safe/hello.txt

# ---------------------------------------------------------------------------
# MSZIP create / list / extract roundtrip
# ---------------------------------------------------------------------------
run_gcab --create --nopath --zip archive-mszip.cab \
    input/hello.txt input/sub/world.txt input/empty.txt

run_gcab --list archive-mszip.cab > list-mszip.txt
grep -Fqx 'hello.txt' list-mszip.txt || fail "MSZIP hello.txt not listed"

mkdir out-mszip
run_gcab --extract --directory out-mszip archive-mszip.cab
cmp input/hello.txt out-mszip/hello.txt
cmp input/sub/world.txt out-mszip/world.txt
test ! -s out-mszip/empty.txt || fail "MSZIP empty file is not empty"

# ---------------------------------------------------------------------------
# recursive paths are stored with cabinet separators and restored
# ---------------------------------------------------------------------------
run_gcab --create archive-path.cab input
run_gcab --list archive-path.cab | grep -Fqx 'input\hello.txt' ||
    fail "recursive file path not listed"
run_gcab --list archive-path.cab | grep -Fqx 'input\sub\world.txt' ||
    fail "recursive nested file path not listed"

mkdir out-path
run_gcab --extract --directory out-path archive-path.cab
cmp input/hello.txt out-path/input/hello.txt
cmp input/sub/world.txt out-path/input/sub/world.txt

# ---------------------------------------------------------------------------
# extraction limits and error handling
# ---------------------------------------------------------------------------
mkdir out-small
if run_gcab --extract -C out-small --max-extract-size 1 archive-none.cab \
        >/dev/null 2>&1; then
    fail "max-extract-size was not enforced"
fi

if run_gcab --extract --max-extract-size -1 -C out-small archive-none.cab \
        >/dev/null 2>&1; then
    fail "negative max-extract-size accepted"
fi

if run_gcab --create -s 70000 --nopath invalid-space.cab input/hello.txt \
        >/dev/null 2>&1; then
    fail "reserved size larger than the header field was accepted"
fi

if run_gcab --create empty.cab missing-input.txt >/dev/null 2>&1; then
    fail "creating from a missing input succeeded"
fi

# ---------------------------------------------------------------------------
# reserved area and signature dumping of an unsigned archive
# ---------------------------------------------------------------------------
run_gcab --create -s 64 signed-space.cab --nopath input/hello.txt
if run_gcab --dump-reserved signed-space.cab >/dev/null 2>&1; then
    fail "unsigned archive signature dump was expected to fail"
fi
test -f signed-space.cab.header || fail "reserved header was not dumped"

echo "CLI tests passed"
