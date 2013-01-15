/*
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
#ifndef GLIB_COMPAT_H_
# define GLIB_COMPAT_H_

#include <glib-object.h>
#include <gio/gio.h>

#if !GLIB_CHECK_VERSION(2,26,0)
char *   g_data_input_stream_read_upto (GDataInputStream  *stream,
                                        const gchar       *stop_chars,
                                        gssize             stop_chars_len,
                                        gsize             *length,
                                        GCancellable      *cancellable,
                                        GError           **error);
#endif /* glib 2.26 */

#if !GLIB_CHECK_VERSION(2,28,0)
#define g_clear_object(object_ptr) \
  G_STMT_START {                                                             \
    /* Only one access, please */                                            \
    gpointer *_p = (gpointer) (object_ptr);                                  \
    gpointer _o;                                                             \
                                                                             \
    do                                                                       \
      _o = g_atomic_pointer_get (_p);                                        \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_p, _o, NULL));\
                                                                             \
    if (_o)                                                                  \
      g_object_unref (_o);                                                   \
  } G_STMT_END

void
g_slist_free_full(GSList         *list,
                  GDestroyNotify free_func);

#endif /* glib 2.28 */

#if !GLIB_CHECK_VERSION(2,32,0)
GByteArray* g_byte_array_new_take (guint8 *data, gsize len);
#endif /* glib 2.32 */

#ifndef g_clear_pointer
#define g_clear_pointer(pp, destroy) \
  G_STMT_START {                                                               \
    G_STATIC_ASSERT (sizeof *(pp) == sizeof (gpointer));                       \
    /* Only one access, please */                                              \
    gpointer *_pp = (gpointer *) (pp);                                         \
    gpointer _p;                                                               \
    /* This assignment is needed to avoid a gcc warning */                     \
    GDestroyNotify _destroy = (GDestroyNotify) (destroy);                      \
                                                                               \
    (void) (0 ? (gpointer) *(pp) : 0);                                         \
    do                                                                         \
      _p = g_atomic_pointer_get (_pp);                                         \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_pp, _p, NULL)); \
                                                                               \
    if (_p)                                                                    \
      _destroy (_p);                                                           \
  } G_STMT_END
#endif /* g_clear_pointer */

#endif /* !GLIB_COMPAT_H_ */
