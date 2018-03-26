/* grapheme - encoding-error-tolerant character I/O

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

/* Written by Eric Fischer.  */

#include <config.h>

#include "system.h"
#include "xalloc.h"
#include "grapheme.h"
#include "exitfail.h"

grapheme
grnext (const char **s, const char *end, mbstate_t *mbs)
{
  // No bytes remained, so this is EOF

  if (*s == NULL || *s >= end)
    return grapheme_wchar (WEOF);

  wchar_t c;
  mbstate_t mbs_copy = *mbs;
  size_t n = mbrtowc (&c, *s, end - *s, &mbs_copy);

  if (n == 0)
    {
      // NUL wide character. There is no information about how many
      // bytes from the source text it took to produce this NUL,
      // so try again with more and more bytes until it works.

      size_t j;
      for (j = 1; j <= end - *s; j++)
        {
          mbs_copy = *mbs;
          if (mbrtowc (&c, *s, end - *s, &mbs_copy) == 0)
            break;
        }

      // If j > end - *s, then the decoding isn't reproducible and something
      // is wrong. But still keep inside the array bounds;
      if (j > end - *s)
        j = end - *s;

      *s += j;
      *mbs = mbs_copy;

      return grapheme_wchar (L'\0');
    }
  else if (n == (size_t) -1 || n == (size_t) -2)
    {
      // Decoding error. -2 (incomplete character) shouldn't be possible
      // unless the file was truncated. Return the first byte as a byte.
      // Leave the decoding state however it was, since nothing was
      // decoded.

      grapheme ret = grapheme_byte (**s);

      (*s)++;
      return ret;
    }
  else
    {
      // Legitimate wide character.  Put as many bytes as were not used back
      // into the stream, and return the character.

      (*s) += n;
      *mbs = mbs_copy;

      return grapheme_wchar (c);
    }
}

grapheme
grpeek (const char **s, const char *end, mbstate_t *mbs)
{
  const char *tmps = *s;
  mbstate_t tmpmbs = *mbs;
  return grnext (&tmps, end, &tmpmbs);
}

grapheme
grafter (const char **s, const char *end, mbstate_t *state)
{
  grnext (s, end, state);
  return grpeek (s, end, state);
}

/**** Binary-tolerant wide character I/O */

static grapheme
fgetgr_internal (FILE *f, mbstate_t *mbs, bool peek, size_t *count)
{
  char tmp[MB_CUR_MAX];
  mbstate_t copy;

  // Special case for the common case of a valid character
  // from just one byte.

  int b = getc (f);
  if (b == EOF)
    {
      *count = 0;
      return grapheme_wchar (WEOF);
    }

  tmp[0] = b;
  copy = *mbs;
  wchar_t ch;
  if (mbrtowc (&ch, tmp, 1, &copy) == 1)
    {
      if (peek)
        ungetc ((unsigned char) b, f);

      *count = 1;
      return grapheme_wchar (ch);
    }

  // May need to read as many as MB_CUR_MAX bytes ahead to get
  // one complete multibyte character.

  size_t i;
  for (i = 1; i < MB_CUR_MAX; i++)
    {
      int c = getc (f);
      if (c == EOF)
        break;
      tmp[i] = c;
    }

  const char *s = tmp;
  grapheme c;

  copy = *mbs;
  c = grnext (&s, s + i, &copy);
  *count = s - tmp;

  if (peek)
    {
      // Put everything back
      for (size_t k = i; k > 0; k--)
        ungetc ((unsigned char) tmp[k - 1], f);
    }
  else
    {
      // Put unused bytes back
      for (size_t k = i; k > s - tmp; k--)
        ungetc ((unsigned char) tmp[k - 1], f);

      *mbs = copy;
    }

  return c;
}

grapheme
fgetgr (FILE *f, mbstate_t *mbs)
{
  size_t count;
  return fgetgr_internal (f, mbs, false, &count);
}

grapheme
fgetgr_count (FILE *f, mbstate_t *mbs, size_t *count)
{
  return fgetgr_internal (f, mbs, false, count);
}

grapheme
fpeekgr (FILE *f, mbstate_t *mbs)
{
  size_t count;
  return fgetgr_internal (f, mbs, true, &count);
}

grapheme
fputgr (grapheme c, FILE *f)
{
  if (c.isbyte)
    {
      int ret = putc ((unsigned char) c.c, f);
      if (ret == EOF)
        return grapheme_wchar (WEOF);

      return c;
    }
  else
    {
      // TODO: Deal with different encoding states

      char tmp[MB_CUR_MAX];
      int n = wctomb (tmp, c.c);

      if (n < 0)
        {
          if (c.c <= UCHAR_MAX)
            {
              // This must be a byte in the C locale,
              // where 0x80-0xFF are sometimes not
              // regarded as characters.

              int ret = putc (c.c, f);
              if (ret == EOF)
                c.c = WEOF;
              return c;
            }
          else
            {
              c.c = WEOF;
              return c;
            }
        }

      for (size_t i = 0; i < n; i++)
        {
          int ret = putc (tmp[i], f);
          if (ret == EOF)
            {
              c.c = WEOF;
              return c;
            }
        }

      return c;
    }
}

wchar_t fputwcgr (wchar_t c, FILE *f)
{
  return fputgr (grapheme_wchar (c), f).c;
}

grapheme
putgrapheme (grapheme c)
{
  return fputgr (c, stdout);
}

grapheme
getgrapheme (mbstate_t *mbs)
{
  return fgetgr (stdin, mbs);
}


/**** Grapheme/wide version of memchr */

grapheme * _GL_ATTRIBUTE_PURE
grmemchr (grapheme *haystack, wchar_t needle, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    if (haystack[i].c == needle) {
      return haystack + i;
    }
  }
  return NULL;
}

size_t _GL_ATTRIBUTE_PURE
grslen (const grapheme *s)
{
  size_t i = 0;
  while (s[i].c != L'\0')
    i++;
  return i;
}

grapheme *
grsdup (const grapheme *s)
{
  size_t n = grslen (s);
  grapheme *out = xmalloc ((n + 1) * sizeof (grapheme));
  memcpy (out, s, (n + 1) * sizeof (grapheme));
  return out;
}

grapheme
grapheme_wchar (wchar_t c)
{
  grapheme g;
  g.c = c;
  g.isbyte = false;
  return g;
}

grapheme
grapheme_byte (unsigned char c)
{
  grapheme g;
  g.c = c;
  g.isbyte = true;
  return g;
}

void
mbstogrs (grapheme *out, const char *in)
{
  const char *end = in + strlen (in);
  mbstate_t mbs = { 0 };
  grapheme g;
  size_t n = 0;

  while ((g = grnext (&in, end, &mbs)).c != WEOF)
    out[n++] = g;

  out[n] = grapheme_wchar (L'\0');
}
