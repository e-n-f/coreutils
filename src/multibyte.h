#if !defined MULTIBYTE_H
# define MULTIBYTE_H

#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>

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

const char *wquote (const wchar_t *s);

extern wchar_t *xwcsndup (const wchar_t *string, size_t n);

ssize_t wgetndelim2 (wchar_t **lineptr, size_t *linesize, size_t offset, size_t nmax,
                     wchar_t delim1, wchar_t delim2, FILE *stream);

wchar_t *xwcsdup (wchar_t const *string);

/* '\n' is considered a field separator with  --zero-terminated.  */
static inline bool
wfield_sep (wchar_t ch)
{
  return iswblank (ch) || ch == L'\n';
}

#define WSTREQ_LEN(a, b, n) (wcsncmp (a, b, n) == 0)

#endif
