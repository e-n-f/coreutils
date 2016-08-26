/* mbbuffer test program
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

#include <config.h>

#include <assert.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include <sys/types.h>

#include "unitypes.h"
#include "xalloc.h"
#include "multibyte.h"
#include "mbbuffer.h"
#include "error.h"

/*
  If SUPPORT_EXTENDED_UNICODE is defined,
  this program will test UTF-8 resulting
  in unicode values beyond U+10FFFF - which is outside
  the scope of the current standard (6-octet UTF-8 codes).

  while this "just works" on Glibc systems,
  it fails on Cygwin systems as there is no way
  to represent these values with UTF-16 surrogate pairs.
*/
/* #defined SUPPORT_EXTENDED_UNICODE */



#ifdef HAVE_FMEMOPEN

/* Systems supporting fmemopen(3) function */

static FILE*
xfmemopen(const char* buf, size_t len)
{
  FILE *fp = fmemopen((void*)buf, len, "r");
  if (fp==NULL)
    error(1,errno,"fmemopen failed");
  return fp;
}

#elif HAVE_FUNOPEN

/* Systems without fmemopen(3) but with funopen(3)
   (e.g. FreeBSD 10 and aarlier). */

struct memcookie
{
  const char* buf;
  size_t len;
  size_t pos;
};

static int
fread_cookie (void* cookie, char* buf, int len)
{
  struct memcookie *mc = (struct memcookie*)cookie;
  const size_t bytes_left = mc->len - mc->pos;
  const size_t n = (bytes_left>(size_t)len)?(size_t)len:bytes_left;

  memmove (buf, mc->buf+mc->pos, n);
  mc->pos += n;
  return n;
}

static int
fclose_cookie (void *cookie)
{
  free (cookie);
  return 0;
}

static FILE*
xfmemopen(const char* buf, size_t len)
{
  struct memcookie *mc = (struct memcookie*)malloc(sizeof(struct memcookie));
  mc->buf = buf;
  mc->len = len;
  mc->pos = 0;

  FILE *fp = funopen((void*)mc, fread_cookie, NULL, NULL, fclose_cookie);
  if (fp==NULL)
    error(1,errno,"funopen failed");
  return fp;
}

#else

/* Systems without fmemopen(3) or funopen(3):
   write to a real file then open it. */

static FILE*
xfmemopen(const char* buf, size_t len)
{
  char filename[L_tmpnam+1];
  char *ptr;
  ptr = tmpnam (filename);
  if (!ptr)
    error (EXIT_FAILURE, errno, "tmpnam failed");

  FILE *f = fopen (ptr,"w");
  if (!f)
    error (EXIT_FAILURE, errno, "fopen(%s) [for writing] failed",ptr);
  size_t n = fwrite (buf, sizeof (char), len, f);
  if (n != len)
    error (EXIT_FAILURE, errno, "fwrite(%s) failed",ptr);
  if (fclose (f))
    error (EXIT_FAILURE, errno, "fclose(%s) failed",ptr);

  f = fopen (ptr,"r");
  if (!f)
    error (EXIT_FAILURE, errno, "fopen(%s) [for reading] failed",ptr);
  if (unlink (ptr))
    error (EXIT_FAILURE, errno, "unlink(%s) failed",ptr);
  return f;
}
#endif



struct mbbuf_check_data
{
  ucs4_t wc;
  bool   valid;
};

/* Given UTF8 input MB_INPUT (of length LEN),
   uses mbbuf_getchar to parse the input into wide-characters.
   expected parsed characters are in VALUES (of length NUM_VALUES).

   MB_INPUT can contain NULs and invalid multibyte sequences.
   Terminates with exit code 1 on failure.
*/
static void
mbbuf_check (const char* testname,
             const char* mb_input, size_t len,
             size_t bufsize,
             const struct mbbuf_check_data* values, size_t num_values)
{
  struct mbbuf mbb;
  mbbuf_init (&mbb, bufsize);

  FILE *fp = xfmemopen (mb_input, len);

  for (size_t i = 0; i<num_values; ++i)
    {
      const struct mbbuf_check_data* c = &values[i];

      bool b = mbbuf_getchar (&mbb, fp);
      if (!b)
        error (EXIT_FAILURE, 0, "%s: mbbuf_getchar retuned EOF/ERR, "   \
               "more characters expected", testname);

      if (c->valid != mbb.mb_valid)
        {
          error (0, 0, "%s: expecting '%s' character (0x%06x), "      \
                 "mbbuf_getchar returned '%s'", testname,
                 c->valid?"valid":"invalid",
                 (unsigned int)c->wc,
                 mbb.mb_valid?"valid":"invalid");
          mbbuf_debug_print_char (&mbb, stderr);
          fputc ('\n', stderr);
          exit (EXIT_FAILURE);
        }

      if (c->valid && (c->wc != mbb.wc))
        {
          error (0, 0, "%s: expecting U+%06x character, "    \
               "mbbuf_getchar returned U+%06x", testname, (unsigned int)c->wc,
               (unsigned int)mbb.wc);
          mbbuf_debug_print_char (&mbb, stderr);
          fputc ('\n', stderr);
          exit (EXIT_FAILURE);
        }

      /* When expecting invalid characters,
         compare the first octet of the multibyte string. */
      if (!c->valid && (c->wc != (unsigned char)mbb.mb_str[0]))
        {
          error (0, 0, "%s: expecting 0x%02X octet (invalid wchar), " \
                 "mbbuf_getchar returned 0x%02X octet",
                 testname, (unsigned int)(unsigned char)c->wc,
                 (unsigned int)(unsigned char)mbb.mb_str[0]);
          mbbuf_debug_print_char (&mbb, stderr);
          fputc ('\n', stderr);
          exit (EXIT_FAILURE);
        }
    }

  bool b = mbbuf_getchar (&mbb, fp);
  if (b)
    {
      error (EXIT_FAILURE, 0, "%s: mbbuf_getchar returned too many characters",
             testname);
      mbbuf_debug_print_char (&mbb, stderr);
      fputc ('\n', stderr);
      exit (EXIT_FAILURE);
    }

  if (!mbb.eof)
    error (EXIT_FAILURE, 0, "%s: mbbufer_getchar did not set EOF",
           testname);

  mbbuf_free (&mbb);
  fclose (fp);
}



/* Given a NUL-terminated multibyte input MB_INPUT,
   parses the input using mbbuf_getchar and checks if it results
   in one valid unicode wide-character whose value is VALUE.

   Terminates on error. */
static void
mbbuf_check_char (const char* testname, const char* mb_input, ucs4_t value)
{
  struct mbbuf_check_data wc;
  wc.wc = value;
  wc.valid = true;

  mbbuf_check (testname, mb_input, strlen (mb_input), BUFSIZ, &wc, 1);
}


/* Given a NUL-terminated multibyte input MB_INPUT,
   parses the input using mbbuf_getchar and checks if it results
   in the unicode VALUES. The last element of VALUES must be zero
   (to indicate number of expected ucs4_t values).

   MB_INPUT must contain only valid charactrers. To test invalid multibyte
   input use mbbuf_check directly.

   Terminates on error. */
static void
mbbuf_check_string (const char* testname, const char* mb_input,
                    const ucs4_t *values)
{
  size_t num_values;

  for (num_values=0;values[num_values]!=0;++num_values)
    ;

  struct mbbuf_check_data *cd = XCALLOC(num_values, struct mbbuf_check_data);
  for (size_t i=0; i<num_values; ++i)
    {
      cd[i].wc = values[i];
      cd[i].valid = true;
    }
  mbbuf_check (testname, mb_input, strlen (mb_input), BUFSIZ, cd, num_values);
  free (cd);
}



static void
test_ascii (void)
{
  mbbuf_check_char ("asc1", "A", 0x41);

  const ucs4_t test1[] = { 'a', 'b', 'c', 'd', 0};
  mbbuf_check_string ("asc2","abcd", test1);

  /* control characters  */
  const ucs4_t asc3[] = { 1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                          13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                          25, 26, 27, 28, 29, 30, 31, 127, 0};
  mbbuf_check_string ("asc3","\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B" \
                             "\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16" \
                             "\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x7F",
                      asc3);
}

static void
test_invalid (void)
{
  /* Range 0x8x-0xBx was valid in UTF-1 - should be rejected in UTF-8. */
  const struct mbbuf_check_data inv1 = { 0x80, false };
  mbbuf_check ("inv1","\x80", 1, BUFSIZ, &inv1, 1 );

  const struct mbbuf_check_data inv2 = { 0x90, false };
  mbbuf_check ("inv2","\x90", 1, BUFSIZ, &inv2, 1 );

  const struct mbbuf_check_data inv3 = { 0xA0, false };
  mbbuf_check ("inv3","\xA0", 1, BUFSIZ, &inv3, 1 );

  const struct mbbuf_check_data inv4 = { 0xB0, false };
  mbbuf_check ("inv4","\xB0", 1, BUFSIZ, &inv4, 1 );

}


static void
test_utf8_bmp_2octets (void)
{
  /* LATIN SMALL LETTER E WITH ACUTE (U+00E9)
     $ printf '\u00e9' | od -tx1
      c3 a9
  */
  mbbuf_check_char ("bmp1", "\xc3\xa9", 0x00e9);

  /* Unicode block: Latin-Extended-A
     Latin Capital Letter L with stroke (U+0141)
     $ printf '\u00e9' | od -tx1
      c5 81
  */
  mbbuf_check_char ("bmp2", "\xc5\x81", 0x0141);

  /* Unicode block: Latin-Extended-B
     Latin Capital Letter E with cedilla (U+0228)
     $ printf '\u0228' | od -tx1
      c8 a8
  */
  mbbuf_check_char ("bmp3", "\xc8\xa8", 0x0228);

  /* Unicode block: IPA Extensions
     Latin Letter Bilabial Click (U+0298)
     $ printf '\u0298' | od -tx1
      ca 98
  */
  mbbuf_check_char ("bmp4", "\xca\x98", 0x0298);

  /* Unicode block: Combining diacritical marks
     COMBINING RING ABOVE (U+030A)
     $ printf '\u030A' | od -tx1
      cc 8a
  */
  mbbuf_check_char ("bmp5", "\xcc\x8a", 0x030A);

  /* Unicode block: NKo
     NKO LETTER NYA WOLOSO (U+07E7)
     $ printf '\u07e7' | od -tx1
      df a7
  */
  mbbuf_check_char ("bmp6", "\xdf\xa7", 0x07E7);

  /* Unicode block: NKo
     RESERVED (U+07FF) - but should be parsed without error.

     $ printf '\u07FF' | od -tx1
      df bf
  */
  mbbuf_check_char ("bmp7", "\xdf\xbf", 0x07FF);
}


/* NOTE:
   UTF-16 Surrogate code-points are 3 octets
   but are tested in separate function below. */
static void
test_utf8_bmp_3octets (void)
{
  /* Unicode block: Samaritan
     SAMARITAN LETTER ALAF (U+0800)

     $ printf '\u0800' | od -tx1
      e0 a0 80
  */
  mbbuf_check_char ("smr1", "\xe0\xa0\x80", 0x0800);


  /* Unicode block: Hangul Syllables
     HANGUL SYLLABLE GA (U+AC00)

     $ printf '\uAC00' | od -tx1
      ea b0 80
  */
  mbbuf_check_char ("hng1", "\xea\xb0\x80", 0xAC00);

  /* Unicode block: Hangul Syllables
     HANGUL SYLLABLE HIH (U+D7A3)

     $ printf '\uD7A3' | od -tx1
      ed 9e a3
  */
  mbbuf_check_char ("hng2", "\xed\x9e\xa3", 0xD7A3);

  /* Unicode block: Hangul Syllables
     RESERVED (U+D7AF) - but should be parsable without error

     $ printf '\uD7AF' | od -tx1
      ed 9e af
  */
  mbbuf_check_char ("hng3", "\xed\x9e\xaf", 0xD7AF);

  /* Unicode block: Private Use (BMP)
     PRIVATE USE CODEPOINT (U+E000) - should be parsed as-is without error

     $ printf '\uE000' | od -tx1
      ee 80 80
  */
  mbbuf_check_char ("prv1", "\xee\x80\x80", 0xE000);

  /* Unicode block: Private Use (BMP)
     PRIVATE USE CODEPOINT (U+F8FF) - should be parsed as-is without error

     $ printf '\uF8FF' | od -tx1
      ef a3 bf
  */
  mbbuf_check_char ("prv2", "\xef\xa3\xbf", 0xF8FF);


  /* Unicode block: Full width forms
     FULLWIDTH LATIN CAPITAL A (U+FF21)

     $ printf '\uFF21' | od -tx1
      ef bc a1
  */
  mbbuf_check_char ("flw1", "\xef\xbc\xa1", 0xFF21);


  /* Unicode block: Full width forms
     FULLWIDTH LATIN CAPITAL A (U+FF21)

     $ printf '\uFF21' | od -tx1
      ef bc a1
  */
  mbbuf_check_char ("flw1", "\xef\xbc\xa1", 0xFF21);


  /* Unicode block: Specials
     OBJECT REPLACEMENT CHARACTER (U+FFFC)

     $ printf '\uFFFC' | od -tx1
      ef bf bc
  */
  mbbuf_check_char ("spc1", "\xef\xbf\xbc", 0xFFFC);

  /* Unicode block: Specials
     NONCHARACTER (U+FFFF) - should be parsed as-is without error.

     $ printf '\uFFFF' | od -tx1
      ef bf bf
  */
  mbbuf_check_char ("spc2", "\xef\xbf\xbf", 0xFFFf);
}




static void
test_utf8_smp1 (void)
{
  /* Unicode block: Linear B Syllabary
     LINEAR B SYLLABLE B008 A (U+10000)

     $ printf '\U00010000' | od -tx1
      f0 90 80 80
  */
  mbbuf_check_char ("lnr1", "\xf0\x90\x80\x80", 0x10000);


  /* Unicode block: Supp Symbols and Pictographs
     SQUID (U+1F991)

     $ printf '\U0001F991' | od -tx1
      f0 9f a6 91
  */
  mbbuf_check_char ("pic1", "\xf0\x9f\xa6\x91", 0x1F991);

  /* Unicode block: Supp Symbols and Pictographs
     UNASSIGNED (U+1F9FF) - should be parsed as-is.

     $ printf '\U0001F9FF' | od -tx1
      f0 9f a7 bf
  */
  mbbuf_check_char ("pic2", "\xf0\x9f\xa7\xbf", 0x1F9FF);

}

static void
test_utf8_sip2 (void)
{
  /* Unicode block: CJK Unified Ideographs Extension B
     CJK UNIFIED IDEOGRAPH-20000 (U+20000)

     $ printf '\U00020000' | od -tx1
      f0 A0 80 80
  */
  mbbuf_check_char ("cjk1", "\xf0\xA0\x80\x80", 0x20000);

  /* Unicode block: CJK Compatibility Ideographs Supplement
     CJK COMPATIBILITY IDEOGRAPH-2FA1D (U+2FA1D)

     $ printf '\U0002FA1D' | od -tx1
      f0 AF A8 9D
  */
  mbbuf_check_char ("cjk2", "\xf0\xAF\xA8\x9d", 0x2FA1D);

  /* Unicode block: CJK Compatibility Ideographs Supplement
     UNASSIGNED (U+2FA1F) - should be parsed as-is without error.

     $ printf '\U0002FA1F' | od -tx1
      f0 AF A8 9F
  */
  mbbuf_check_char ("cjk3", "\xf0\xAF\xA8\x9F", 0x2FA1F);
}



static void
test_utf8_ssp14 (void)
{
  /* Unicode block: Tags
     LANGUAGE TAG (U+E0001) - deprecated as of Unicode 5.1, but
                              should still be parsed without error.

     $ printf '\U000E0001' | od -tx1
      f3 a0 80 81
  */
  mbbuf_check_char ("tag1", "\xf3\xa0\x80\x81", 0xE0001);

  /* Unicode block: Variation Selectors Supplement
     VARIATION SELECTOR-256 (U+E01EF)

     $ printf '\U000E01EF' | od -tx1
     f3 a0 87 af
  */
  mbbuf_check_char ("vsel1", "\xf3\xa0\x87\xaf", 0xE01EF);
}

static void
test_utf8_puaA15 (void)
{
  /* Unicode block: Private Use Areas A
     PRIVATE USE CODEPOINT (U+F0000)
     $ printf '\U000F0000' | od -tx1
      f3 b0 80 80
  */
  mbbuf_check_char ("puaA1", "\xf3\xb0\x80\x80", 0xF0000);

  /* Unicode block: Private Use Areas A
     PRIVATE USE CODEPOINT (U+FFFFD)
     $ printf '\U000FFFFD' | od -tx1
      f3 bf bf bd
  */
  mbbuf_check_char ("puaA2", "\xf3\xbf\xbf\xbd", 0xFFFFD);
}

static void
test_utf8_puaB16 (void)
{
  /* Unicode block: Private Use Areas B
     PRIVATE USE CODEPOINT (U+100000)
     $ printf '\U00100000' | od -tx1
      f4 80 80 80
  */
  mbbuf_check_char ("puaB1", "\xf4\x80\x80\x80", 0x100000);

  /* Unicode block: Private Use Areas B
     PRIVATE USE CODEPOINT (U+10FFFF)
     $ printf '\U0010FFFF' | od -tx1
      f4 8f bf bf
  */
  mbbuf_check_char ("puaB2", "\xf4\x8f\xbf\xbf", 0x10FFFF);
}


/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 2 "Boundary condition test cases". */
static void
test_boundary_conditions (void)
{
  /* 2.1.1 */
  const struct mbbuf_check_data bnd1 = { 0x00, true };
  mbbuf_check ("bnd1","\x00", 1, BUFSIZ, &bnd1, 1 );

  /* 2.1.2 */
  mbbuf_check_char ("bnd2","\302\200", 0x80 );

  /* 2.1.3 */
  mbbuf_check_char ("bnd3","\340\240\200", 0x800 );

  /* 2.1.4 */
  mbbuf_check_char ("bnd4","\360\220\200\200", 0x10000 );

#ifdef SUPPORT_EXTENDED_UNICODE
  /* 2.1.5 */
  mbbuf_check_char ("bnd5","\370\210\200\200\200", 0x200000 );

  /* 2.1.6 */
  mbbuf_check_char ("bnd6","\374\204\200\200\200\200", 0x4000000 );
#endif

  /* 2.2.1 */
  mbbuf_check_char ("bnd7","\x7F", 0x7F );

  /* 2.2.2 */
  mbbuf_check_char ("bnd8","\337\277", 0x7FF );

  /* 2.2.3 */
  mbbuf_check_char ("bnd9","\357\277\277", 0xFFFF );

#ifdef SUPPORT_EXTENDED_UNICODE
  /* 2.2.4 */
  mbbuf_check_char ("bnd10","\367\277\277\277", 0x1FFFFF );

  /* 2.2.5 */
  mbbuf_check_char ("bnd11","\373\277\277\277\277", 0x3FFFFFF );

  /* 2.2.6 */
  mbbuf_check_char ("bnd12","\375\277\277\277\277\277", 0x7FFFFFFF );
#endif

  /* 2.3.1 */
  mbbuf_check_char ("bnd13","\355\237\277", 0xD7FF );

  /* 2.3.2 */
  mbbuf_check_char ("bnd14","\356\200\200", 0xE000 );

  /* 2.3.3 */
  mbbuf_check_char ("bnd15","\357\277\275", 0xFFFD );

  /* 2.3.4 */
  mbbuf_check_char ("bnd16","\364\217\277\277", 0x10FFFF );

#ifdef SUPPORT_EXTENDED_UNICODE
  /* 2.3.5 */
  mbbuf_check_char ("bnd17","\364\220\200\200", 0x110000 );
#endif
}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 3.1 "Unexpected Continuation Bytes"

   Each should be parsed as individual octets.
*/
static void
test_unexpected_continuation (void)
{
  /* 3.1.1 */
  const struct mbbuf_check_data ucnt1 = { 0x80, false };
  mbbuf_check ("ucnt1","\x80", 1, BUFSIZ, &ucnt1, 1 );

  /* 3.1.2 */
  const struct mbbuf_check_data ucnt2 = { 0xbf, false };
  mbbuf_check ("ucnt2","\xBF", 1, BUFSIZ, &ucnt2, 1 );

  /* input for 3.1.3 - 3.1.8  */
  const struct mbbuf_check_data ucnt3[] = {
    { 0200, false },
    { 0277, false },
    { 0200, false },
    { 0277, false },
    { 0200, false },
    { 0277, false },
    { 0200, false }
  };
  const char* ucnt3_mbstr = "\200\277\200\277\200\277\200";

  for (size_t i = 2; i <= 7; ++i)
    {
      char testname[10];
      snprintf(testname,sizeof (testname), "ucntx%d", (int)i);

      mbbuf_check (testname, ucnt3_mbstr, i, BUFSIZ, ucnt3, i );
    }


}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 3.2 "Lonely start characters"

   Each should be parsed as individual octets.
*/
static void
test_lonely_start (void)
{
  struct mbbuf_check_data ls[2];
  char mbstr[2]; /* input multibyte string */
  char testname[10];

  /* The second input octet is always space */
  mbstr[1] = ' ';

  /* expected output: first octet is invalid,
     followed by a space. */
  ls[0].wc = 0; /* dummy value, will be set below */
  ls[0].valid = false;
  ls[1].wc = ' ';
  ls[1].valid = true;

  for (size_t i = 0x80 ; i <= 0xfd ; ++i)
    {
      snprintf(testname,sizeof (testname), "lnly%d", (int)i);

      mbstr[0] = (unsigned char)i;
      ls[0].wc = i;

      mbbuf_check (testname,mbstr,2, BUFSIZ,ls,2);
    }
}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 3.3 "Sequences with last continuation byte missing"

   Each should be parsed as individual octets.
*/
static void
test_last_byte_missing (void)
{
  /* 3.3.1 */
  const struct mbbuf_check_data lst1[] = {
    { 0300, false }
  };
  mbbuf_check ("lst1","\300", 1, BUFSIZ, lst1, 1);

  /* 3.3.2 */
  const struct mbbuf_check_data lst2[] = {
    { 0340, false },
    { 0200, false }
  };
  mbbuf_check ("lst2","\340\200", 2, BUFSIZ, lst2, 2);

  /* 3.3.3 */
  const struct mbbuf_check_data lst3[] = {
    { 0360, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("lst3","\360\200\200", 3, BUFSIZ, lst3, 3);

  /* 3.3.4 */
  const struct mbbuf_check_data lst4[] = {
    { 0370, false },
    { 0200, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("lst4","\370\200\200\200", 4, BUFSIZ, lst4, 4);

  /* 3.3.5 */
  const struct mbbuf_check_data lst5[] = {
    { 0374, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("lst5","\374\200\200\200\200", 5, BUFSIZ, lst5, 5);



  /* 3.3.6 */
  const struct mbbuf_check_data lst6[] = {
    { 0337, false }
  };
  mbbuf_check ("lst6","\337", 1, BUFSIZ, lst6, 1);

  /* 3.3.7 */
  const struct mbbuf_check_data lst7[] = {
    { 0357, false },
    { 0277, false }
  };
  mbbuf_check ("lst7","\357\277", 2, BUFSIZ, lst7, 2);

  /* 3.3.8 */
  const struct mbbuf_check_data lst8[] = {
    { 0367, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("lst8","\367\277\277", 3, BUFSIZ, lst8, 3);

  /* 3.3.9 */
  const struct mbbuf_check_data lst9[] = {
    { 0373, false },
    { 0277, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("lst9","\373\277\277\277", 4, BUFSIZ, lst9, 4);

  /* 3.3.10 */
  const struct mbbuf_check_data lst10[] = {
    { 0375, false },
    { 0277, false },
    { 0277, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("lst10","\375\277\277\277\277", 5, BUFSIZ, lst10, 5);


  /*
     3.4 - all the above, concatenated.

     NOTE:
     The UTF8 test page says:
     > All the 10 sequences of 3.3 concatenated, you should see 10 malformed
     > sequences being signalled:

     Unlike the recommendation of the Markus Kuhn's UTF8 test page,
     the mbbuffer parser returns EACH invalid octet as an independant
     character.
  */
  const struct mbbuf_check_data lst11[] = {
    { 0300, false },
    { 0340, false },
    { 0200, false },
    { 0360, false },
    { 0200, false },
    { 0200, false },
    { 0370, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0374, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0337, false },
    { 0357, false },
    { 0277, false },
    { 0367, false },
    { 0277, false },
    { 0277, false },
    { 0373, false },
    { 0277, false },
    { 0277, false },
    { 0277, false },
    { 0375, false },
    { 0277, false },
    { 0277, false },
    { 0277, false },
    { 0277, false }
  };
  const char* lst11_in = "\300\340\200\360\200\200\370\200\200\200" \
                         "\374\200\200\200\200\337\357\277\367\277\277" \
                         "\373\277\277\277\375\277\277\277\277";

  mbbuf_check ("lst11",lst11_in, strlen(lst11_in), BUFSIZ,
               lst11, sizeof(lst11)/sizeof(lst11[0]));
}

/* Impossible UTF8 bytes, per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 3.5 "Impossible Bytes".

   Each should be parsed as individual octets.
*/
static void
test_impossible (void)
{
  /* 3.5.1 */
  const struct mbbuf_check_data imp1 = { 0xfe, false };
  mbbuf_check ("imp1","\xFE", 1, BUFSIZ, &imp1, 1 );

  /* 3.5.2 */
  const struct mbbuf_check_data imp2 = { 0xff, false };
  mbbuf_check ("imp2","\xFF", 1, BUFSIZ, &imp2, 1 );

  /* 3.5.3 */
  const struct mbbuf_check_data imp3[] = {
    { 0xfe, false },
    { 0xfe, false },
    { 0xff, false },
    { 0xff, false }
  };
  mbbuf_check ("imp3","\xFE\xFE\xFF\xFF", 4, BUFSIZ, imp3, 4 );
}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 4.1 "Examples of overlong ASCII characters"

   Each should be parsed as individual octets.
*/
static void
test_overlong_ascii (void)
{
  /* 4.1.1 */
  const struct mbbuf_check_data ola1[] = {
    { 0300, false },
    { 0257, false }
  };
  mbbuf_check ("ola1","\300\257", 2, BUFSIZ, ola1, 2 );

  /* 4.1.2 */
  const struct mbbuf_check_data ola2[] = {
    { 0340, false },
    { 0200, false },
    { 0257, false }
  };
  mbbuf_check ("ola2","\340\200\257", 3, BUFSIZ, ola2, 3 );

  /* 4.1.3 */
  const struct mbbuf_check_data ola3[] = {
    { 0360, false },
    { 0200, false },
    { 0200, false },
    { 0257, false }
  };
  mbbuf_check ("ola3","\360\200\200\257", 4, BUFSIZ, ola3, 4 );

  /* 4.1.4 */
  const struct mbbuf_check_data ola4[] = {
    { 0370, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0257, false }
  };
  mbbuf_check ("ola4","\370\200\200\200\257", 5, BUFSIZ, ola4, 5 );

  /* 4.1.5 */
  const struct mbbuf_check_data ola5[] = {
    { 0374, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0257, false }
  };
  mbbuf_check ("ola5","\374\200\200\200\200\257", 6, BUFSIZ, ola5, 6 );
}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 4.2 "Maximmim overlong sequences

   Each should be parsed as individual octets.
*/
static void
test_max_overlong (void)
{
  /* 4.2.1 */
  const struct mbbuf_check_data mol1[] = {
    { 0301, false },
    { 0277, false }
  };
  mbbuf_check ("mol1","\301\277", 2, BUFSIZ, mol1, 2 );

  /* 4.2.2 */
  const struct mbbuf_check_data mol2[] = {
    { 0340, false },
    { 0237, false },
    { 0277, false }
  };
  mbbuf_check ("mol2","\340\237\277", 3, BUFSIZ, mol2, 3 );

  /* 4.2.3 */
  const struct mbbuf_check_data mol3[] = {
    { 0360, false },
    { 0217, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("mol3","\360\217\277\277", 4, BUFSIZ, mol3, 4 );

  /* 4.2.4 */
  const struct mbbuf_check_data mol4[] = {
    { 0370, false },
    { 0207, false },
    { 0277, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("mol4","\370\207\277\277\277", 5, BUFSIZ, mol4, 5 );

  /* 4.2.5 */
  const struct mbbuf_check_data mol5[] = {
    { 0374, false },
    { 0203, false },
    { 0277, false },
    { 0277, false },
    { 0277, false },
    { 0277, false }
  };
  mbbuf_check ("mol5","\374\203\277\277\277\277", 6, BUFSIZ, mol5, 6 );
}

/* per UTF-8-Test page,
   Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
   Section 4.3 "Overlong NUL"

   Each should be parsed as individual octets.
*/
static void
test_overlong_nul (void)
{
  /* 4.3.1 */
  /* NOTE:
     Overlong NUL is accepted by Emacs/Java (modified-UTF8 parsers?).
     Currently rejected by mbbuffer parser. */
  const struct mbbuf_check_data oln1[] = {
    { 0300, false },
    { 0200, false }
  };
  mbbuf_check ("oln1","\300\200", 2, BUFSIZ, oln1, 2 );

  /* 4.3.2 */
  const struct mbbuf_check_data oln2[] = {
    { 0340, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("oln2","\340\200\200", 3, BUFSIZ, oln2, 3 );

  /* 4.3.3 */
  const struct mbbuf_check_data oln3[] = {
    { 0360, false },
    { 0200, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("oln3","\360\200\200\200", 4, BUFSIZ, oln3, 4 );

  /* 4.3.4 */
  const struct mbbuf_check_data oln4[] = {
    { 0370, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("oln4","\370\200\200\200\200", 5, BUFSIZ, oln4, 5 );

  /* 4.3.5 */
  const struct mbbuf_check_data oln5[] = {
    { 0374, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false },
    { 0200, false }
  };
  mbbuf_check ("oln5","\374\200\200\200\200\200", 6, BUFSIZ, oln5, 6 );
}



/*per UTF-8-Test page,
  Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
  Section 5.1 "Single UTF-16 Surrogates" .

  Each should be parsed as individual octets.

  On most systems (glibc, BSD), unicode UTF-16 surrogate
  code-points in the input files are rejected (mbrtowc(3) returns -1).

  On systems using UTF16 surrogates (where wchar_t==uint16, e.g. Cygwin),
  The native mbrtowc(3) accepts such input.

  The additional code in mbbuffer.c:mbbuf_parse_next_char() rejects it
  on such systems for consistency.

  UTF-16 surrogate codepoints in the input is treated as a sequence
  of invalid multibyte octets (as if mbrtowc(3) returns -1 for each).
 */
static void
test_single_utf16_surrogates (void)
{
  /* 5.1.1 */
  const struct mbbuf_check_data srg1[] = {
    { 0xED, false },
    { 0xA0, false },
    { 0x80, false }
  };
  mbbuf_check ("srg1","\xED\xA0\x80", 3, BUFSIZ, srg1, 3 );

  /* 5.1.2 */
  const struct mbbuf_check_data srg2[] = {
    { 0xED, false },
    { 0xAD, false },
    { 0xBF, false }
  };
  mbbuf_check ("srg1","\xED\xAD\xBF", 3, BUFSIZ, srg2, 3 );

  /* 5.1.3 */
  const struct mbbuf_check_data srg3[] = {
    { 0xED, false },
    { 0xAE, false },
    { 0x80, false }
  };
  mbbuf_check ("srg3","\xED\xAE\x80", 3, BUFSIZ, srg3, 3 );

  /* 5.1.4 */
  const struct mbbuf_check_data srg4[] = {
    { 0xed, false },
    { 0xaf, false },
    { 0xbf, false }
  };
  mbbuf_check ("srg4","\xED\xAF\xBF", 3, BUFSIZ, srg4, 3 );

  /* 5.1.5 */
  const struct mbbuf_check_data srg5[] = {
    { 0xed, false },
    { 0xb0, false },
    { 0x80, false }
  };
  mbbuf_check ("srg5","\xED\xB0\x80", 3, BUFSIZ, srg5, 3 );

  /* 5.1.6 */
  const struct mbbuf_check_data srg6[] = {
    { 0xed, false },
    { 0xbe, false },
    { 0x80, false }
  };
  mbbuf_check ("srg6","\xED\xBE\x80", 3, BUFSIZ, srg6, 3 );

  /* 5.1.7 */
  const struct mbbuf_check_data srg7[] = {
    { 0xed, false },
    { 0xbf, false },
    { 0xbf, false }
  };
  mbbuf_check ("srg7","\xED\xBF\xBF", 3, BUFSIZ, srg7, 3 );
}

/*per UTF-8-Test page,
  Markus Kuhn <http://www.cl.cam.ac.uk/~mgk25/>
  Section 5.2 "Paired UTF-16 Surrogates" .

  Two UTF-16 surrogate code-points (low+high),
  On Cygwin these are both valid, and could result
  in U+100000 (if the application calls mbrtowc(3) twice
  and reconstructs the unicode code-point).
  The added code in mbbuffer rejects such input, even on Cygwin.
*/
static void
test_paired_utf16_surrogates (void)
{
  /* 5.2.1 */
  const struct mbbuf_check_data srgp1[] = {
    { 0xED, false },
    { 0xA0, false },
    { 0x80, false },
    { 0xED, false },
    { 0xB0, false },
    { 0x80, false }
  };
  mbbuf_check ("srgp1", "\xED\xA0\x80\xED\xB0\x80", 6, BUFSIZ, srgp1, 6);

  /* 5.2.2 */
  const struct mbbuf_check_data srgp2[] = {
    { 0xED, false },
    { 0xA0, false },
    { 0x80, false },
    { 0xED, false },
    { 0xBF, false },
    { 0xBF, false }
  };
  mbbuf_check ("srgp2", "\xED\xA0\x80\xED\xBF\xBF", 6, BUFSIZ, srgp2, 6);

  /* 5.2.3 */
  const struct mbbuf_check_data srgp3[] = {
    { 0xED, false },
    { 0xAD, false },
    { 0xBF, false },
    { 0xED, false },
    { 0xB0, false },
    { 0x80, false }
  };
  mbbuf_check ("srgp3", "\xED\xAD\xBF\xED\xB0\x80", 6, BUFSIZ, srgp3, 6);

  /* 5.2.4 */
  const struct mbbuf_check_data srgp4[] = {
    { 0xED, false },
    { 0xAD, false },
    { 0xBF, false },
    { 0xED, false },
    { 0xBF, false },
    { 0xBF, false }
  };
  mbbuf_check ("srgp4", "\xED\xAD\xBF\xED\xBF\xBF", 6, BUFSIZ, srgp4, 6);

  /* 5.2.5 */
  const struct mbbuf_check_data srgp5[] = {
    { 0xED, false },
    { 0xAE, false },
    { 0x80, false },
    { 0xED, false },
    { 0xB0, false },
    { 0x80, false }
  };
  mbbuf_check ("srgp5", "\xED\xAE\x80\xED\xB0\x80", 6, BUFSIZ, srgp5, 6);

  /* 5.2.6 */
  const struct mbbuf_check_data srgp6[] = {
    { 0xED, false },
    { 0xAE, false },
    { 0x80, false },
    { 0xED, false },
    { 0xBF, false },
    { 0xBF, false }
  };
  mbbuf_check ("srgp6", "\xED\xAE\x80\xED\xBF\xBF", 6, BUFSIZ, srgp6, 6);

  /* 5.2.7 */
  const struct mbbuf_check_data srgp7[] = {
    { 0xED, false },
    { 0xaf, false },
    { 0xbf, false },
    { 0xED, false },
    { 0xB0, false },
    { 0x80, false }
  };
  mbbuf_check ("srgp7", "\xED\xAF\xBF\xED\xB0\x80", 6, BUFSIZ, srgp7, 6);

  /* 5.2.8 */
  const struct mbbuf_check_data srgp8[] = {
    { 0xED, false },
    { 0xAF, false },
    { 0xBF, false },
    { 0xED, false },
    { 0xBF, false },
    { 0xBF, false }
  };
  mbbuf_check ("srgp1", "\xED\xAF\xBF\xED\xBF\xBF", 6, BUFSIZ, srgp8, 6);
}


int main (void)
{
  setlocale (LC_ALL, "");

  test_ascii ();

  test_invalid ();

  test_utf8_bmp_2octets ();

  test_utf8_bmp_3octets ();

  test_utf8_smp1 ();

  test_utf8_sip2 ();

  test_utf8_ssp14 ();

  test_utf8_puaA15 ();

  test_utf8_puaB16 ();

  test_boundary_conditions ();

  test_unexpected_continuation ();

  test_lonely_start ();

  test_last_byte_missing ();

  test_impossible ();

  test_overlong_ascii ();

  test_max_overlong ();

  test_overlong_nul ();

  test_single_utf16_surrogates ();

  test_paired_utf16_surrogates ();

  return 0;
}
