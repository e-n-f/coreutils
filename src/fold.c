/* fold -- wrap each input line to fit in specified width.
   Copyright (C) 1991-2017 Free Software Foundation, Inc.

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

/* Written by David MacKenzie, djm@gnu.ai.mit.edu. */

#include <config.h>

#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>

#include "system.h"
#include "die.h"
#include "error.h"
#include "fadvise.h"
#include "multibyte.h"
#include "mbbuffer.h"
#include "xdectoint.h"

#define TAB_WIDTH 8

/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "fold"

#define AUTHORS proper_name ("David MacKenzie")

/* If nonzero, try to break on whitespace. */
static bool break_spaces;

/* If nonzero, count bytes, not column positions. */
static bool count_bytes;

/* If nonzero, at least one of the files we read was standard input. */
static bool have_read_stdin;

/* The current input file */
static FILE *istream;

/* Screen column where next char will go. */
static size_t column = 0;

/* Index in 'line_out' for next char. */
static size_t offset_out = 0;

/* The offset in line_out of the last 'blank' character.
   Used with "-s" instead of scanning the string backwards. */
static size_t last_blank_offset = 0;

/* The value of 'column' when the last blank was found. */
static size_t last_blank_column = 0;

/* Buffer holding the currently loaded line */
static char *line_out = NULL;

/* Allocated size of 'line_out' */
static size_t allocated_out = 0;


static char const shortopts[] = "bsw:0::1::2::3::4::5::6::7::8::9::";

static struct option const longopts[] =
{
  {"bytes", no_argument, NULL, 'b'},
  {"spaces", no_argument, NULL, 's'},
  {"width", required_argument, NULL, 'w'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("\
Usage: %s [OPTION]... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Wrap input lines in each FILE, writing to standard output.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  -b, --bytes         count bytes rather than columns\n\
  -s, --spaces        break at spaces\n\
  -w, --width=WIDTH   use WIDTH columns instead of 80\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

/* Assuming the current column is COLUMN, return the column that
   printing C will move the cursor to.
   The first column is 0. */

static size_t
adjust_column (size_t clmn, char c)
{
  if (!count_bytes)
    {
      if (c == '\b')
        {
          if (clmn > 0)
            clmn--;
        }
      else if (c == '\r')
        clmn = 0;
      else if (c == '\t')
        clmn += TAB_WIDTH - clmn % TAB_WIDTH;
      else /* if (isprint (c)) */
        clmn++;
    }
  else
    clmn++;
  return clmn;
}

static bool
open_stream (char const* filename)
{
  if (STREQ (filename, "-"))
    {
      istream = stdin;
      have_read_stdin = true;
    }
  else
    istream = fopen (filename, "r");

  if (istream == NULL)
    {
      error (0, errno, "%s", quotef (filename));
      return false;
    }

  fadvise (istream, FADVISE_SEQUENTIAL);
  return true;
}


static bool
close_stream (char const* filename, int saved_errno)
{
  if (ferror (istream))
    {
      error (0, saved_errno, "%s", quotef (filename));
      if (!STREQ (filename, "-"))
        fclose (istream);
      return false;
    }
  if (!STREQ (filename, "-") && fclose (istream) == EOF)
    {
      error (0, errno, "%s", quotef (filename));
      return false;
    }

  return true;
}

static void
write_current_line (bool add_newline)
{
  if (add_newline)
    line_out[offset_out++] = '\n';
  fwrite (line_out, sizeof (char), offset_out, stdout);
  column = offset_out = last_blank_offset = last_blank_column = 0;
}

static void
fold_text (FILE *input, size_t width, int *saved_errno)
{
  int c;
  while ((c = getc (input)) != EOF)
    {
      if (offset_out + 1 >= allocated_out)
        line_out = X2REALLOC (line_out, &allocated_out);

      if (c == '\n')
        {
          write_current_line (true);
          continue;
        }

    rescan:
      column = adjust_column (column, c);

      if (column > width)
        {
          /* This character would make the line too long.
             Print the line plus a newline, and make this character
             start the next line. */
          if (break_spaces)
            {
              bool found_blank = false;
              size_t logical_end = offset_out;

              /* Look for the last blank. */
              while (logical_end)
                {
                  --logical_end;
                  if (isblank (to_uchar (line_out[logical_end])))
                    {
                      found_blank = true;
                      break;
                    }
                }

              if (found_blank)
                {
                  size_t i;

                  /* Found a blank.  Don't output the part after it. */
                  logical_end++;
                  fwrite (line_out, sizeof (char), (size_t) logical_end,
                          stdout);
                  putchar ('\n');
                  /* Move the remainder to the beginning of the next line.
                     The areas being copied here might overlap. */
                  memmove (line_out, line_out + logical_end,
                           offset_out - logical_end);
                  offset_out -= logical_end;
                  for (column = i = 0; i < offset_out; i++)
                    column = adjust_column (column, line_out[i]);
                  goto rescan;
                }
            }

          if (offset_out == 0)
            {
              line_out[offset_out++] = c;
              continue;
            }

          write_current_line (true);
          goto rescan;
        }

      line_out[offset_out++] = c;
    }

  *saved_errno = errno;

  if (offset_out)
    write_current_line (false);
}

/* Limited multibyte implementation:
   1. special characters (\b \r \t) are handled as in the ASCII case.
   2. invalid multibyte sequence is treated as 1 byte/character.
   3. Any valid multibyte sequence is still treated as
      character consuming width of 1.

   Future improvement could use wcwidth(3) to check the real width,
   but in many implementations wcwidth(3) does not return the actual
   visual width consumed by the character (sometimes libc can't know
   in advance how many spots will be consumed - only the Xterm can do it).

   Also consider using iswprint(3).
*/
static size_t
adjust_column_multibyte (size_t col, const struct mbbuf *mbb)
{
  /* This relies on the fact the 'adjust_column' does not check
     the value of the chracter (e.g. no isblank or isprint).
     Otherwise the content of mbb->wc needs to be checked.  */
  const char c = (mbb->wc=='\b'||mbb->wc=='\t'||mbb->wc=='\r')
    ? ((char)mbb->wc) : 'A';
  return adjust_column (col, c);
}


static void
fold_multibyte_text (FILE *input, size_t width, int *saved_errno)
{
  struct mbbuf mbb;
  mbbuf_init (&mbb, BUFSIZ);

  while (mbbuf_getchar (&mbb, input))
    {
      if (offset_out + mbb.mb_len >= allocated_out)
        line_out = X2REALLOC (line_out, &allocated_out);

      if (mbb.mb_valid && mbb.wc == '\n')
        {
          write_current_line (true);
          continue;
        }

    rescan:
      column = adjust_column_multibyte (column, &mbb);

      if (column > width)
        {
          /* This character would make the line too long.
             Print the line plus a newline, and make this character
             start the next line. */
          if (break_spaces)
            {
              if (last_blank_offset > 0)
                {
                  const size_t logical_end = last_blank_offset ;

                  /* Found a blank.  Don't output the part after it. */
                  fwrite (line_out, sizeof (char), (size_t) logical_end,
                          stdout);
                  putchar ('\n');

                  /* Move the remainder to the beginning of the next line.
                     The areas being copied here might overlap. */
                  memmove (line_out, line_out + logical_end,
                           offset_out - logical_end);

                  offset_out -= last_blank_offset;
                  column -= last_blank_column;
                  last_blank_offset = 0 ;
                  last_blank_column = 0 ;
                  goto rescan;
                }
            }

          if (offset_out == 0)
            {
              /* Add the multibyte sequence to the output buffer */
              memmove (line_out + offset_out, mbb.mb_str, mbb.mb_len);
              offset_out += mbb.mb_len ;
              continue;
            }

          write_current_line (true);
          goto rescan;
        }

      /* Add the multibyte sequence to the output buffer.
         TODO: optimize for more common single-bytes? */
      memmove (line_out + offset_out, mbb.mb_str, mbb.mb_len);
      offset_out += mbb.mb_len ;

      /* Keep track of the last encountered blank character */
      if (break_spaces)
        {
          if (iswblank (mbb.wc))
            {
              last_blank_offset = offset_out;
              last_blank_column = column;
            }
        }
    }

  *saved_errno = errno;

  if (offset_out)
    write_current_line (false);
}


/* Fold file FILENAME, or standard input if FILENAME is "-",
   to stdout, with maximum line length WIDTH.
   Return true if successful.  */
static bool
fold_file (char const *filename, size_t width)
{
  int saved_errno;

  if (!open_stream (filename))
    return false;

  if (use_multibyte ())
    fold_multibyte_text (istream, width, &saved_errno);
  else
    fold_text (istream, width, &saved_errno);



  return close_stream (filename, saved_errno);
}

int
main (int argc, char **argv)
{
  size_t width = 80;
  int i;
  int optc;
  bool ok;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  break_spaces = count_bytes = have_read_stdin = false;

  while ((optc = getopt_long (argc, argv, shortopts, longopts, NULL)) != -1)
    {
      char optargbuf[2];

      switch (optc)
        {
        case 'b':		/* Count bytes rather than columns. */
          count_bytes = true;
          break;

        case 's':		/* Break at word boundaries. */
          break_spaces = true;
          break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          if (optarg)
            optarg--;
          else
            {
              optargbuf[0] = optc;
              optargbuf[1] = '\0';
              optarg = optargbuf;
            }
          FALLTHROUGH;
        case 'w':		/* Line width. */
          width = xdectoumax (optarg, 1, SIZE_MAX - TAB_WIDTH - 1, "",
                              _("invalid number of columns"), 0);
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (argc == optind)
    ok = fold_file ("-", width);
  else
    {
      ok = true;
      for (i = optind; i < argc; i++)
        ok &= fold_file (argv[i], width);
    }

  if (have_read_stdin && fclose (stdin) == EOF)
    die (EXIT_FAILURE, errno, "-");

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
