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

/**** Binary-tolerant I/O */

cb
fgetcb(FILE *f, mbstate_t *mbs)
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

  // No bytes were read, so this is EOF

  if (i == 0)
    {
      cb ret;

      ret.c = WEOF;
      ret.isbyte = false;
      return ret;
    }

  wchar_t c;
  mbstate_t mbs_copy = *mbs;
  size_t n = mbrtowc(&c, tmp, i, &mbs_copy);

  if (n == 0)
    {
      // NUL wide character. There is no guarantee about how many
      // bytes from the source text it took to produce this NUL,
      // so try again with more and more bytes until it works.

      size_t j;
      for (j = 1; j <= i; j++)
        {
          mbs_copy = *mbs;
          if (mbrtowc(&c, tmp, j, &mbs_copy) == 0)
            break;
        }

      // If j > i, then the decoding isn't reproducible and something
      // is wrong. But still keep inside the array bounds;
      if (j > i)
        j = i;

      // Put the unconsumed bytes back for the next read.
      for (size_t k = i; k > j; k--)
        {
          ungetc(tmp[k - 1], f);
        }

      *mbs = mbs_copy;

      cb ret;
      ret.c = L'\0';
      ret.isbyte = false;
      return ret;
    }
  else if (n == (size_t) -1 || n == (size_t) -2)
    {
      // Decoding error. -2 (incomplete character) shouldn't be possible
      // unless the file was truncated. Return the first byte as a byte.
      // Leave the decoding state however it was, since nothing was
      // decoded.

      for (size_t k = i; k > 1; k--)
        ungetc(tmp[k - 1], f);

      cb ret;
      ret.c = (unsigned char) tmp[0];
      ret.isbyte = true;
      return ret;
    }
  else
    {
      // Legitimate wide character. Put as many bytes as were not used back
      // into the stream, and return the character.

      for (size_t k = i; k > n; k--)
        ungetc(tmp[k - 1], f);

      *mbs = mbs_copy;

      cb ret;
      ret.c = c;
      ret.isbyte = false;
      return ret;
    }
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

cb
ungetcb(cb c, FILE *f, mbstate_t *mbs)
{
  // TODO: If the decoder is stateful, what will get it into the
  // proper states for this character and the following?

  if (c.isbyte)
    {
      if (ungetc(c.c, f) == EOF)
        {
          c.isbyte = false;
          c.c = WEOF;
        }
    }
  else
    {
      c.c = ungetwc(c.c, f);
    }
  return c;
}

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


/**** Character iterators */

mb_error
mbrnext0(wchar_t *c, const char **s, const char *end, mbstate_t *state)
{
  if (s == NULL)
    {
      *c = L'\0';
      return MB_ERROR;
    }
  if (*s >= end)
    {
      *c = L'\0';
      return MB_EOF;
    }

  mbstate_t st2 = *state;
  size_t ret = mbrtowc(c, *s, end - *s, &st2);
  if (ret == 0)
    {
      // Success, but NUL

      if (**s != '\0')
        {
          // The next byte is not NUL. Is this possible in a legitimate
          // multibyte charset? In any case, if it happens, return an
          // error instead, since we don't know how to advance over it.

          return MB_ERROR;
        }

      // Advance text pointer and state
      (*s)++;
      *c = L'\0';
      *state = st2;
      return MB_OK;
    }
  else if (ret == (size_t) -1)
    {
      // Error
      // Text pointer and state do not advance
      *c = L'\0';
      return MB_ERROR;
    }
  else if (ret == (size_t) -2)
    {
      // Incomplete multibyte character
      // Text pointer and state do not advance
      *c = L'\0';
      return MB_INCOMPLETE;
    }
  else
    {
      (*s) += ret;
      *state = st2;
      return MB_OK;
    }
}

mb_error
mbrpeek0(wchar_t *c, const char **s, const char *end, mbstate_t *state)
{
  const char *ostring = *s;
  mbstate_t ostate = *state;

  return mbrnext0(c, &ostring, end, &ostate);
}

mb_error
mbrafter0(wchar_t *c, const char **s, const char *end, mbstate_t *state)
{
  // TODO: callers are relying on seeing L'\0' when calling with *s == end
  // Is this a good thing to guarantee?

  mbrnext0(c, s, end, state);
  return mbrpeek0(c, s, end, state);
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
  wchar_t ca, cb;
  const char *aend = a + strlen(a);
  const char *bend = b + strlen(b);

  mbrpeek0(&ca, &a, aend, mbsa);
  mbrpeek0(&cb, &b, bend, mbsb);

  if (ca == decimal_point && cb == decimal_point)
    {
      while (1)
        {
          mbrnext0(&ca, &a, aend, mbsa);
          mbrnext0(&cb, &b, bend, mbsb);

          if (ca != cb)
            break;

          mbrpeek0(&ca, &a, aend, mbsa);
          mbrpeek0(&cb, &b, bend, mbsb);

          if (!ISWDIGIT(ca))
            return 0;

          if (ISWDIGIT (ca) && ISWDIGIT (ca))
            return ca - cb;

          if (ISWDIGIT (ca))
            goto a_trailing_nonzero;
          if (ISWDIGIT (cb))
            goto b_trailing_nonzero;

          return 0;
        }
    }
  else if (ca == decimal_point)
    {
      mbrnext0(&ca, &a, aend, mbsa);
    a_trailing_nonzero:
      for (; mbrpeek0(&ca, &a, aend, mbsa) == MB_OK; mbrnext0(&ca, &a, aend, mbsa))
        {
          if (ca != WNUMERIC_ZERO)
            break;
        }

      mbrnext0(&ca, &a, aend, mbsa);
      return ISWDIGIT (ca);
    }
  else if (*b++ == decimal_point)
    {
      mbrnext0(&cb, &b, bend, mbsb);
    b_trailing_nonzero:
      for (; mbrpeek0(&cb, &b, bend, mbsb) == MB_OK; mbrnext0(&cb, &b, bend, mbsb))
        {
          if (cb != WNUMERIC_ZERO)
            break;
        }

      mbrnext0(&cb, &b, bend, mbsb);
      return ISWDIGIT (cb);
    }
  return 0;
}

static inline int _GL_ATTRIBUTE_PURE
wnumcompare (char const *a, char const *b,
            wint_t decimal_point, wint_t thousands_sep)
{
  wint_t tmp;
  size_t log_a;
  size_t log_b;

  wchar_t tmpa, tmpb;
  const char *aend = a + strlen(a), *bend = b + strlen(b);
  mbstate_t mbsa = { 0 }, mbsb = { 0 };

  mbrpeek0(&tmpa, &a, aend, &mbsa);
  mbrpeek0(&tmpb, &b, bend, &mbsb);

  if (tmpa == WNEGATION_SIGN)
    {
      do
        mbrafter0(&tmpa, &a, aend, &mbsa);
      while (tmpa == WNUMERIC_ZERO || tmpa == thousands_sep);
      if (tmpb != WNEGATION_SIGN)
        {
          if (tmpa == decimal_point)
            do
              mbrafter0(&tmpa, &a, aend, &mbsa);
            while (tmpa == WNUMERIC_ZERO);
          if (ISWDIGIT (tmpa))
            return -1;
          while (tmpb == WNUMERIC_ZERO || tmpb == thousands_sep)
            mbrafter0(&tmpb, &b, bend, &mbsb);
          if (tmpb == decimal_point)
            do
              mbrafter0(&tmpb, &b, bend, &mbsb);
            while (tmpb == WNUMERIC_ZERO);
          return - ISWDIGIT (tmpb);
        }
      do
        mbrafter0(&tmpb, &b, bend, &mbsb);
      while (tmpb == WNUMERIC_ZERO || tmpb == thousands_sep);

      while (tmpa == tmpb && ISWDIGIT (tmpa))
        {
          do
            mbrafter0(&tmpa, &a, aend, &mbsa);
          while (tmpa == thousands_sep);
          do
            mbrafter0(&tmpb, &b, bend, &mbsb);
          while (tmpb == thousands_sep);
        }

      if ((tmpa == decimal_point && !ISWDIGIT (tmpb))
          || (tmpb == decimal_point && !ISWDIGIT (tmpa)))
        return wfraccompare (b, a, decimal_point, &mbsb, &mbsa);

      tmp = tmpb - tmpa;

      for (log_a = 0; ISWDIGIT (tmpa); ++log_a)
        do
          mbrafter0(&tmpa, &a, aend, &mbsa);
        while (tmpa == thousands_sep);

      for (log_b = 0; ISWDIGIT (tmpb); ++log_b)
        do
          mbrafter0(&tmpb, &b, bend, &mbsb);
        while (tmpb == thousands_sep);

      if (log_a != log_b)
        return log_a < log_b ? 1 : -1;

      if (!log_a)
        return 0;

      return tmp;
    }
  else if (tmpb == WNEGATION_SIGN)
    {
      do
        mbrafter0(&tmpb, &b, bend, &mbsb);
      while (tmpb == WNUMERIC_ZERO || tmpb == thousands_sep);
      if (tmpb == decimal_point)
        do
          mbrafter0(&tmpb, &b, bend, &mbsb);
        while (tmpb == WNUMERIC_ZERO);
      if (ISWDIGIT (tmpb))
        return 1;
      while (tmpa == WNUMERIC_ZERO || tmpa == thousands_sep)
        mbrafter0(&tmpa, &a, aend, &mbsa);
      if (tmpa == decimal_point)
        do
          mbrafter0(&tmpa, &a, aend, &mbsa);
        while (tmpa == WNUMERIC_ZERO);
      return ISWDIGIT (tmpa);
    }
  else
    {
      while (tmpa == WNUMERIC_ZERO || tmpa == thousands_sep)
        mbrafter0(&tmpa, &a, aend, &mbsa);
      while (tmpb == WNUMERIC_ZERO || tmpb == thousands_sep)
        mbrafter0(&tmpb, &b, bend, &mbsb);

      while (tmpa == tmpb && ISWDIGIT (tmpa))
        {
          do
            mbrafter0(&tmpa, &a, aend, &mbsa);
          while (tmpa == thousands_sep);
          do
            mbrafter0(&tmpb, &b, bend, &mbsb);
          while (tmpb == thousands_sep);
        }

      if ((tmpa == decimal_point && !ISWDIGIT (tmpb))
          || (tmpb == decimal_point && !ISWDIGIT (tmpa)))
        return wfraccompare (a, b, decimal_point, &mbsa, &mbsb);

      tmp = tmpa - tmpb;

      for (log_a = 0; ISWDIGIT (tmpa); ++log_a)
        do
          mbrafter0(&tmpa, &a, aend, &mbsa);
        while (tmpa == thousands_sep);

      for (log_b = 0; ISWDIGIT (tmpb); ++log_b)
        do
          mbrafter0(&tmpb, &b, bend, &mbsb);
        while (tmpb == thousands_sep);

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


