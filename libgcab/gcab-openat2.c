/*
 * LibGCab
 * Copyright (c) 2026, Marc-André Lureau <marcandre.lureau@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gio/gunixoutputstream.h>
#include <linux/openat2.h>
#include <sys/syscall.h>

#include "gcab-priv.h"

#define GCAB_OPENAT2_RESOLVE \
    (RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS)

static gint
_gcab_openat2 (gint dirfd, const gchar *path, guint64 flags, guint64 mode)
{
    struct open_how how = {
        .flags = flags,
        .mode = mode,
        .resolve = GCAB_OPENAT2_RESOLVE,
    };

    return (gint) syscall (SYS_openat2, dirfd, path, &how, sizeof (how));
}

static void
_gcab_openat2_set_error (GError **error, const gchar *path, gint errnum)
{
    switch (errnum) {
    case ELOOP:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Refusing to extract through symbolic link '%s'", path);
        break;
    case EXDEV:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "File path '%s' is outside the extraction directory", path);
        break;
    case ENOTDIR:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Extraction path component '%s' is not a directory", path);
        break;
    case EISDIR:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Cannot replace directory '%s' with a file", path);
        break;
    default:
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Cannot create '%s': %s", path, g_strerror (errnum));
        break;
    }
}

G_GNUC_INTERNAL GOutputStream *
gcab_file_get_output_stream_openat2 (GCabFile *self,
                                     GFile *path_extract,
                                     GError **error,
                                     gboolean *fallback)
{
    const gchar *extract_name = gcab_file_get_extract_name (self);
    g_autofree gchar *root_path = g_file_get_path (path_extract);
    g_autofree gchar *fname = NULL;
    g_auto(GStrv) components = NULL;
    gint root_fd = -1;
    gint dir_fd = -1;
    gint file_fd = -1;
    GOutputStream *out = NULL;
    guint n_components;

    *fallback = FALSE;

    /* Non-local GFiles cannot be resolved with openat2. */
    if (root_path == NULL) {
        *fallback = TRUE;
        return NULL;
    }

    if (extract_name == NULL) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "File has no extraction name");
        return NULL;
    }

    fname = g_strdup (extract_name);
    g_strdelimit (fname, "\\", '/');

    if (_gcab_file_name_is_unsafe (fname)) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Invalid file path '%s'", fname);
        return NULL;
    }

    components = g_strsplit (fname, "/", -1);
    n_components = g_strv_length (components);
    for (guint i = 0; i < n_components; i++) {
        if (components[i][0] == '\0' ||
            g_strcmp0 (components[i], ".") == 0 ||
            g_strcmp0 (components[i], "..") == 0) {
            g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                         "Invalid file path '%s'", fname);
            return NULL;
        }
    }

    root_fd = open (root_path, O_PATH | O_DIRECTORY | O_CLOEXEC);
    if (root_fd < 0) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Cannot open extraction directory '%s': %s",
                     root_path, g_strerror (errno));
        return NULL;
    }
    dir_fd = root_fd;

    /* Create and open each parent component relative to the previous one. */
    for (guint i = 0; i + 1 < n_components; i++) {
        gint next = _gcab_openat2 (dir_fd, components[i],
                                   O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
        if (next < 0 && errno == ENOENT) {
            if (mkdirat (dir_fd, components[i], 0777) < 0 && errno != EEXIST) {
                _gcab_openat2_set_error (error, components[i], errno);
                goto out;
            }
            next = _gcab_openat2 (dir_fd, components[i],
                                  O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
        }
        if (next < 0) {
            if (errno == ENOSYS) {
                *fallback = TRUE;
                goto out;
            }
            _gcab_openat2_set_error (error, components[i], errno);
            goto out;
        }
        if (dir_fd != root_fd)
            close (dir_fd);
        dir_fd = next;
    }

    file_fd = _gcab_openat2 (dir_fd, components[n_components - 1],
                             O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC |
                             O_NOFOLLOW | O_NONBLOCK | O_NOCTTY,
                             0666);
    if (file_fd < 0) {
        if (errno == ENOSYS) {
            *fallback = TRUE;
            goto out;
        }
        _gcab_openat2_set_error (error, fname, errno);
        goto out;
    }

    /* Do not write through pre-existing FIFOs, devices, or sockets. */
    struct stat st;
    if (fstat (file_fd, &st) < 0) {
        _gcab_openat2_set_error (error, fname, errno);
        goto out;
    }
    if (!S_ISREG (st.st_mode)) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                     "Refusing to replace non-regular file '%s'", fname);
        goto out;
    }

    out = g_unix_output_stream_new (file_fd, TRUE);
    if (out != NULL)
        file_fd = -1;

out:
    if (file_fd >= 0)
        close (file_fd);
    if (dir_fd >= 0 && dir_fd != root_fd)
        close (dir_fd);
    if (root_fd >= 0)
        close (root_fd);
    return out;
}
