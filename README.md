GCab
====

A GObject library to create cabinet files

Fuzzing
-------

    CC=afl-gcc meson --default-library=static ../
    AFL_HARDEN=1 ninja
    export GCAB_SKIP_CHECKSUM=1
    afl-fuzz -m 300 -i ../tests/fuzzing/ -o findings ./gcab --list-details @@
    afl-fuzz -m 300 -i ../tests/fuzzing/ -o findings2 ./gcab --directory=/tmp --extract @@
