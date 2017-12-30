#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <errno.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "xalloc.h"
#include "multibyte.h"
#include "error.h"
#include "exitfail.h"
#include "quotearg.h"
#include "quote.h"

/**** Wide version of linebuffer.c */

/* Initialize wlinebuffer LINEBUFFER for use. */

void
initwbuffer (struct wlinebuffer *linebuffer)
{
  memset (linebuffer, 0, sizeof *linebuffer);
}

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
   Consider lines to be terminated by DELIMITER.
   Keep the delimiter; append DELIMITER if it's the last line of a file
   that ends in a character other than DELIMITER.  Do not NUL-terminate.
   Therefore the stream can contain NUL bytes, and the length
   (including the delimiter) is returned in linebuffer->length.
   Return NULL when stream is empty.  Return NULL and set errno upon
   error; callers can distinguish this case from the empty case by
   invoking ferror (stream).
   Otherwise, return LINEBUFFER.  */

struct wlinebuffer *
readwlinebuffer_delim (struct wlinebuffer *linebuffer, FILE *stream,
                       wchar_t delimiter)
{
  int c;
  wchar_t *buffer = linebuffer->buffer;
  wchar_t *p = linebuffer->buffer;
  wchar_t *end = buffer + linebuffer->size; /* Sentinel. */

  if (feof (stream))
    return NULL;

  do
    {
      c = getwc (stream);
      if (c == WEOF)
        {
          if (p == buffer || ferror (stream))
            return NULL;
          if (p[-1] == delimiter)
            break;
          c = delimiter;
        }
      if (p == end)
        {
          size_t oldsize = linebuffer->size;
          buffer = x2nrealloc (buffer, &linebuffer->size, sizeof(wchar_t));
          p = buffer + oldsize;
          linebuffer->buffer = buffer;
          end = buffer + linebuffer->size;
        }
      *p++ = c;
    }
  while (c != delimiter);

  linebuffer->length = p - buffer;
  return linebuffer;
}

/**** Wide version of memcoll.c */

/* Compare S1 (with size S1SIZE) and S2 (with length S2SIZE) according
   to the LC_COLLATE locale.  S1 and S2 are both blocks of memory with
   nonzero sizes, and the last byte in each block must be a null byte.
   Set errno to an error number if there is an error, and to zero
   otherwise.  */

static int
wcscoll_loop (wchar_t const *s1, size_t s1size, wchar_t const *s2, size_t s2size)
{
  int diff;

  while (! (errno = 0, (diff = wcscoll (s1, s2)) || errno))
    {
      /* strcoll found no difference, but perhaps it was fooled by NUL
         characters in the data.  Work around this problem by advancing
         past the NUL chars.  */
      size_t size1 = wcslen (s1) + 1;
      size_t size2 = wcslen (s2) + 1;
      s1 += size1;
      s2 += size2;
      s1size -= size1;
      s2size -= size2;

      if (s1size == 0)
        return - (s2size != 0);
      if (s2size == 0)
        return 1;
    }

  return diff;
}

/* Compare S1 (with length S1LEN) and S2 (with length S2LEN) according
   to the LC_COLLATE locale.  S1 and S2 do not overlap, and are not
   adjacent.  Perhaps temporarily modify the bytes after S1 and S2,
   but restore their original contents before returning.  Set errno to an
   error number if there is an error, and to zero otherwise.  */

int
wmemcoll (wchar_t *s1, size_t s1len, wchar_t *s2, size_t s2len)
{
  int diff;

  /* strcoll is slow on many platforms, so check for the common case
     where the arguments are bytewise equal.  Otherwise, walk through
     the buffers using strcoll on each substring.  */

  if (s1len == s2len && memcmp (s1, s2, s1len * sizeof(wchar_t)) == 0)
    {
      errno = 0;
      diff = 0;
    }
  else
    {
      wchar_t n1 = s1[s1len];
      wchar_t n2 = s2[s2len];

      s1[s1len] = L'\0';
      s2[s2len] = L'\0';

      diff = wcscoll_loop (s1, s1len + 1, s2, s2len + 1);

      s1[s1len] = n1;
      s2[s2len] = n2;
    }

  return diff;
}

/**** Wide version of xmemcoll.c */

static void
wcollate_error (int collation_errno,
               wchar_t const *s1, size_t s1len,
               wchar_t const *s2, size_t s2len)
{
  error (0, collation_errno, _("string comparison failed"));
  error (0, 0, _("Set LC_ALL='C' to work around the problem."));
  // TODO: quote
  error (exit_failure, 0,
         _("The strings compared were %ls and %ls."), s1, s2);
}

/* Compare S1 (with length S1LEN) and S2 (with length S2LEN) according
   to the LC_COLLATE locale.  S1 and S2 do not overlap, and are not
   adjacent.  Temporarily modify the bytes after S1 and S2, but
   restore their original contents before returning.  Report an error
   and exit if there is an error.  */

int
xwmemcoll (wchar_t *s1, size_t s1len, wchar_t *s2, size_t s2len)
{
  int diff = wmemcoll (s1, s1len, s2, s2len);
  int collation_errno = errno;
  if (collation_errno)
    wcollate_error (collation_errno, s1, s1len, s2, s2len);
  return diff;
}

/**** Wide version of quotearg.c */

const char *
wquote (const wchar_t *s)
{
  size_t bytes = MB_LEN_MAX * (wcslen(s) + 1);
  char tmp[bytes];

  size_t n = wcstombs(tmp, s, bytes);
  return quote(tmp);
}

/**** Wide version of xstrndup.c */

/* Return a newly allocated copy of at most N characters of STRING.
   In other words, return a copy of the initial segment of length N of
   STRING.  */

wchar_t *
xwcsndup (const wchar_t *string, size_t n)
{
  size_t len = wcslen(string);
  if (len > n)
    {
      len = n;
    }

  wchar_t *s = xmalloc ((n + 1) * sizeof(wchar_t));
  if (! s)
    xalloc_die ();
  wcsncpy(s, string, n);
  s[n] = L'\0';
  return s;
}
