/* system-dependent multibyte-related definitions for coreutils
   Copyright (C) 2016 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/* Written by Assaf Gordon.  */

#include <config.h>

#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "multibyte.h"

/* Copied from <coreutils>/src/system.h */
#ifndef STREQ
#define STREQ(a, b) (strcmp (a, b) == 0)
#endif

struct utf8_ucs4
{
  const char* utf8;    /* input as UTF-8 multibyte string */
  const uint32_t ucs4; /* expected output as UCS-4 codepoint */
};

static const struct utf8_ucs4 utf8_ucs4_tests[] =
  {
    /* ASCII */
    { "r",            0x0072 }, /* Latin Small Letter R (U+0072) */

    /* BMP */
    { "\xCE\xB1",     0x03B1 }, /* GREEK SMALL LETTER ALPHA (U+03B1) */
    { "\xEA\x9D\xA4", 0xA764 }, /* THORN WITH STROKE  (U+A764) */
    { "\xEF\xB9\xAA", 0xFE6A }, /* SMALL PERCENT SIGN (U+FE6A) */

    /* SMP */
    { "\xF0\x90\x8C\xBB", 0x1033B},  /* GOTHIC LETTER LAGUS (U+1033B) */
    { "\xF0\x9F\x82\xB1", 0x1F0B1},  /* PLAYING CARD ACE OF HEARTS (U+1F0B1) */

    { NULL, 0 }
  };


bool
use_multibyte (void)
{
#if HAVE_MBRTOWC
  return (MB_CUR_MAX > 1);
#else
  return false;
#endif
}


/* Surely there's a better way.... */
bool
is_utf8_locale_name (void)
{
  /* TODO: add #ifdef HAVE_MBRTOWC and also return FALSE if not available? */
  const char *l = setlocale (LC_CTYPE, NULL);
  if (!l)
    return false;

  l = strrchr (l, '.');
  if (!l)
    return false;
  ++l;

  return (STREQ (l,"UTF-8") || STREQ(l,"UTF8")
          || STREQ (l,"utf-8") || STREQ (l,"utf8"));
}

/* Return TRUE if the given multibyte string results in the EXPECTED
   value for wchar_t. */
static bool
_check_mb_wc (const char* mbstr,  const uint32_t expected,
              bool verbose)
{
  mbstate_t mbs;
  wchar_t wc;
  size_t n;
  const size_t l = strlen (mbstr);

  memset (&mbs, 0, sizeof (mbs));
  n = mbrtowc (&wc, mbstr, l, &mbs);

  if (verbose)
    {
      fputs ("mbstr( ", stdout);
      for (size_t i=0;i<l;++i)
        printf ("\\x%02x ", (unsigned char)mbstr[i]);
      fputs (") ",stdout);

      if ((n == (size_t)-1) || (n==(size_t)-2))
        {
          /* conversion failed */
          printf ("failed conversion, n=%zu (expected U+%04x)\n", n, expected);
        }
      else
        {
          printf ("= wchar_t ( 0x%04x ) ", (uint32_t)wc);
          if ( (l==n) && ((uint32_t)wc == expected) )
            puts ("- as expected");
          else
            printf (" - mismatch, n=%zu (expected U+%04x)\n", n, expected);
        }
    }

  return (l==n) && ( ((uint32_t)wc) == expected);
}

static bool
_check_utf8_ucs (bool verbose, bool check_ucs4)
{
  bool ok = true;
  const struct utf8_ucs4 *p = utf8_ucs4_tests;

  const size_t min_size = check_ucs4 ? 4 : 2 ;
  if (sizeof(wchar_t)<min_size)
    return false;

  while (p && p->utf8)
    {
      if (check_ucs4 || (p->ucs4 <= 0xFFFF))
          ok &= _check_mb_wc (p->utf8, p->ucs4, verbose);

      ++p;
    }

  return ok;
}


bool
is_utf8_wchar_ucs2 (void)
{
  return _check_utf8_ucs (false, false);
}

bool _GL_ATTRIBUTE_CONST
is_utf8_wchar_ucs2_surrogate (void)
{
  /* Test with:
       U+1D771 MATHEMATICAL SANS-SERIF BOLD SMALL BETA
       UTF-8: \xF0\x9D\x9D\xb1
       UTF-16: 0x0d835 0x0df71

     If libc uses UTF-16 surrogate pairs,
     mbrtowc should return the two UTF-16 values above.
  */
  const char* beta = "\xF0\x9D\x9D\xb1";
  mbstate_t mbs;
  wchar_t wc1,wc2;
  size_t n1,n2;

  if (sizeof (wchar_t) != 2)
    return false;

  if (!is_utf8_wchar_ucs2 ())
    return false;

  memset (&mbs, 0, sizeof(mbs));

  n1 = mbrtowc (&wc1, beta, 4, &mbs);
  if ((n1 == (size_t)-1) || (n1==(size_t)-2))
    return false;

  assert (n1<=4);
  if (n1>4)
    return false;

  n2 = mbrtowc (&wc2, beta+n1, 4-n1, &mbs);
  if ((n2 == (size_t)-1) || (n2==(size_t)-2))
    return false;

  return (wc1==0xd835) && (wc2==0xdf71);
}

/* Return TRUE if the current locale is able to parse UTF-8 input,
   AND can accept UTF-16 surrogate pair codepoints,
   e.g. "\xED\xA0\x81" (which is U+D801).

   On most systems with wchar_t==ucs4, mbrtowc(3) will treat the
   above input as 3 invalid octects.
   On cygwin (where wchar_t==uint16 and UTf-16 surrogates are supported),
   mbrtowc(3) will return wc==0xD801.
*/
extern bool _GL_ATTRIBUTE_CONST
is_utf8_surrogate_input_valid (void)
{
  const char* in = "\xED\xA0\x81";
  mbstate_t mbs;
  wchar_t wc;
  size_t n;

  memset (&mbs, 0, sizeof(mbs));

  n = mbrtowc (&wc, in, 3, &mbs);
  if ((n == (size_t)-1) || (n==(size_t)-2))
    return false;

  /* Not a multibyte locale */
  if ((MB_CUR_MAX==1) && (n==1) && ((unsigned char)wc==0xED))
    return false;

  /* If asserted, this is a strange system which
     accepted the above UTF-8 input, but parsed
     it as something other than U+D801 . */
  assert (n==3);

  if (n!=3)
    return false;

  return (wc==0xD801);
}



bool
is_utf8_wchar_ucs4 (void)
{
  return _check_utf8_ucs (false, true);
}

void
debug_utf8_ucs4 (void)
{
  _check_utf8_ucs (true, true);
}


bool _GL_ATTRIBUTE_CONST
is_supplementary_plane (ucs4_t c)
{
  return (c>=0x10000) && (c<=0x10FFFF);
}


bool _GL_ATTRIBUTE_CONST
is_utf16_surrogate (ucs4_t c)
{
  return (c>=0xD800) && (c<=0xDFFF);
}

bool _GL_ATTRIBUTE_CONST
is_utf16_surrogate_high (ucs4_t c)
{
  return (c>=0xD800) && (c<=0xDBFF);
}

bool _GL_ATTRIBUTE_CONST
is_utf16_surrogate_low (ucs4_t c)
{
  return (c>=0xDC00) && (c<=0xDFFF);
}

/* Return the unicode codepoint (UCS4) corresponding to
   the given UTF-16 surrogate pair.
   (No input validation is performed) */
ucs4_t _GL_ATTRIBUTE_CONST
utf16_surrogate_to_ucs4 (wchar_t h, wchar_t l)
{
  const ucs4_t a = (h-0xD800) << 10;
  const ucs4_t b = (l-0xDC00) ;
  return 0x10000 + a + b;
}

/* Return the UTF-16 surrogate pair from a given UCS4 character.
   No input validation is performed.
   Results are undefined if C is less than U+10000 and does not
   require two UTF-16 surrogate values, or if C is outside
   valid unicode codepoint range. */
void
ucs4_to_utf16_surrogate_pair (ucs4_t c, wchar_t /*OUT*/ *h, wchar_t /*OUT*/ *l)
{
  const ucs4_t t = (c - 0x10000) & 0xFFFFF;
  *h = (wchar_t) (0xD800 + ((t>>10) & 0x3FF));
  *l = (wchar_t) (0xDC00 + ( t      & 0x3FF));
}

size_t
mbtowc_utf16(ucs4_t /*out*/ *wc, const char *s, size_t n)
{
  wchar_t h,l;  /* high,low wchars */
  size_t nh,nl; /* num. octets for high,low wchars */
  mbstate_t mbs;

  memset (&mbs, 0, sizeof(mbs));

  /* first character (possibly the 'high' surrogate) */
  nh = mbrtowc (&h, s, n, &mbs);

  /* Invalid MB sequence, Incomplete MB sequence.
     Emulate mbtowc(3) and do not return -2. */
  if ( (n==(size_t)-1) || (n==(size_t)-2))
    return (size_t)-1;

  /* NUL character */
  if (n==0)
    {
      if (wc)
        *wc = 0;
      return nh;
    }

  /* A multibyte character but not a UTF-16 surrogate - return as-is. */
  if (!is_utf16_surrogate_low (h) && !is_utf16_surrogate_high (h))
    {
      if (wc)
        *wc = h;
      return nh;
    }

  /* A "low" (trailing) surrogate character, meaning the "high"
     wide-character is missing. */
  if (is_utf16_surrogate_low (h))
    return (size_t)-1;

  /* If reached here, this is a "high" (leading) UTF-16 surrogate character. */

  /* if all the input octets were used, this 'high' UTF-16 character is not
     followed by additional octets (i.e. no "low" surrogate widechar). */
  if (nh >= n)
    return (size_t)-1;


  /* on cygwin, if (mb->_mbst.__count != 4)
     it means the INPUT was a UTF-16 surrogate codepoint
     (i.e. not cygwin internally breaking an SMP input wide-character
     into two UTF-16 surrogate wide-characters).
     Such codepoints should never appear in valid input files.
     To maintain consistency with other systems (e.g. glibc),
     this should be rejected as invalid octet.

     TODO:
     Implement the same on other such system (AIX/32bit)
     where wchar_t==uint16.
  */
#ifdef __CYGWIN__
  if (mbs.__count != 4)
    return (size_t)-1;
#endif

  /* Parse the remaining octets, this should be the 'low'
     (trailing) UTF-16 surrogate character. */
  nl = mbrtowc (&l, s+nh, n-nh, &mbs);

  /* Not a valid multibyte character, or valid but not
     a 'low' UTF-16 surrogate code-point.

     This means the input string S contains a valid 'high'
     (leading) UTF-16 surrogate codepoint followed by an invalid
     "low" code-point (or by a character which is not a "low"
     surrogate code-point at all).

     Becaue the function rejects raw UTF-16 surrogates in the input
     (the 'mbs.__count!=4' above), if the code asserts here, it means the
     LibC itself sent us invalid pair.

     This can happen on inputs such as: "\xF0\x90\x8Cq"
     where cygwin parses the first 3 octets as valid high UTF16 surrogate,
     and on the next invocation of mbrtowc(3) cygwin encountes 'q',
     and simply returns it.
  */
  if ((nl==(size_t)-1) || (nl==(size_t)-2) || (nl==0)
      || !is_utf16_surrogate_low (l))
    return (size_t)-1;


  /* If reached here, the input S contained a multibyte-sequence
     that was parsed as two valid UTF-16 surrogate code-points.

     Combine them into one ucs4 value, emulating a libc system that
     parses the multibyte sequence into a ucs4_t value in one call.
  */
  if (wc)
    *wc = utf16_surrogate_to_ucs4 (h, l);
  return nh + nl;
}


bool
is_valid_mb_character (const char* s, uint32_t /*out*/ *wc)
{
  if (!s)
    return false;

  const size_t l = strlen(s);
  if (l==0)
    return false;

  /* In singlebyte locales, every octet is valid.
     TODO: is this a valid assumption ? e.g. in every ISO-8859-X? */
  if (MB_CUR_MAX==1 && l==1)
    return true;

#ifdef HAVE_UTF16_SURROGATES
  uint32_t w;
  const size_t n = mbtowc_utf16 (&w, s, l);
#else
  wchar_t w;
  mbstate_t mbs;
  memset (&mbs, 0, sizeof(mbs));
  const size_t n = mbrtowc (&w, s, l, &mbs);
#endif

  if ((n == (size_t)-1) || (n==(size_t)-2))
    return false;

  /* A valid multibyte character, but the input string contains
     more characters after it (i.e. not a single character) */
  if (n!=l)
    return false;

  if (wc)
    *wc = w;
  return true;
}
