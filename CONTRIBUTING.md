# Contributing to gcab

Thanks for your interest in gcab. The project is hosted on
[GNOME GitLab](https://gitlab.gnome.org/GNOME/gcab). Merge requests, bug
reports, and translation updates are welcome.

## Building

```sh
meson setup _build
meson compile -C _build
```

For a quick build without documentation, introspection, translations, or
Vala bindings:

```sh
meson setup _build -Ddocs=false -Dintrospection=false -Dvapi=false -Dnls=false
```

The default build treats compiler warnings as errors. If a compiler version
adds a diagnostic the project has not handled yet, configure with
`-Dwerror=false` to continue, and please report the warning.

## Tests

Run the full suite before submitting a change:

```sh
meson test -C _build --print-errorlogs
```

The suite contains the GObject API self-tests and the `gcab` command line
integration tests. The latter requires `sh` and standard POSIX utilities.

## Windows cross-build

CI cross-compiles for 64-bit Windows with MinGW and runs the full test suite
under Wine. On Fedora, a local build can be done with:

```sh
sudo dnf install mingw64-gcc mingw64-glib2 mingw64-zlib wine
meson setup _build_mingw --cross-file ci/mingw64-cross.ini \
    -Ddocs=false -Dintrospection=false -Dvapi=false -Dnls=false
meson compile -C _build_mingw
meson test -C _build_mingw --print-errorlogs
```

Useful development builds:

```sh
# Address and undefined-behaviour sanitizers
meson setup _build_san -Db_sanitize=address,undefined -Db_lundef=false \
    -Ddocs=false -Dintrospection=false -Dvapi=false -Dnls=false \
    -Dtest-env=true
meson test -C _build_san --print-errorlogs

# Valgrind
meson setup _build_vg -Ddocs=false -Dintrospection=false -Dvapi=false -Dnls=false
meson test -C _build_vg gcab-self-test gcab-decomp-test \
    --wrapper 'valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=99'

# Coverage (requires gcovr)
meson setup _build_cov -Db_coverage=true -Ddocs=false \
    -Dintrospection=false -Dvapi=false -Dnls=false
meson test -C _build_cov
gcovr -r . _build_cov --filter 'libgcab/' --filter 'src/' \
    --merge-mode-functions=merge-use-line-min --txt
```

## Fuzzing

See the Fuzzing section in `README.md`. New regression inputs for malformed
cabinets belong in `tests/` as small `.cab` files and should be registered in
`tests/gcab-self-test.c`.

## Coding style

- Follow the existing GLib/GObject style and the configured
  `gobject-linter` rules.
- Keep the public API annotated for GObject introspection.
- Add argument validation with `g_return_*_if_fail()` to public entry points.
- Add a `Since:` tag to new API and update `NEWS`.
- Do not enable the test-only environment overrides in production builds.

## Commit messages

Use a short imperative subject line, optionally with a Conventional Commits
prefix, for example:

```text
fix(extract): reject empty member names
tests: cover multi-block MSZIP roundtrips
ci: run the test suite under sanitizers
```

Keep commits focused. Explain the user-visible effect and any security impact
in the body.
