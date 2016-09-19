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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
/*
These are taken from Pádraig Brady's 'i18n' branch of coreutils
at: https://github.com/pixelb/coreutils/commit/2a2f58c1ee
*/

/* Get mbstate_t, mbrtowc(), wcwidth(). */
#if HAVE_WCHAR_H
# include <wchar.h>
#endif

/* Get iswblank(). */
#if HAVE_WCTYPE_H
# include <wctype.h>
#endif

/* MB_LEN_MAX is incorrectly defined to be 1 in at least one GCC
   installation; work around this configuration error.  */
#if !defined MB_LEN_MAX || MB_LEN_MAX < 2
# define MB_LEN_MAX 16
#endif


/* Special handling for systems where wchar_t==uint16,
   and special handling for UTF16 surrogate pairs is required
   (e.g. Cygwin, AIX/32bit).

   TODO: This should be a check during 'configure' according
   to sizeof(wchar_t) + mbrtowc's ability to handle UTF16 surrogates,
   instead of hard-coding an operating-system #define */
#ifdef __CYGWIN__
#define HAVE_UTF16_SURROGATES
#endif
#ifdef __AIX
  /* TODO: is there a specific def for AIX/32bit? */
  #ifdef __64BIT__
  #else
    #define HAVE_UTF16_SURROGATES
  #endif
#endif





#include "unitypes.h"

/* Return TRUE if the currently active locale is multibyte locale
   (i.e MB_CUR_MAX>1).
   Always returns FALSE if coreutils was not compiled with multibyte support.
   TRUE does not imply UTF-8 locale (or any other specific locale).

   The following locales are known to return FALSE:
     'C', 'POSIX'
     'en_US.iso88591' (on glibc)

   The following locales are known to return TRUE:
     'C.UTF-8' (on musl-libc)
     'en_US.iso88591' (on musl-libc)
   */
bool
use_multibyte (void);


/* Return TRUE if the name of the current locale
   ends with one of the following:
      .UTF-8
      .UTF8
      .utf-8
      .utf8
   This only checks the string of the locale name -
   not actual implementation support.

   NOTE:
   On AIX-7.1 this check is meaningless: according to the manual,
   UTF-8 locale support is indicated by upper-case locale name
   (e.g. "en_US" = single-byte, "EN_US" = UTF-8).
*/
bool
is_utf8_locale_name (void);


/* Return TRUE if the current locale is able to parse UTF-8 input
   and internal representation of wchar_t supports
   AT LEAST 16-bit unicode code-point values.
   TODO: distinguish between UCS-2 and BMP. */
bool
is_utf8_wchar_ucs2 (void);

/* Return TRUE if the current locale is able to parse UTF-8 input
   and internal representation of wchar_t supports
   UCS-2 with surrogate pairs. */
bool
is_utf8_wchar_ucs2_surrogate (void) _GL_ATTRIBUTE_CONST;


/* Return TRUE if the current locale is able to parse UTF-8 input,
   AND can accept UTF-16 surrogate pair codepoints,
   e.g. "\xED\xA0\x81" (which is U+D801).

   On most systems with wchar_t==ucs4, mbrtowc(3) will treat the
   above input as 3 invalid octects.
   On cygwin (where wchar_t==uint16 and UTf-16 surrogates are supported),
   mbrtowc(3) will return wc==0xD801.
*/
bool
is_utf8_surrogate_input_valid (void) _GL_ATTRIBUTE_CONST;


/* Return TRUE if the current locale is able to parse UTF-8 input
   and internal representation of wchar_t supports
   32-bit unicode code-point values (UCS-4).

   Only SMP1 (Supp. Multilingual Plane) is checked.
   TODO: Consider adding SIP-2 and others. */
bool
is_utf8_wchar_ucs4 (void);


/* Debug helper: print to stdout the conversion
   results of testing utf8/ucs2/ucs4. */
void
debug_utf8_ucs4 (void);


/* Return TRUE if C is a unicode codepoint in the range of
   U+10000 to U+10FFFF
 */
bool
is_supplementary_plane (ucs4_t c) _GL_ATTRIBUTE_CONST;


/* Return TRUE if C is a unicode codepoint in the range of
   UTF-16 surrogate pairs (high or low):
   (C>=0xD800) && (C<=0xDFFF)
 */
bool
is_utf16_surrogate (ucs4_t c) _GL_ATTRIBUTE_CONST;

/* Return TRUE if C is a unicode codepoint in the range of
   "high" (leading) UTF-16 surrogate pairs
   (C>=0xD800) && (C<=0xDBFF) */
bool
is_utf16_surrogate_high (ucs4_t c) _GL_ATTRIBUTE_CONST;

/* Return TRUE if C is a unicode codepoint in the range of
   "low" (trailing) UTF-16 surrogate pairs
   (C>=0xDC00) && (C<=0xDFFF) */
bool
is_utf16_surrogate_low (ucs4_t c) _GL_ATTRIBUTE_CONST;

/* Return the unicode codepoint (UCS4) corresponding to
   the given UTF-16 surrogate pair.
   (No input validation is performed) */
ucs4_t
utf16_surrogate_to_ucs4 (wchar_t h, wchar_t l) _GL_ATTRIBUTE_CONST;

/* Return the UTF-16 surrogate pair from a given UCS4 character.
   No input validation is performed.
   Results are undefined if C is less than U+10000 and does not
   require two UTF-16 surrogate values */
void
ucs4_to_utf16_surrogate_pair (ucs4_t c, wchar_t /*OUT*/ *h, wchar_t /*OUT*/ *l);

/* Similar to mbrtowc(3), but assumes 'wchar_t==uint16'
   and performs additional processing to handle UTF-16 surrogate pairs.

   NOTE:
   This function is *not* restartable, and does not keep state information
   between invocations. The entire multibyte-sequence must be supplied.

   Calling this function in other locales or incompatible
   implementations is undefined (e.g. libc implementations in which
   wchar_t!=utf16 and do not use UTF-16 surrogates internally, or
   non unicode wchar_t).
*/
size_t
mbtowc_utf16(ucs4_t /*out*/ *wc, const char *restrict s, size_t n);


/* Return TRUE if the NUL-terminated input string
   results in one (and only one) multibyte character
   in the currently active locale.

   NOTE:
   1. If the current locale is single-byte (MB_CUR_MAX==1),
      and the input string is 1 octet long, returns TRUE
      regardless of the octet's value.
   2. If the current locale is multibyte (MB_CUR_MAX>1),
      even single-octet input strings are checked for validity,
      e.g. "\x90" is *not* a valid character under
      UTF-8 locales and will return FALSE.
   3. Empty strings or NULL input result in FALSE.
*/
bool
is_valid_mb_character (const char* s, uint32_t /*out*/ *wc);
