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
#include "multibyte.h"
#include "error.h"
#include "exitfail.h"
#include "quotearg.h"
#include "quote.h"

/* Use this to suppress gcc's "...may be used before initialized" warnings. */
#if defined GCC_LINT || defined lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code) /* empty */
#endif

cb
cbnext (const char **s, const char *end, mbstate_t *mbs)
{
  // No bytes remained, so this is EOF

  if (*s == NULL || *s >= end)
    {
      cb ret;

      ret.c = WEOF;
      ret.isbyte = false;
      return ret;
    }

  wchar_t c;
  mbstate_t mbs_copy = *mbs;
  size_t n = mbrtowc(&c, *s, end - *s, &mbs_copy);

  if (n == 0)
    {
      // NUL wide character. There is no information about how many
      // bytes from the source text it took to produce this NUL,
      // so try again with more and more bytes until it works.

      size_t j;
      for (j = 1; j <= end - *s; j++)
        {
          mbs_copy = *mbs;
          if (mbrtowc(&c, *s, end - *s, &mbs_copy) == 0)
            break;
        }

      // If j > end - *s, then the decoding isn't reproducible and something
      // is wrong. But still keep inside the array bounds;
      if (j > end - *s)
        j = end - *s;

      cb ret;
      ret.c = L'\0';
      ret.isbyte = false;

      *s += j;
      *mbs = mbs_copy;

      return ret;
    }
  else if (n == (size_t) -1 || n == (size_t) -2)
    {
      // Decoding error. -2 (incomplete character) shouldn't be possible
      // unless the file was truncated. Return the first byte as a byte.
      // Leave the decoding state however it was, since nothing was
      // decoded.

      cb ret;
      ret.c = (unsigned char) **s;
      ret.isbyte = true;

      (*s)++;

      return ret;
    }
  else
    {
      // Legitimate wide character.  Put as many bytes as were not used back
      // into the stream, and return the character.

      cb ret;
      ret.c = c;
      ret.isbyte = false;

      (*s) += n;
      *mbs = mbs_copy;

      return ret;
    }
}

cb
cbpeek (const char **s, const char *end, mbstate_t *mbs)
{
  const char *tmps = *s;
  mbstate_t tmpmbs = *mbs;
  return cbnext(&tmps, end, &tmpmbs);
}

cb
cbafter(const char **s, const char *end, mbstate_t *state)
{
  cbnext(s, end, state);
  return cbpeek(s, end, state);
}

/**** Binary-tolerant I/O */

static cb
fgetcb_internal(FILE *f, mbstate_t *mbs, bool peek)
{
  char tmp[MB_CUR_MAX];

  // May need to read as many as MB_CUR_MAX bytes ahead to get
  // one complete multibyte character.

  size_t i;
  for (i = 0; i < MB_CUR_MAX; i++)
    {
      int c = getc(f);
      if (c == EOF)
        break;
      tmp[i] = c;
    }

  mbstate_t copy = *mbs;
  const char *s = tmp;

  cb c = cbnext(&s, s + i, &copy);

  if (peek)
    {
      // Put everything back
      for (size_t k = i; k > 0; k--)
        ungetc((unsigned char) tmp[k - 1], f);
    }
  else
    {
      // Put unused bytes back
      for (size_t k = i; k > s - tmp; k--)
        ungetc((unsigned char) tmp[k - 1], f);

      *mbs = copy;
    }

  return c;
}

cb
fgetcb(FILE *f, mbstate_t *mbs)
{
  return fgetcb_internal(f, mbs, false);
}

cb
fpeekcb(FILE *f, mbstate_t *mbs)
{
  return fgetcb_internal(f, mbs, true);
}

cb
fputcb(cb c, FILE *f)
{
  if (c.isbyte)
    {
      int ret = putc(c.c, f);
      if (ret == EOF)
        {
          c.c = WEOF;
          c.isbyte = false;
        }

      return c;
    }
  else
    {
      // TODO: Can we safely write wide characters and bytes
      // to the same stream?

      c.c = putwc(c.c, f);
      return c;
    }
}

cb
putcbyte(cb c)
{
  return fputcb(c, stdout);
}

cb
getcbyte(mbstate_t *mbs)
{
  return fgetcb(stdin, mbs);
}

/**** Wide version of linebuffer.c */

/* Initialize cblinebuffer LINEBUFFER for use. */

void
initcbbuffer (struct cblinebuffer *linebuffer)
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

struct cblinebuffer *
readcblinebuffer_delim (struct cblinebuffer *linebuffer, FILE *stream,
                       wchar_t delimiter, mbstate_t *mbs)
{
  cb c;
  cb *buffer = linebuffer->buffer;
  cb *p = linebuffer->buffer;
  cb *end = buffer + linebuffer->size; /* Sentinel. */

  if (feof (stream))
    return NULL;

  do
    {
      c = fgetcb (stream, mbs);
      if (c.c == WEOF)
        {
          if (p == buffer || ferror (stream))
            return NULL;
          if (p[-1].c == delimiter)
            break;
          c.c = delimiter;
          c.isbyte = false;
        }
      if (p == end)
        {
          size_t oldsize = linebuffer->size;
          buffer = x2nrealloc (buffer, &linebuffer->size, sizeof(cb));
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

int
xcbmemcoll (cb *s1, size_t s1len, cb *s2, size_t s2len)
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
  return quote(tmp);
}

const char *
cbnstr (const cb *s, size_t n)
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
          int n = wctomb(tmp + out, s[i].c);
          if (n > 0)
            out += n;
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
cbgetndelim2 (cb **lineptr, size_t *linesize, size_t offset, size_t nmax,
            wchar_t delim1, wchar_t delim2, FILE *stream, mbstate_t *mbs)
{
  size_t nbytes_avail;          /* Allocated but unused bytes in *LINEPTR.  */
  cb *read_pos;               /* Where we're reading into *LINEPTR. */
  ssize_t bytes_stored = -1;
  cb *ptr = *lineptr;
  size_t size = *linesize;
  bool found_delimiter;

  if (!ptr)
    {
      size = nmax < MIN_CHUNK ? nmax : MIN_CHUNK;
      ptr = malloc (size * sizeof(cb));
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

      cb c;
      c.c = L'\0';
      c.isbyte = false;

      const cb *buffer;
      size_t buffer_len;

      buffer = NULL;
      if (true)
        {
          c = fgetcb (stream, mbs);
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
          cb *newptr;

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
          newptr = realloc (ptr, newsize * sizeof(cb));
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
  cb c;
  c.c = L'\0';
  c.isbyte = false;
  *read_pos = c;

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
  cb ca, cb;
  const char *aend = a + strlen(a);
  const char *bend = b + strlen(b);

  ca = cbpeek (&a, aend, mbsa);
  cb = cbpeek (&b, bend, mbsb);

  if (ca.c == decimal_point && cb.c == decimal_point)
    {
      while (1)
        {
          ca = cbnext (&a, aend, mbsa);
          cb = cbnext (&b, bend, mbsb);

          if (ca.c != cb.c)
            break;

          ca = cbpeek (&a, aend, mbsa);
          cb = cbpeek (&b, bend, mbsb);

          if (!ISWDIGIT (ca.c))
            return 0;

          if (ISWDIGIT (ca.c) && ISWDIGIT (ca.c))
            return ca.c - cb.c;

          if (ISWDIGIT (ca.c))
            goto a_trailing_nonzero;
          if (ISWDIGIT (cb.c))
            goto b_trailing_nonzero;

          return 0;
        }
    }
  else if (ca.c == decimal_point)
    {
      ca = cbnext (&a, aend, mbsa);
    a_trailing_nonzero:
      for (; (ca = cbpeek (&a, aend, mbsa)).c != WEOF; ca = cbnext (&a, aend, mbsa))
        {
          if (ca.c != WNUMERIC_ZERO)
            break;
        }

      ca = cbnext (&a, aend, mbsa);
      return ISWDIGIT (ca.c);
    }
  else if (cb.c == decimal_point)
    {
      cb = cbnext (&b, bend, mbsb);
    b_trailing_nonzero:
      for (; (cb = cbpeek (&b, bend, mbsb)).c != WEOF; cb = cbnext (&b, bend, mbsb))
        {
          if (cb.c != WNUMERIC_ZERO)
            break;
        }

      cb = cbnext (&b, bend, mbsb);
      return ISWDIGIT (cb.c);
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

  cb tmpa, tmpb;
  const char *aend = a + strlen(a), *bend = b + strlen(b);
  mbstate_t mbsa = { 0 }, mbsb = { 0 };

  tmpa = cbpeek (&a, aend, &mbsa);
  tmpb = cbpeek (&b, bend, &mbsb);

  if (tmpa.c == WNEGATION_SIGN)
    {
      do
        tmpa = cbafter (&a, aend, &mbsa);
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF));
      if (tmpb.c != WNEGATION_SIGN)
        {
          if (tmpa.c == decimal_point)
            do
              tmpa = cbafter (&a, aend, &mbsa);
            while (tmpa.c == WNUMERIC_ZERO);
          if (ISWDIGIT (tmpa.c))
            return -1;
          while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF))
            tmpb = cbafter (&b, bend, &mbsb);
          if (tmpb.c == decimal_point)
            do
              tmpb = cbafter(&b, bend, &mbsb);
            while (tmpb.c == WNUMERIC_ZERO);
          return - ISWDIGIT (tmpb.c);
        }
      do
        tmpb = cbafter (&b, bend, &mbsb);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF));

      while (tmpa.c == tmpb.c && ISWDIGIT (tmpa.c))
        {
          do
            tmpa = cbafter (&a, aend, &mbsa);
          while (tmpa.c == thousands_sep && thousands_sep != WEOF);
          do
            tmpb = cbafter (&b, bend, &mbsb);
          while (tmpb.c == thousands_sep && thousands_sep != WEOF);
        }

      if ((tmpa.c == decimal_point && !ISWDIGIT (tmpb.c))
          || (tmpb.c == decimal_point && !ISWDIGIT (tmpa.c)))
        return wfraccompare (b, a, decimal_point, &mbsb, &mbsa);

      tmp = tmpb.c - tmpa.c;

      for (log_a = 0; ISWDIGIT (tmpa.c); ++log_a)
        do
          tmpa = cbafter (&a, aend, &mbsa);
        while (tmpa.c == thousands_sep && thousands_sep != WEOF);

      for (log_b = 0; ISWDIGIT (tmpb.c); ++log_b)
        do
          tmpb = cbafter (&b, bend, &mbsb);
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
        tmpb = cbafter (&b, bend, &mbsb);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF));
      if (tmpb.c == decimal_point)
        do
          tmpb = cbafter (&b, bend, &mbsb);
        while (tmpb.c == WNUMERIC_ZERO);
      if (ISWDIGIT (tmpb.c))
        return 1;
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF))
        tmpa = cbafter (&a, aend, &mbsa);
      if (tmpa.c == decimal_point)
        do
          tmpa = cbafter (&a, aend, &mbsa);
        while (tmpa.c == WNUMERIC_ZERO);
      return ISWDIGIT (tmpa.c);
    }
  else
    {
      while (tmpa.c == WNUMERIC_ZERO || (tmpa.c == thousands_sep && thousands_sep != WEOF))
        tmpa = cbafter (&a, aend, &mbsa);
      while (tmpb.c == WNUMERIC_ZERO || (tmpb.c == thousands_sep && thousands_sep != WEOF))
        tmpb = cbafter (&b, bend, &mbsb);

      while (tmpa.c == tmpb.c && ISWDIGIT (tmpa.c))
        {
          do
            tmpa = cbafter (&a, aend, &mbsa);
          while (tmpa.c == thousands_sep && thousands_sep != WEOF);
          do
            tmpb = cbafter (&b, bend, &mbsb);
          while (tmpb.c == thousands_sep && thousands_sep != WEOF);
        }

      if ((tmpa.c == decimal_point && !ISWDIGIT (tmpb.c))
          || (tmpb.c == decimal_point && !ISWDIGIT (tmpa.c)))
        return wfraccompare (a, b, decimal_point, &mbsa, &mbsb);

      tmp = tmpa.c - tmpb.c;

      for (log_a = 0; ISWDIGIT (tmpa.c); ++log_a)
        do
          tmpa = cbafter (&a, aend, &mbsa);
        while (tmpa.c == thousands_sep && thousands_sep != WEOF);

      for (log_b = 0; ISWDIGIT (tmpb.c); ++log_b)
        do
          tmpb = cbafter (&b, bend, &mbsb);
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


/**** CB/Wide version of memchr */

cb *
cbmemchr(cb *haystack, wchar_t needle, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    if (haystack[i].c == needle) {
      return haystack + i;
    }
  }
  return NULL;
}

int charwidth (wchar_t c)
{
  if (iswprint (c))
    return wcwidth (c);
  else if (iscntrl (c))
    return 0;
  else
    return 1; // unknown, so probably from the future of Unicode
}
