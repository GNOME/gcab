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

/**
 * GCabFileAttribute:
 * @GCAB_FILE_ATTRIBUTE_RDONLY: file is read-only
 * @GCAB_FILE_ATTRIBUTE_HIDDEN: file is hidden
 * @GCAB_FILE_ATTRIBUTE_SYSTEM: file is a system file
 * @GCAB_FILE_ATTRIBUTE_ARCH: file modified since last backup
 * @GCAB_FILE_ATTRIBUTE_EXEC: run after extraction
 * @GCAB_FILE_ATTRIBUTE_NAME_IS_UTF: name contains UTF
 *
 * Attributes associated with the #GCabFile.
 **/
typedef enum
{
  GCAB_FILE_ATTRIBUTE_RDONLY      = 0x01,
  GCAB_FILE_ATTRIBUTE_HIDDEN      = 0x02,
  GCAB_FILE_ATTRIBUTE_SYSTEM      = 0x04,
  GCAB_FILE_ATTRIBUTE_ARCH        = 0x20,
  GCAB_FILE_ATTRIBUTE_EXEC        = 0x40,
  GCAB_FILE_ATTRIBUTE_NAME_IS_UTF = 0x80
} GCabFileAttribute;

/**
 * GCabFile:
 *
 * An opaque object, referencing a file in a Cabinet.
 **/
typedef struct _GCabFile GCabFile;

/**
 * GCabFileCallback:
 * @file: the file being processed
 * @user_data: user data passed to the callback.
 *
 * The type used for callback called when processing Cabinet archive
 * files.
 **/
typedef gboolean (*GCabFileCallback) (GCabFile *file, gpointer user_data);

GType gcab_file_get_type (void) G_GNUC_CONST;

GCabFile *      gcab_file_new_with_file             (const gchar *name, GFile *file);
GFile *         gcab_file_get_file                  (GCabFile *file);
const gchar *   gcab_file_get_name                  (GCabFile *file);
guint32         gcab_file_get_size                  (GCabFile *file);
guint32         gcab_file_get_attributes            (GCabFile *file);
void            gcab_file_get_date                  (GCabFile *file, GTimeVal *result);
const gchar *   gcab_file_get_extract_name          (GCabFile *file);
void            gcab_file_set_extract_name          (GCabFile *file, const gchar *name);

G_END_DECLS

#endif /* _GCAB_FILE_H_ */
