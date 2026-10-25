GCab
====

A GObject library to create cabinet files

Security
--------

When extracting a cabinet to a directory, the destination must be controlled by
the caller and must not be writable by untrusted users. Existing path
components are checked for symbolic links on a best-effort basis, but this
cannot prevent another process from replacing a component after it has been
checked. For hostile archives, extract into a private directory or a sandbox.

The GCAB_SKIP_CHECKSUM and GCAB_SKIP_COMPRESSION_CHECK overrides used by tests
and fuzzing are compiled out unless the test-env build option is enabled.

Fuzzing
-------

    CC=afl-gcc meson -Dtest-env=true --default-library=static ../
    AFL_HARDEN=1 ninja
    afl-fuzz -m 300 -i ../tests/fuzzing/ -o findings ./src/gcab-fuzz @@
