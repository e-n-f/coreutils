/* multibyte temporary test program
   Copyright (C) 1989-2016 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Assaf Gordon.  */

/*
This program prints locale,utf,wchar_t information,
based on current locale and libc implementation.

Examples:

In C locale, no multibyte-support is detected:

    $ detected locale: C
    use_multibyte: false
    is_utf8_locale_name: false
    is_utf8_wchar_ucs2:  false
    is_utf8_wchar_ucs2_surrogate:  false
    is_utf8_wchar_ucs4:  false
    mbstr( \x72 ) = wchar_t ( 0x0072 ) - as expected
    mbstr( \xce \xb1 ) = wchar_t ( 0x00ce )  - mismatch, n=1 (expected U+03b1)
    ...

In UTF-8 locale and glibc, wchar_t is implemented as UCS4:

    $ LC_ALL=en_US.UTF-8 ./src/multibyte-test
    detected locale: en_US.UTF-8
    use_multibyte: true
    is_utf8_locale_name: true
    is_utf8_wchar_ucs2:  true
    is_utf8_wchar_ucs2_surrogate:  false
    is_utf8_wchar_ucs4:  true
    mbstr( \x72 ) = wchar_t ( 0x0072 ) - as expected
    mbstr( \xce \xb1 ) = wchar_t ( 0x03b1 ) - as expected
    mbstr( \xea \x9d \xa4 ) = wchar_t ( 0xa764 ) - as expected
    mbstr( \xef \xb9 \xaa ) = wchar_t ( 0xfe6a ) - as expected
    mbstr( \xf0 \x90 \x8c \xbb ) = wchar_t ( 0x1033b ) - as expected
    mbstr( \xf0 \x9f \x82 \xb1 ) = wchar_t ( 0x1f0b1 ) - as expected

On FreeBSD, under non-utf8 multibyte locales, whcar_t is NOT UCS4:

    $ LC_ALL=zh_TW.Big5 ./src/multibyte-test
    detected locale: zh_TW.Big5
    use_multibyte: true
    is_utf8_locale_name: false
    is_utf8_wchar_ucs2:  false
    is_utf8_wchar_ucs2_surrogate:  false
    is_utf8_wchar_ucs4:  false
    mbstr( \x72 ) = wchar_t ( 0x0072 ) - as expected
    mbstr( \xce \xb1 ) = wchar_t ( 0xceb1 )  - mismatch, n=2 (expected U+03b1)
    ...

*/

#include <config.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <locale.h>
#include <string.h>

#include "unitypes.h"
#include "multibyte.h"

static void _GL_ATTRIBUTE_PURE
test_utf16_surrogates (void)
{
  assert (!is_supplementary_plane (0x0001));
  assert (!is_supplementary_plane (0xD800)); /* These are UTF-16 surrogate    */
  assert (!is_supplementary_plane (0xDC00)); /* code points, but they are not */
  assert (!is_supplementary_plane (0xDFFF)); /* SMP...                        */
  assert (!is_supplementary_plane (0xE000));
  assert (!is_supplementary_plane (0xFFFF));
  assert ( is_supplementary_plane (0x10000));
  assert ( is_supplementary_plane (0x10001));
  assert ( is_supplementary_plane (0x10000));
  assert ( is_supplementary_plane (0x10FFFE));
  assert ( is_supplementary_plane (0x10FFFF));


  assert (!is_utf16_surrogate (0x0001));
  assert (!is_utf16_surrogate (0xD7FF));
  assert ( is_utf16_surrogate (0xD800));
  assert ( is_utf16_surrogate (0xD801));
  assert ( is_utf16_surrogate (0xDBFE));
  assert ( is_utf16_surrogate (0xDBFF));
  assert ( is_utf16_surrogate (0xDC00));
  assert ( is_utf16_surrogate (0xDC01));
  assert ( is_utf16_surrogate (0xDFFE));
  assert ( is_utf16_surrogate (0xDFFF));
  assert (!is_utf16_surrogate (0xE000));
  assert (!is_utf16_surrogate (0xFFFD));
  assert (!is_utf16_surrogate (0xFFFE));
  assert (!is_utf16_surrogate (0xFFFF));

  assert (!is_utf16_surrogate_high (0x0001));
  assert (!is_utf16_surrogate_high (0xD7FF));
  assert ( is_utf16_surrogate_high (0xD800));
  assert ( is_utf16_surrogate_high (0xD801));
  assert ( is_utf16_surrogate_high (0xDBFE));
  assert ( is_utf16_surrogate_high (0xDBFF));
  assert (!is_utf16_surrogate_high (0xDC00));
  assert (!is_utf16_surrogate_high (0xDC01));
  assert (!is_utf16_surrogate_high (0xDFFE));
  assert (!is_utf16_surrogate_high (0xDFFF));
  assert (!is_utf16_surrogate_high (0xE000));
  assert (!is_utf16_surrogate_high (0xFFFD));
  assert (!is_utf16_surrogate_high (0xFFFE));
  assert (!is_utf16_surrogate_high (0xFFFF));

  assert (!is_utf16_surrogate_low (0x0001));
  assert (!is_utf16_surrogate_low (0xD7FF));
  assert (!is_utf16_surrogate_low (0xD800));
  assert (!is_utf16_surrogate_low (0xD801));
  assert (!is_utf16_surrogate_low (0xDBFE));
  assert (!is_utf16_surrogate_low (0xDBFF));
  assert ( is_utf16_surrogate_low (0xDC00));
  assert ( is_utf16_surrogate_low (0xDC01));
  assert ( is_utf16_surrogate_low (0xDFFE));
  assert ( is_utf16_surrogate_low (0xDFFF));
  assert (!is_utf16_surrogate_low (0xE000));
  assert (!is_utf16_surrogate_low (0xFFFD));
  assert (!is_utf16_surrogate_low (0xFFFE));
  assert (!is_utf16_surrogate_low (0xFFFF));
}

static void
test_utf16_surrogate_conversion (void)
{
  struct conv {
    uint16_t h;
    uint16_t l;
    uint32_t c;
  } convtable[] = {
    { 0xD800, 0xDC00,   0x010000 },
    { 0xD801, 0xDC00,   0x010400 },
    { 0xD800, 0xDC01,   0x010001 },
    { 0xD802, 0xDC00,   0x010800 },
    { 0xD803, 0xDC04,   0x010c04 },
    { 0xDBFF, 0xDFFF,   0x10FFFF },
    { 0,      0,        0}
  };

  struct conv *p = convtable;
  while (p->c)
    {
      const ucs4_t q = utf16_surrogate_to_ucs4 (p->h, p->l);
      if ( q != p->c )
        {
          fprintf (stderr,"utf16 surrogates: h,l = 0x%04x 0x%04x  "     \
                   "expected: U+%06x   got: U+%06x\n",
                   p->h, p->l, p->c, q);
          assert ( q == p->c );
        }

      wchar_t h2, l2;
      ucs4_to_utf16_surrogate_pair ( q, &h2, &l2 );
      if ( (h2 != p->h) || (l2 != p->l) )
        {
          fprintf (stderr, "utf16 surrogates: c=U+%06x, expected " \
                   "h,l = 0x%04x 0x%04x  got: 0x%04x 0x%04x\n",
                   q, p->h, p->l, (unsigned int)h2, (unsigned int)l2);
          assert ( h2 == p->h );
          assert ( l2 == p->l );
        }

      ++p;
    }
}

/* This test is only meaningful when wchar_t==ucs4.
   TODO: skip it when wchar_t==uint16 */
static void
test_emoji_wcswidth (void)
{
  const wchar_t emoji[] = { 0x0001F466, 0 };
  const wchar_t fitz[]  = { 0x0001F3FB, 0 };
  const wchar_t both[] = {  0x0001F466, 0x0001F3FB, 0 };

  #if 0
  /* UTF-8 equivalents */
  const char* emoji = "\xf0\x9f\x91\xa6";
  const char* fitz  = "\xf0\x9f\x8f\xbb";
  const char* both = "\xf0\x9f\x91\xa6\xf0\x9f\x8f\xbb";
  #endif

  ssize_t lc = (ssize_t)wcwidth (emoji[0]);
  printf ("wcwidth(U+1F466 'BOY') = %zd\n", lc);

  ssize_t ls = (ssize_t)wcswidth (emoji, 1);
  printf ("wcswidth(U+1F466 'BOY') = %zd\n", ls);

  lc = (ssize_t)wcwidth (fitz[0]);
  printf ("wcwidth(U+1F3FB 'EMOJI MODIFIER FITZPATRICK TYPE-1-2') = %zd\n", lc);

  ls = (ssize_t)wcswidth (fitz, 1);
  printf ("wcswidth(U+1F3FB 'EMOJI MODIFIER FITZPATRICK TYPE-1-2') = %zd\n", ls);

  ls = (ssize_t)wcswidth (both, 2);
  printf ("wcswidth(U+1F466 U+1F3FB) = %zd\n", ls);
}

/* Unicode block Latin Extended B
   has some digraph letters - check how wcwidth handles them. */
static void
test_latin_extB_wcwidth (void)
{
  /* Latin Capital Letter DZ with caron (U+01C4) */
  const wchar_t dz[] = { 0x01c4, 0 };

  ssize_t lc = (ssize_t)wcwidth (dz[0]);
  printf ("wcwidth(U+01C4 'capital DZ with caron') = %zd\n", lc);

  ssize_t ls = (ssize_t)wcswidth (dz, 1);
  printf ("wcswidth(U+01C4 'capital DZ with caron') = %zd\n", ls);
}

static inline const char *
btos (const bool b)
{
  return b?"true":"false";
}



/* Test the implementation of mbtowc_utf16()
   which provides ucs4_t parsing on-top of UTF-16 systems
   with UTF-16 surrogate (e.g. cygwin) */
#define ASSERT_MBTOWC_UTF16(s,c)                \
  do {                                          \
    l = strlen (s);                             \
    n = mbtowc_utf16 (&wc, s, l);               \
    assert (n == l);                            \
    assert (wc == c);                           \
  } while (0)
#define ASSERT_MBTOWC_UTF16_REJECT(s)           \
  do {                                          \
    l = strlen (s);                             \
    n = mbtowc_utf16 (&wc, s, l);               \
    assert (n == (size_t)-1);                   \
  } while (0)

static void
test_mbtowc_utf16 (void)
{
  ucs4_t wc;
  size_t n,l;

  if (!is_utf8_wchar_ucs2_surrogate())
    {
      printf ("test_mbtowc_utf16(): this system does not use UTF-16 surrogate" \
              " codes - skipping test\n");
      return ;
    }

  /* Valid UTF-8 input that fits in uint16 */
  ASSERT_MBTOWC_UTF16 ("\n", 0x0a);
  ASSERT_MBTOWC_UTF16 ("a", 0x61);

  /* LATIN SMALL LETTER E WITH ACUTE (U+00E9) \xC3\xA9 */
  ASSERT_MBTOWC_UTF16 ("\xC3\xA9", 0x00E9);

  /* NKO LETTER NYA WOLOSO (U+07E7) \xDF\xA7 */
  ASSERT_MBTOWC_UTF16 ("\xDF\xA7", 0x07E7);

  /* SAMARITAN LETTER ALAF (U+0800) \xE0\xA0\x80 */
  ASSERT_MBTOWC_UTF16 ("\xE0\xA0\x80", 0x0800);

  /* OBJECT REPLACEMENT CHARACTER (U+FFFC) */
  ASSERT_MBTOWC_UTF16 ("\xEF\xBF\xBC", 0xFFFC);


  /* Invalid UTF-8 sequences */
  ASSERT_MBTOWC_UTF16_REJECT("\xEF" "a");
  ASSERT_MBTOWC_UTF16_REJECT("\x90");
  ASSERT_MBTOWC_UTF16_REJECT("\340\200\257");



  /* Valid UTF-8 input which require UTF-16 surrogates */


  /* LINEAR B SYLLABLE B008 A (U+10000) */
  ASSERT_MBTOWC_UTF16 ("\xF0\x90\x80\x80", 0x10000);

  /* CJK COMPATIBILITY IDEOGRAPH-2FA1D (U+2FA1D) */
  ASSERT_MBTOWC_UTF16 ("\xF0\xAF\xA8\x9D", 0x2FA1D);

  /* PRIVATE USE CODEPOINT (U+FFFFD) */
  ASSERT_MBTOWC_UTF16 ("\xF3\xBF\xBF\xBD", 0xFFFFD);


  /* The following are UTF-16 surrogate code-points
     embedded in the input as UTF-8 values - these should
     be rejected (Cygwin's mbrtowc(3) accepts them, and mbtowc_utf16
     detects then and rejects them after mbrtowc()). */

  /* 'high' code-point alone */
  ASSERT_MBTOWC_UTF16_REJECT("\xED\xA0\x80");

  /* High code-point followed by non 'low' */
  ASSERT_MBTOWC_UTF16_REJECT("\xED\xA0\x80" "a");
  /* High code-point foloowed by invalid MB sequence */
  ASSERT_MBTOWC_UTF16_REJECT("\xED\xA0\x80\xED");

  /* High code-point followed by low code-point.
     (NOTE: this is valid IF cygwin's mbrtowc(3) sends them to
     the application, because the input was a 4-octet sequence,
     but is *not* valid if appeared in the input as-is.) */
  ASSERT_MBTOWC_UTF16_REJECT("\xED\xA0\x80\xED\xB0\x80");

  /* Low code-point */
  ASSERT_MBTOWC_UTF16_REJECT("\xED\xBF\xBF");

  /* The first 3 octets of a 4-octet sequence.
     Cygwin's parses the first 3 octets as valid high UTF16 surrogate,
     and on the next invocation of mbrtowc(3) cygwin encountes 'q',
     and simply returns it. Ensure such cases are rejected. */
  ASSERT_MBTOWC_UTF16_REJECT("\xF0\x90\x8Cq");

  /* called on an input with many characters */
  n = mbtowc_utf16 (&wc, "\xC3\xA9\xC3\xA9", 4);
  assert (n == 2);
  assert (wc == 0x00e9);
}



/* Test is_valid_mb_character() function.
   The function ensures the input NUL-terminated
   string represents exactly ONE valid character
   (multibyte in the current locale or single byte).

   If the input contains multibyte characters it is rejected.
   This function can be used to validate delimeter character
   parameters (e.g. 'cut -d').
*/
#define ASSERT_VALID_MB_CHAR(s,c)               \
  do {                                          \
    b = is_valid_mb_character (s, &wc);         \
    assert (b);                                 \
    assert (wc == c);                           \
  } while (0)
#define ASSERT_INVALID_MB_CHAR(s)               \
  do {                                          \
    b = is_valid_mb_character (s, &wc);         \
    assert (!b);                                \
  } while (0)

static void
test_is_valid_mb_char (void)
{
  ucs4_t wc;
  size_t n;
  bool b;

  if (!is_utf8_wchar_ucs2 ())
      {
      printf ("test_is_valid_mb_char(): current locale is not UTF-8" \
              " - skipping test\n");
      return ;
    }

  /* Valid UTF-8 input that fits in uint16 */
  ASSERT_VALID_MB_CHAR ("a", 0x61);
  ASSERT_INVALID_MB_CHAR ("aa");
  ASSERT_VALID_MB_CHAR (" ", 0x20);
  ASSERT_INVALID_MB_CHAR (" a");

  /* LATIN SMALL LETTER E WITH ACUTE (U+00E9) \xC3\xA9 */
  ASSERT_VALID_MB_CHAR ("\xC3\xA9", 0x00E9);
  ASSERT_INVALID_MB_CHAR ("\xC3\xA9""a");

  /* NKO LETTER NYA WOLOSO (U+07E7) \xDF\xA7 */
  ASSERT_VALID_MB_CHAR ("\xDF\xA7", 0x07E7);
  ASSERT_INVALID_MB_CHAR (" \xDF\xA7");

  /* SAMARITAN LETTER ALAF (U+0800) \xE0\xA0\x80 */
  ASSERT_VALID_MB_CHAR ("\xE0\xA0\x80", 0x0800);

  /* OBJECT REPLACEMENT CHARACTER (U+FFFC) */
  ASSERT_VALID_MB_CHAR ("\xEF\xBF\xBC", 0xFFFC);


  /* Invalid UTF-8 sequences */
  ASSERT_INVALID_MB_CHAR("\xEF" "a");
  ASSERT_INVALID_MB_CHAR("\x90");
  ASSERT_INVALID_MB_CHAR("\340\200\257");



  /* Valid UTF-8 input which require UTF-16 surrogates */


  /* LINEAR B SYLLABLE B008 A (U+10000) */
  ASSERT_VALID_MB_CHAR ("\xF0\x90\x80\x80", 0x10000);

  /* CJK COMPATIBILITY IDEOGRAPH-2FA1D (U+2FA1D) */
  ASSERT_VALID_MB_CHAR ("\xF0\xAF\xA8\x9D", 0x2FA1D);

  /* PRIVATE USE CODEPOINT (U+FFFFD) */
  ASSERT_VALID_MB_CHAR ("\xF3\xBF\xBF\xBD", 0xFFFFD);


  /* The following are UTF-16 surrogate code-points
     embedded in the input as UTF-8 values - these should
     be rejected (Cygwin's mbrtowc(3) accepts them, and mbtowc_utf16
     detects then and rejects them after mbrtowc()). */

  /* 'high' code-point alone */
  ASSERT_INVALID_MB_CHAR("\xED\xA0\x80");

  /* High code-point followed by non 'low' */
  ASSERT_INVALID_MB_CHAR("\xED\xA0\x80" "a");
  /* High code-point followed by invalid MB sequence */
  ASSERT_INVALID_MB_CHAR("\xED\xA0\x80\xED");

  /* High code-point followed by low code-point.
     (NOTE: this is valid IF cygwin's mbrtowc(3) sends them to
     the application, because the input was a 4-octet sequence,
     but is *not* valid if appeared in the input as-is.) */
  ASSERT_INVALID_MB_CHAR("\xED\xA0\x80\xED\xB0\x80");

  /* Low code-point */
  ASSERT_INVALID_MB_CHAR("\xED\xBF\xBF");

  /* The first 3 octets of a 4-octet sequence.
     Cygwin's parses the first 3 octets as valid high UTF16 surrogate,
     and on the next invocation of mbrtowc(3) cygwin encountes 'q',
     and simply returns it. Ensure such cases are rejected. */
  ASSERT_INVALID_MB_CHAR("\xF0\x90\x8Cq");
}


int
main (void)
{
  const char* l = setlocale(LC_ALL,"");

  printf ("detected locale: %s\n", l);

  printf ("use_multibyte: %s\n", btos (use_multibyte ()));
  printf ("is_utf8_locale_name: %s\n", btos (is_utf8_locale_name ()));
  printf ("is_utf8_wchar_ucs2:  %s\n", btos (is_utf8_wchar_ucs2 ()));
  printf ("is_utf8_wchar_ucs2_surrogate:  %s\n",
          btos (is_utf8_wchar_ucs2_surrogate ()));
  printf ("is_utf8_wchar_ucs4:  %s\n", btos (is_utf8_wchar_ucs4 ()));
  printf ("is_utf8_surrogate_input_valid:  %s\n",
          btos (is_utf8_surrogate_input_valid ()));

  test_utf16_surrogates ();
  test_utf16_surrogate_conversion ();

  debug_utf8_ucs4 ();

  test_latin_extB_wcwidth ();

  test_emoji_wcswidth ();

  test_mbtowc_utf16 ();

  test_is_valid_mb_char ();

  return 0;
}
