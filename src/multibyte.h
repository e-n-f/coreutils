#if !defined MULTIBYTE_H
# define MULTIBYTE_H

#include <stdio.h>
#include <stddef.h>
#include <wchar.h>
#include <wctype.h>

typedef struct cb {
	wint_t c;
	bool isbyte;
} cb;

cb fgetcb(FILE *f, mbstate_t *mbs);
cb fputcb(cb c, FILE *f);
cb putcbyte(cb c);
cb getcbyte(mbstate_t *mbs);
cb fpeekcb(FILE *f, mbstate_t *mbs);

/* A 'struct linebuffer' holds a line of multibyte text. */

struct cblinebuffer
{
  size_t size;                  /* Allocated. */
  size_t length;                /* Used. */
  cb *buffer;
};

/* Initialize cblinebuffer LINEBUFFER for use. */

void initcbbuffer (struct cblinebuffer *linebuffer);

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
   Consider lines to be terminated by DELIMITER.
   Keep the delimiter; append DELIMITER if we reach EOF and it wasn't
   the last character in the file.  Do not NUL-terminate.
   Return LINEBUFFER, except at end of file return NULL.  */

struct cblinebuffer *readcblinebuffer_delim (struct cblinebuffer *linebuffer,
                                         FILE *stream, wchar_t delimiter, mbstate_t *mbs);

int xwmemcoll (wchar_t *, size_t, wchar_t *, size_t);
int xcbmemcoll (cb *, size_t, cb *, size_t);
cb *cbmemchr (cb *, wchar_t, size_t);

const char *wquote (const wchar_t *s);
const char *cbnstr (const cb *s, size_t n);

extern wchar_t *xwcsndup (const wchar_t *string, size_t n);

ssize_t cbgetndelim2 (cb **lineptr, size_t *linesize, size_t offset, size_t nmax,
                     wchar_t delim1, wchar_t delim2, FILE *stream, mbstate_t *mbs);

wchar_t *xwcsdup (wchar_t const *string);

/* '\n' is considered a field separator with  --zero-terminated.  */
static inline bool
wfield_sep (wchar_t ch)
{
  return iswblank (ch) || ch == L'\n';
}

#define WSTREQ_LEN(a, b, n) (wcsncmp (a, b, n) == 0)

typedef enum mb_error {
  MB_OK,
  MB_EOF,
  MB_ERROR,
  MB_INCOMPLETE,
} mb_error;

mb_error mbrnext0(wchar_t *c, const char **s, const char *end, mbstate_t *state);

mb_error mbrpeek0(wchar_t *c, const char **s, const char *end, mbstate_t *state);

int wstrnumcmp (char const *, char const *, wint_t, wint_t);

#endif
