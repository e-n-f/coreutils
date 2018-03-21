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
