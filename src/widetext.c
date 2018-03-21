/* widetext - utility functions for strings of wide characters

   Copyright (C) 2018 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)

#include "xalloc.h"
#include "grapheme.h"
#include "widetext.h"
#include "error.h"
#include "quote.h"

/**** Wide version of linebuffer.c */

/* Initialize grlinebuffer LINEBUFFER for use. */

void
initgrbuffer (struct grlinebuffer *linebuffer)
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

struct grlinebuffer *
readgrlinebuffer_delim (struct grlinebuffer *linebuffer, FILE *stream,
                        wchar_t delimiter, mbstate_t *mbs)
{
  grapheme c;
  grapheme *buffer = linebuffer->buffer;
  grapheme *p = linebuffer->buffer;
  grapheme *end = buffer + linebuffer->size; /* Sentinel. */

  if (feof (stream))
    return NULL;

  do
    {
      c = fgetgr (stream, mbs);
      if (c.c == WEOF)
        {
          if (p == buffer || ferror (stream))
            return NULL;
          if (p[-1].c == delimiter)
            break;
          c = grapheme_wchar (delimiter);
        }
      if (p == end)
        {
          size_t oldsize = linebuffer->size;
          buffer = x2nrealloc (buffer, &linebuffer->size, sizeof(grapheme));
          p = buffer + oldsize;
          linebuffer->buffer = buffer;
          end = buffer + linebuffer->size;
        }
      *p++ = c;
    }
  while (c.c != delimiter);

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

static int
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
  error (0, 0, _("set LC_ALL='C' to work around the problem"));
  // TODO: quote
  error (EXIT_FAILURE, 0,
         _("the strings compared were %ls and %ls"), s1, s2);
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

int
xgrmemcoll (grapheme *s1, size_t s1len, grapheme *s2, size_t s2len)
{
  wchar_t tmp1[s1len];
  wchar_t tmp2[s2len];

  for (size_t i = 0; i < s1len; i++)
    tmp1[i] = s1[i].c;
  for (size_t i = 0; i < s2len; i++)
    tmp2[i] = s2[i].c;

  int cmp = xwmemcoll (tmp1, s1len, tmp2, s2len);
  if (cmp != 0)
    return cmp;

  if (s1len < s2len)
    return -1;
  if (s1len > s2len)
    return 1;

  for (size_t i = 0; i < s1len; i++)
    {
      if (s1[i].isbyte != s2[i].isbyte)
        return s1[i].isbyte - s2[i].isbyte;
    }

  return 0;
}

/**** Wide version of quotearg.c */

const char *
wquote (const wchar_t *s)
{
  size_t bytes = MB_LEN_MAX * (wcslen(s) + 1);
  char tmp[bytes];

  size_t n = wcstombs(tmp, s, bytes);
  if (n == (size_t) -1)
    return _("conversion error");
  else
    return quote (tmp);
}

const char *
grnstr (const grapheme *s, size_t n)
{
  size_t bytes = MB_LEN_MAX * (n + 1);
  char tmp[bytes];
  size_t out = 0;

  for (size_t i = 0; i < n; i++)
    {
      if (s[i].isbyte)
        tmp[out++] = s[i].c;
      else
        {
          int nn = wctomb(tmp + out, s[i].c);
          if (nn > 0)
            out += nn;
        }
    }
  tmp[out] = '\0';
  return xstrdup(tmp);
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

/**** Wide version of getndelim2.c */

/* The maximum value that getndelim2 can return without suffering from
   overflow problems, either internally (because of pointer
   subtraction overflow) or due to the API (because of ssize_t).  */
#define GETNDELIM2_MAXIMUM (PTRDIFF_MAX < SSIZE_MAX ? PTRDIFF_MAX : SSIZE_MAX)

/* Try to add at least this many bytes when extending the buffer.
   MIN_CHUNK must be no greater than GETNDELIM2_MAXIMUM.  */
#define MIN_CHUNK 64

ssize_t
grgetndelim2 (grapheme **lineptr, size_t *linesize, size_t offset, size_t nmax,
              wchar_t delim1, wchar_t delim2, FILE *stream, mbstate_t *mbs)
{
  size_t nbytes_avail;          /* Allocated but unused bytes in *LINEPTR.  */
  grapheme *read_pos;               /* Where we're reading into *LINEPTR. */
  ssize_t bytes_stored = -1;
  grapheme *ptr = *lineptr;
  size_t size = *linesize;
  bool found_delimiter;

  if (!ptr)
    {
      size = nmax < MIN_CHUNK ? nmax : MIN_CHUNK;
      ptr = malloc (size * sizeof(grapheme));
      if (!ptr)
        return -1;
    }

  if (size < offset)
    goto done;

  nbytes_avail = size - offset;
  read_pos = ptr + offset;

  if (nbytes_avail == 0 && nmax <= size)
    goto done;

  /* Normalize delimiters, since memchr2 doesn't handle EOF.  */
  if (delim1 == WEOF)
    delim1 = delim2;
  else if (delim2 == WEOF)
    delim2 = delim1;

  flockfile (stream);

  found_delimiter = false;
  do
    {
      /* Here always ptr + size == read_pos + nbytes_avail.
         Also nbytes_avail > 0 || size < nmax.  */

      grapheme c = grapheme_wchar (L'\0');

      size_t buffer_len;

      if (true)
        {
          c = fgetgr (stream, mbs);
          if (c.c == WEOF)
            {
              /* Return partial line, if any.  */
              if (read_pos == ptr)
                goto unlock_done;
              else
                break;
            }
          if (c.c == delim1 || c.c == delim2)
            found_delimiter = true;
          buffer_len = 1;
        }

      /* We always want at least one byte left in the buffer, since we
         always (unless we get an error while reading the first byte)
         NUL-terminate the line buffer.  */

      if (nbytes_avail < buffer_len + 1 && size < nmax)
        {
          /* Grow size proportionally, not linearly, to avoid O(n^2)
             running time.  */
          size_t newsize = size < MIN_CHUNK ? size + MIN_CHUNK : 2 * size;
          grapheme *newptr;

          /* Increase newsize so that it becomes
             >= (read_pos - ptr) + buffer_len.  */
          if (newsize - (read_pos - ptr) < buffer_len + 1)
            newsize = (read_pos - ptr) + buffer_len + 1;
          /* Respect nmax.  This handles possible integer overflow.  */
          if (! (size < newsize && newsize <= nmax))
            newsize = nmax;

          if (GETNDELIM2_MAXIMUM < newsize - offset)
            {
              size_t newsizemax = offset + GETNDELIM2_MAXIMUM + 1;
              if (size == newsizemax)
                goto unlock_done;
              newsize = newsizemax;
            }

          nbytes_avail = newsize - (read_pos - ptr);
          newptr = realloc (ptr, newsize * sizeof(grapheme));
          if (!newptr)
            goto unlock_done;
          ptr = newptr;
          size = newsize;
          read_pos = size - nbytes_avail + ptr;
        }

      /* Here, if size < nmax, nbytes_avail >= buffer_len + 1.
         If size == nmax, nbytes_avail > 0.  */

      if (1 < nbytes_avail)
        {
          size_t copy_len = nbytes_avail - 1;
          if (buffer_len < copy_len)
            copy_len = buffer_len;
          *read_pos = c;
          read_pos += copy_len;
          nbytes_avail -= copy_len;
        }

      /* Here still nbytes_avail > 0.  */
    }
  while (!found_delimiter);

  /* Done - NUL terminate and return the number of bytes read.
     At this point we know that nbytes_avail >= 1.  */
  *read_pos = grapheme_wchar (L'\0');
  bytes_stored = read_pos - (ptr + offset);

 unlock_done:
  funlockfile (stream);

 done:
  *lineptr = ptr;
  *linesize = size;
  return bytes_stored ? bytes_stored : -1;
}

/**** Wide version of xmalloc.c */

/* Clone STRING.  */

wchar_t *
xwcsdup (wchar_t const *string)
{
  return xmemdup (string, (wcslen(string) + 1) * sizeof(wchar_t));
}


/**** Wide version of lib/strnumcmp-in.h */

# define WNEGATION_SIGN   L'-'
# define WNUMERIC_ZERO    L'0'

static inline int
ISWDIGIT(wchar_t c)
{
  return c >= L'0' && c - L'0' <= 9;
}

static inline int _GL_ATTRIBUTE_PURE
wfraccompare (char const *a, char const *b, wint_t decimal_point, mbstate_t *mbsa, mbstate_t *mbsb)
{
  const char *aend = a + strlen (a);
  const char *bend = b + strlen (b);

  if ((grpeek (&a, aend, mbsa).c) == decimal_point && (grpeek (&b, bend, mbsb)).c == decimal_point)
    {
      while ((grafter (&a, aend, mbsa)).c == (grafter (&b, bend, mbsb)).c)
        if (! ISWDIGIT ((grpeek (&a, aend, mbsa)).c))
          return 0;
      if (ISWDIGIT ((grpeek (&a, aend, mbsa)).c) && ISWDIGIT ((grpeek (&b, bend, mbsb)).c))
        return (grpeek (&a, aend, mbsa)).c - (grpeek (&b, bend, mbsb)).c;
      if (ISWDIGIT ((grpeek (&a, aend, mbsa)).c))
        goto a_trailing_nonzero;
      if (ISWDIGIT ((grpeek (&b, bend, mbsb)).c))
        goto b_trailing_nonzero;
      return 0;
    }
  else if ((grnext (&a, aend, mbsa)).c == decimal_point)
    {
    a_trailing_nonzero:
      while ((grpeek (&a, aend, mbsa)).c == WNUMERIC_ZERO)
        grnext (&a, aend, mbsa);
      return ISWDIGIT ((grpeek (&a, aend, mbsa)).c);
    }
  else if ((grnext (&b, bend, mbsb)).c == decimal_point)
    {
    b_trailing_nonzero:
      while ((grpeek (&b, bend, mbsb)).c == WNUMERIC_ZERO)
        grnext (&b, bend, mbsb);
      return - ISWDIGIT ((grpeek (&b, bend, mbsb)).c);
    }
  return 0;
}

static inline int _GL_ATTRIBUTE_PURE
wnumcompare (char const *a, char const *b,
             wint_t decimal_point, wint_t thousands_sep)
{
  int tmp;
  size_t log_a;
  size_t log_b;

  grapheme tmpa, tmpb;
  const char *aend = a + strlen(a), *bend = b + strlen(b);
  mbstate_t mbsa = { 0 }, mbsb = { 0 };

  tmpa = grpeek (&a, aend, &mbsa);
  tmpb = grpeek (&b, bend, &mbsb);

  if (tmpa.c == WNEGATION_SIGN)
    {
      do
        tmpa = grafter (&a, aend, &mbsa);
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF));
      if (tmpb.c != WNEGATION_SIGN)
        {
          if (tmpa.c == decimal_point)
            do
              tmpa = grafter (&a, aend, &mbsa);
            while (tmpa.c == WNUMERIC_ZERO);
          if (ISWDIGIT (tmpa.c))
            return -1;
          while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF))
            tmpb = grafter (&b, bend, &mbsb);
          if (tmpb.c == decimal_point)
            do
              tmpb = grafter(&b, bend, &mbsb);
            while (tmpb.c == WNUMERIC_ZERO);
          return - ISWDIGIT (tmpb.c);
        }
      do
        tmpb = grafter (&b, bend, &mbsb);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF));

      while (tmpa.c == tmpb.c && ISWDIGIT (tmpa.c))
        {
          do
            tmpa = grafter (&a, aend, &mbsa);
          while (tmpa.c == thousands_sep && thousands_sep != WEOF);
          do
            tmpb = grafter (&b, bend, &mbsb);
          while (tmpb.c == thousands_sep && thousands_sep != WEOF);
        }

      if ((tmpa.c == decimal_point && !ISWDIGIT (tmpb.c))
          || (tmpb.c == decimal_point && !ISWDIGIT (tmpa.c)))
        return wfraccompare (b, a, decimal_point, &mbsb, &mbsa);

      tmp = tmpb.c - tmpa.c;

      for (log_a = 0; ISWDIGIT (tmpa.c); ++log_a)
        do
          tmpa = grafter (&a, aend, &mbsa);
        while (tmpa.c == thousands_sep && thousands_sep != WEOF);

      for (log_b = 0; ISWDIGIT (tmpb.c); ++log_b)
        do
          tmpb = grafter (&b, bend, &mbsb);
        while (tmpb.c == thousands_sep && thousands_sep != WEOF);

      if (log_a != log_b)
        return log_a < log_b ? 1 : -1;

      if (!log_a)
        return 0;

      return tmp;
    }
  else if (tmpb.c == WNEGATION_SIGN)
    {
      do
        tmpb = grafter (&b, bend, &mbsb);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF));
      if (tmpb.c == decimal_point)
        do
          tmpb = grafter (&b, bend, &mbsb);
        while (tmpb.c == WNUMERIC_ZERO);
      if (ISWDIGIT (tmpb.c))
        return 1;
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF))
        tmpa = grafter (&a, aend, &mbsa);
      if (tmpa.c == decimal_point)
        do
          tmpa = grafter (&a, aend, &mbsa);
        while (tmpa.c == WNUMERIC_ZERO);
      return ISWDIGIT (tmpa.c);
    }
  else
    {
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF))
        tmpa = grafter (&a, aend, &mbsa);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF))
        tmpb = grafter (&b, bend, &mbsb);

      while (tmpa.c == tmpb.c && ISWDIGIT (tmpa.c))
        {
          do
            tmpa = grafter (&a, aend, &mbsa);
          while (tmpa.c == thousands_sep && thousands_sep != WEOF);
          do
            tmpb = grafter (&b, bend, &mbsb);
          while (tmpb.c == thousands_sep && thousands_sep != WEOF);
        }

      if ((tmpa.c == decimal_point && !ISWDIGIT (tmpb.c))
          || (tmpb.c == decimal_point && !ISWDIGIT (tmpa.c)))
        return wfraccompare (a, b, decimal_point, &mbsa, &mbsb);

      tmp = tmpa.c - tmpb.c;

      for (log_a = 0; ISWDIGIT (tmpa.c); ++log_a)
        do
          tmpa = grafter (&a, aend, &mbsa);
        while (tmpa.c == thousands_sep && thousands_sep != WEOF);

      for (log_b = 0; ISWDIGIT (tmpb.c); ++log_b)
        do
          tmpb = grafter (&b, bend, &mbsb);
        while (tmpb.c == thousands_sep && thousands_sep != WEOF);

      if (log_a != log_b)
        return log_a < log_b ? -1 : 1;

      if (!log_a)
        return 0;

      return tmp;
    }
}


/**** Wide version of strnumcmp.c */

int _GL_ATTRIBUTE_PURE
wstrnumcmp (char const *a, char const *b,
            wint_t decimal_point, wint_t thousands_sep)
{
  return wnumcompare (a, b, decimal_point, thousands_sep);
}


static int charwidth_cache[UCHAR_MAX] = { 0 };

int charwidth (wchar_t c)
{
  if (c >= 0 && c < UCHAR_MAX)
    {
      if (charwidth_cache[c] != 0)
        return charwidth_cache[c] - 1;

      int wid;
      if (iswprint (c))
        wid = wcwidth (c);
      else if (iswcntrl (c))
        wid = 0;
      else
        wid = 1;

      charwidth_cache[c] = wid + 1;
      return wid;
    }

  if (iswprint (c))
    return wcwidth (c);
  else if (iswcntrl (c))
    return 0;
  else
    return 1; // unknown, so probably from the future of Unicode
}
