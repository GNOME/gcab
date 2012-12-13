/*
 * LibGCab
 * Copyright (c) 2012, Marc-André Lureau <marcandre.lureau@gmail.com>
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
#ifndef _GCAB_FILE_H_
#define _GCAB_FILE_H_

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GCAB_TYPE_FILE             (gcab_file_get_type ())
#define GCAB_FILE(file)            (G_TYPE_CHECK_INSTANCE_CAST ((file), GCAB_TYPE_FILE, GCabFile))
#define GCAB_FILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GCAB_TYPE_FILE, GCabFileClass))
#define GCAB_IS_FILE(file)         (G_TYPE_CHECK_INSTANCE_TYPE ((file), GCAB_TYPE_FILE))
#define GCAB_IS_FILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GCAB_TYPE_FILE))
#define GCAB_FILE_GET_CLASS(file)  (G_TYPE_INSTANCE_GET_CLASS ((file), GCAB_TYPE_FILE, GCabFileClass))

typedef struct _GCabFileClass GCabFileClass;
typedef struct _GCabFile GCabFile;

typedef enum
{
    GCAB_COMPRESSION_NONE = 0,
    GCAB_COMPRESSION_MSZIP = 1,
} GCabCompression;

typedef void (*GCabFileSaveCallback) (GFile *current, gpointer data);

GType gcab_file_get_type (void) G_GNUC_CONST;

GCabFile *      gcab_file_new                 (void);
gboolean        gcab_file_add                 (GCabFile *cabfile,
                                               GFile *file,
                                               const gchar *name,
                                               gboolean recurse,
                                               GCancellable *cancellable,
                                               GError **error);
gboolean        gcab_file_save                (GCabFile *cabfile,
                                               GOutputStream *stream,
                                               GCabCompression compression,
                                               GCabFileSaveCallback file_callback,
                                               GFileProgressCallback progress_callback,
                                               gpointer callback_data,
                                               GCancellable *cancellable,
                                               GError **error);

G_END_DECLS

#endif /* _GCAB_FILE_H_ */
