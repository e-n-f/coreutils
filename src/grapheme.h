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

#if !defined GRAPHEME_H
# define GRAPHEME_H

#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>
#include <stdbool.h>

typedef struct grapheme {
  wint_t c;
  bool isbyte;
} grapheme;

grapheme fgetgr(FILE *f, mbstate_t *mbs);
grapheme fgetgr_count(FILE *f, mbstate_t *mbs, size_t *count);
grapheme fputgr(grapheme c, FILE *f);
grapheme putgrapheme(grapheme c);
grapheme getgrapheme(mbstate_t *mbs);
grapheme fpeekgr(FILE *f, mbstate_t *mbs);
wchar_t fputwcgr (wchar_t c, FILE *f);

size_t grslen (const grapheme *s);
grapheme *grsdup (const grapheme *s);
grapheme *grmemchr (grapheme *, wchar_t, size_t);
grapheme grapheme_wchar (wchar_t c);
grapheme grapheme_byte (unsigned char c);

void mbstogrs(grapheme *out, const char *in);

grapheme grpeek (const char **s, const char *end, mbstate_t *state);
grapheme grnext (const char **s, const char *end, mbstate_t *state);
grapheme grafter (const char **s, const char *end, mbstate_t *state);

#endif
