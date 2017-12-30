#if !defined MULTIBYTE_H
# define MULTIBYTE_H

#include <stdio.h>
#include <stddef.h>
#include <wchar.h>

/* A 'struct linebuffer' holds a line of multibyte text. */

struct wlinebuffer
{
  size_t size;                  /* Allocated. */
  size_t length;                /* Used. */
  wchar_t *buffer;
};

/* Initialize wlinebuffer LINEBUFFER for use. */

void initwbuffer (struct wlinebuffer *linebuffer);

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
   Consider lines to be terminated by DELIMITER.
   Keep the delimiter; append DELIMITER if we reach EOF and it wasn't
   the last character in the file.  Do not NUL-terminate.
   Return LINEBUFFER, except at end of file return NULL.  */

struct wlinebuffer *readwlinebuffer_delim (struct wlinebuffer *linebuffer,
                                         FILE *stream, wchar_t delimiter);

int xwmemcoll (wchar_t *, size_t, wchar_t *, size_t);

#endif
