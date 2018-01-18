#if !defined MULTIBYTE_H
# define MULTIBYTE_H

#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>

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

/* A 'struct linebuffer' holds a line of multibyte text. */

struct grlinebuffer
{
  size_t size;                  /* Allocated. */
  size_t length;                /* Used. */
  grapheme *buffer;
};

/* Initialize grlinebuffer LINEBUFFER for use. */

void initgrbuffer (struct grlinebuffer *linebuffer);

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
   Consider lines to be terminated by DELIMITER.
   Keep the delimiter; append DELIMITER if we reach EOF and it wasn't
   the last character in the file.  Do not NUL-terminate.
   Return LINEBUFFER, except at end of file return NULL.  */

struct grlinebuffer *readgrlinebuffer_delim (struct grlinebuffer *linebuffer,
                                         FILE *stream, wchar_t delimiter, mbstate_t *mbs);

int xwmemcoll (wchar_t *, size_t, wchar_t *, size_t);
int xgrmemcoll (grapheme *, size_t, grapheme *, size_t);
grapheme *grmemchr (grapheme *, wchar_t, size_t);

const char *wquote (const wchar_t *s);
const char *grnstr (const grapheme *s, size_t n);

extern wchar_t *xwcsndup (const wchar_t *string, size_t n);

ssize_t grgetndelim2 (grapheme **lineptr, size_t *linesize, size_t offset, size_t nmax,
                     wchar_t delim1, wchar_t delim2, FILE *stream, mbstate_t *mbs);

wchar_t *xwcsdup (wchar_t const *string);

/* '\n' is considered a field separator with  --zero-terminated.  */
static inline bool
wfield_sep (wchar_t ch)
{
  return iswblank (ch) || ch == L'\n';
}

#define WSTREQ_LEN(a, b, n) (wcsncmp (a, b, n) == 0)

grapheme grpeek (const char **s, const char *end, mbstate_t *state);
grapheme grnext (const char **s, const char *end, mbstate_t *state);
grapheme grafter (const char **s, const char *end, mbstate_t *state);

int wstrnumcmp (char const *, char const *, wint_t, wint_t);

int charwidth (wchar_t c);

size_t grslen (const grapheme *s);
grapheme *grsdup (const grapheme *s);
grapheme grapheme_wchar (wchar_t c);
grapheme grapheme_byte (unsigned char c);

void mbstogrs(grapheme *out, const char *in);

#endif
