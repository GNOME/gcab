GCab
====

A GObject library to create cabinet files

Building and testing
--------------------

    meson setup _build
    meson compile -C _build
    meson test -C _build --print-errorlogs

The test suite contains the GObject API self-tests and command line
integration tests for the `gcab` tool. See `CONTRIBUTING.md` for sanitizer,
Valgrind, coverage, and static-analysis builds.

The project treats compiler warnings as errors by default (`-Dwerror=true`).
Pass `-Dwerror=false` at setup time if a newer compiler emits diagnostics
outside the project's control. The CI pipeline additionally runs the tests
under AddressSanitizer and UndefinedBehaviorSanitizer, under Valgrind, with
coverage reporting, and with clang-tidy. It also builds the release tarball
and re-tests it from a clean extraction.

Security
--------

See `SECURITY.md` for how to report vulnerabilities.

When extracting a cabinet to a directory, the destination must be controlled by
the caller and must not be writable by untrusted users. Existing path
components are checked for symbolic links on a best-effort basis, but this
cannot prevent another process from replacing a component after it has been
checked. For hostile archives, extract into a private directory or a sandbox.

The GCAB_SKIP_CHECKSUM and GCAB_SKIP_COMPRESSION_CHECK overrides used by tests
and fuzzing are compiled out unless the test-env build option is enabled.

Fuzzing
-------

The legacy AFL entry point replays a file passed on the command line:

    CC=afl-gcc meson setup _build -Dtest-env=true ../
    AFL_HARDEN=1 meson compile -C _build
    mkdir -p corpus && cp tests/*.cab corpus/
    afl-fuzz -m 300 -i corpus -o findings ./_build/src/gcab-fuzz @@

For coverage-guided fuzzing with libFuzzer, or for OSS-Fuzz, use the
`gcab-fuzz-libfuzzer` target:

    CC=clang meson setup _build_libfuzzer -Dlibfuzzer=true -Dtest-env=true \
        -Db_sanitize=address,undefined -Db_lundef=false \
        -Ddocs=false -Dintrospection=false -Dvapi=false -Dnls=false
    meson compile -C _build_libfuzzer gcab-fuzz-libfuzzer
    mkdir -p corpus && cp tests/*.cab corpus/
    ./_build_libfuzzer/src/gcab-fuzz-libfuzzer corpus

The libFuzzer harness loads cabinets from memory and through a temporary
cabinet chain, looks up signatures, and extracts both to memory and to a
temporary directory. This covers chained cabinets, continuation blocks,
`gcab_cabinet_get_signature()` and the directory extraction path.

OSS-Fuzz builds can pass the engine through
`-Dfuzzing-engine="$LIB_FUZZING_ENGINE"` instead of using the default
`-fsanitize=fuzzer` link arguments.
