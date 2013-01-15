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

#include <string.h>

#include "glib-compat.h"

#if !GLIB_CHECK_VERSION(2,26,0)
static gssize
scan_for_chars (GDataInputStream *stream,
                gsize            *checked_out,
                const char       *stop_chars,
                gssize            stop_chars_len)
{
  GBufferedInputStream *bstream;
  const char *buffer;
  gsize start, end, peeked;
  int i;
  gsize available, checked;
  const char *stop_char;
  const char *stop_end;

  bstream = G_BUFFERED_INPUT_STREAM (stream);
  stop_end = stop_chars + stop_chars_len;

  checked = *checked_out;

  start = checked;
  buffer = (const char *)g_buffered_input_stream_peek_buffer (bstream, &available) + start;
  end = available;
  peeked = end - start;

  for (i = 0; checked < available && i < peeked; i++)
    {
      for (stop_char = stop_chars; stop_char != stop_end; stop_char++)
        {
          if (buffer[i] == *stop_char)
            return (start + i);
        }
    }

  checked = end;

  *checked_out = checked;
  return -1;
}

char *
g_data_input_stream_read_upto (GDataInputStream  *stream,
                               const gchar       *stop_chars,
                               gssize             stop_chars_len,
                               gsize             *length,
                               GCancellable      *cancellable,
                               GError           **error)
{
  GBufferedInputStream *bstream;
  gsize checked;
  gssize found_pos;
  gssize res;
  char *data_until;

  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);

  if (stop_chars_len < 0)
    stop_chars_len = strlen (stop_chars);

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  checked = 0;

  while ((found_pos = scan_for_chars (stream, &checked, stop_chars, stop_chars_len)) == -1)
    {
      if (g_buffered_input_stream_get_available (bstream) ==
          g_buffered_input_stream_get_buffer_size (bstream))
        g_buffered_input_stream_set_buffer_size (bstream,
                                                 2 * g_buffered_input_stream_get_buffer_size (bstream));

      res = g_buffered_input_stream_fill (bstream, -1, cancellable, error);
      if (res < 0)
        return NULL;
      if (res == 0)
        {
          /* End of stream */
          if (g_buffered_input_stream_get_available (bstream) == 0)
            {
              if (length)
                *length = 0;
              return NULL;
            }
          else
            {
              found_pos = checked;
              break;
            }
        }
    }

  data_until = g_malloc (found_pos + 1);

  res = g_input_stream_read (G_INPUT_STREAM (stream),
                             data_until,
                             found_pos,
                             NULL, NULL);
  if (length)
    *length = (gsize)found_pos;
  g_warn_if_fail (res == found_pos);
  data_until[found_pos] = 0;

  return data_until;
}

#endif

#if !GLIB_CHECK_VERSION(2,28,0)
/**
 * g_slist_free_full: (skip)
 * @list: a #GSList
 * @free_func: a #GDestroyNotify
 *
 * Convenience method, which frees all the memory used by a #GSList,
 * and calls the specified destroy function on every element's data
 *
 * Since: 2.28
 **/
G_GNUC_INTERNAL void
g_slist_free_full(GSList         *list,
                  GDestroyNotify free_func)
{
    GSList *el;

    if (free_func) {
        for (el = list; el ; el = g_slist_next(el)) {
            free_func(el);
        }
    }

    g_slist_free(list);
}
#endif /* 2.28 */

#if !GLIB_CHECK_VERSION(2,32,0)
GByteArray *
g_byte_array_new_take (guint8 *data,
                       gsize   len)
{
  GByteArray *array;

  array = g_byte_array_new ();
  g_assert (array->data == NULL);
  g_assert (array->len == 0);

  array->data = data;
  array->len = len;

  return array;
}
#endif /* 2.32 */
