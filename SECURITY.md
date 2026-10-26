# Security policy

## Reporting a vulnerability

Please report security issues privately through the GitLab confidential issue
form:

<https://gitlab.gnome.org/GNOME/gcab/-/issues/new?issue[confidential]=true>

If you cannot use GitLab, contact the maintainer listed in `gcab.doap`.

Please include the gcab version, the input cabinet (or a minimised reproducer),
the command or API call used, and the sanitizer or crash output. Do not open a
public issue before a fix has been prepared.

## Supported versions

Fixes are made on the current development branch and backported to the latest
stable release when practical. Older releases are not supported.

## Extraction threat model

Cabinet archives are untrusted input. The library validates member paths,
rejects absolute and drive-letter paths, refuses to follow symbolic links in
existing extraction path components, and enforces configurable output-size and
compression-ratio limits. It also rejects overlapping file ranges and invalid
compressed data.

These checks are best effort. When extracting an archive, the destination
directory must be controlled by the caller and must not be writable by
untrusted users. A concurrent attacker can replace a checked path component
between validation and use, so extract hostile archives into a private
directory or a sandbox.

The `GCAB_SKIP_CHECKSUM` and `GCAB_SKIP_COMPRESSION_CHECK` environment
overrides exist only for tests and fuzzing. They are compiled out unless the
`test-env` build option is enabled and must never be enabled in production
builds.
