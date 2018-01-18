/* cut - remove parts of lines of files
   Copyright (C) 1997-2017 Free Software Foundation, Inc.
   Copyright (C) 1984 David M. Ihnat

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

/* Written by David Ihnat.  */

/* POSIX changes, bug fixes, long-named options, and cleanup
   by David MacKenzie <djm@gnu.ai.mit.edu>.

   Rewrite cut_fields and cut_bytes -- Jim Meyering.  */

#include <config.h>

#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>
#include "system.h"

#include "error.h"
#include "fadvise.h"
#include "getndelim2.h"
#include "hash.h"
#include "xstrndup.h"
#include "xalloc.h"
#include "multibyte.h"

#include "set-fields.h"

/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "cut"

#define AUTHORS \
  proper_name ("David M. Ihnat"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Jim Meyering"), \
  proper_name ("Eric Fischer")

#define FATAL_ERROR(Message)						\
  do									\
    {									\
      error (0, 0, (Message));						\
      usage (EXIT_FAILURE);						\
    }									\
  while (0)


/* Pointer inside RP.  When checking if a byte or field is selected
   by a finite range, we check if it is between CURRENT_RP.LO
   and CURRENT_RP.HI.  If the byte or field index is greater than
   CURRENT_RP.HI then we make CURRENT_RP to point to the next range pair. */
static struct field_range_pair *current_rp;

/* This buffer is used to support the semantics of the -s option
   (or lack of same) when the specified field list includes (does
   not include) the first field.  In both of those cases, the entire
   first field must be read into this buffer to determine whether it
   is followed by a delimiter or a newline before any of it may be
   output.  Otherwise, cut_fields can do the job without using this
   buffer.  */
static grapheme *field_1_buffer;

/* The number of bytes allocated for FIELD_1_BUFFER.  */
static size_t field_1_bufsize;

enum operating_mode
  {
    undefined_mode,

    /* Output characters that are in the given bytes. */
    byte_mode,

    /* Output characters that are in the given characters. */
    character_mode,

    /* Output characters that are in the given characters, indexed by bytes. */
    character_byte_mode,

    /* Output the given delimiter-separated fields. */
    field_mode
  };

static enum operating_mode operating_mode;

/* If true do not output lines containing no delimiter characters.
   Otherwise, all such lines are printed.  This option is valid only
   with field mode.  */
static bool suppress_non_delimited;

/* If true, print all bytes, characters, or fields _except_
   those that were specified.  */
static bool complement;

/* The delimiter character for field mode. */
static grapheme delim;

/* The delimiter for each line/record. */
static unsigned char line_delim_byte = '\n';
static wchar_t line_delim_wchar = L'\n';

/* True if the --output-delimiter=STRING option was specified.  */
static bool output_delimiter_specified;

/* The length of output_delimiter_string.  */
static size_t output_delimiter_length;

/* The output field separator string.  Defaults to the 1-character
   string consisting of the input delimiter.  */
static const grapheme *output_delimiter_string;

/* True if we have ever read standard input. */
static bool have_read_stdin;

/* For long options that have no equivalent short option, use a
   non-character as a pseudo short option, starting with CHAR_MAX + 1.  */
enum
{
  OUTPUT_DELIMITER_OPTION = CHAR_MAX + 1,
  COMPLEMENT_OPTION
};

static struct option const longopts[] =
{
  {"bytes", required_argument, NULL, 'b'},
  {"characters", required_argument, NULL, 'c'},
  {"no-character-splitting", no_argument, NULL, 'n'},
  {"fields", required_argument, NULL, 'f'},
  {"delimiter", required_argument, NULL, 'd'},
  {"only-delimited", no_argument, NULL, 's'},
  {"output-delimiter", required_argument, NULL, OUTPUT_DELIMITER_OPTION},
  {"complement", no_argument, NULL, COMPLEMENT_OPTION},
  {"zero-terminated", no_argument, NULL, 'z'},
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
Usage: %s OPTION... [FILE]...\n\
"),
              program_name);
      fputs (_("\
Print selected parts of lines from each FILE to standard output.\n\
"), stdout);

      emit_stdin_note ();
      emit_mandatory_arg_note ();

      fputs (_("\
  -b, --bytes=LIST        select only these bytes\n\
  -c, --characters=LIST   select only these characters\n\
  -d, --delimiter=DELIM   use DELIM instead of TAB for field delimiter\n\
"), stdout);
      fputs (_("\
  -f, --fields=LIST       select only these fields;  also print any line\n\
                            that contains no delimiter character, unless\n\
                            the -s option is specified\n\
  -n                      (ignored)\n\
"), stdout);
      fputs (_("\
      --complement        complement the set of selected bytes, characters\n\
                            or fields\n\
"), stdout);
      fputs (_("\
  -s, --only-delimited    do not print lines not containing delimiters\n\
      --output-delimiter=STRING  use STRING as the output delimiter\n\
                            the default is to use the input delimiter\n\
"), stdout);
      fputs (_("\
  -z, --zero-terminated    line delimiter is NUL, not newline\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      fputs (_("\
\n\
Use one, and only one of -b, -c or -f.  Each LIST is made up of one\n\
range, or many ranges separated by commas.  Selected input is written\n\
in the same order that it is read, and is written exactly once.\n\
"), stdout);
      fputs (_("\
Each range is one of:\n\
\n\
  N     N'th byte, character or field, counted from 1\n\
  N-    from N'th byte, character or field, to end of line\n\
  N-M   from N'th to M'th (included) byte, character or field\n\
  -M    from first to M'th (included) byte, character or field\n\
"), stdout);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}


/* Increment *ITEM_IDX (i.e., a field or byte index),
   and if required CURRENT_RP.  */

static inline void
next_item (size_t *item_idx)
{
  (*item_idx)++;
  if ((*item_idx) > current_rp->hi)
    current_rp++;
}

/* Return nonzero if the K'th field or byte is printable. */

static inline bool
print_kth (size_t k)
{
  return current_rp->lo <= k;
}

/* Return nonzero if K'th byte is the beginning of a range. */

static inline bool
is_range_start_index (size_t k)
{
  return k == current_rp->lo;
}

/* Read from stream STREAM, printing to standard output any selected bytes.  */

static void
cut_bytes (FILE *stream)
{
  size_t byte_idx;	/* Number of bytes in the line so far. */
  /* Whether to begin printing delimiters between ranges for the current line.
     Set after we've begun printing data corresponding to the first range.  */
  bool print_delimiter;

  byte_idx = 0;
  print_delimiter = false;
  current_rp = frp;
  while (true)
    {
      int c;		/* Each character from the file. */

      c = getc (stream);

      if (c == line_delim_byte)
        {
          putchar (c);
          byte_idx = 0;
          print_delimiter = false;
          current_rp = frp;
        }
      else if (c == EOF)
        {
          if (byte_idx > 0)
            putchar (line_delim_byte);
          break;
        }
      else
        {
          next_item (&byte_idx);
          if (print_kth (byte_idx))
            {
              if (output_delimiter_specified)
                {
                  if (print_delimiter && is_range_start_index (byte_idx))
                    {
                      for (size_t i = 0; i < output_delimiter_length; i++)
                        fputgr (output_delimiter_string[i], stdout);
                    }
                  print_delimiter = true;
                }

              putchar (c);
            }
        }
    }
}

/* Read from stream STREAM, printing to standard output any selected characters.  */

static void
cut_characters (FILE *stream, bool use_bytes)
{
  size_t character_idx;	/* Number of bytes in the line so far. */
  /* Whether to begin printing delimiters between ranges for the current line.
     Set after we've begun printing data corresponding to the first range.  */
  bool print_delimiter;

  character_idx = 0;
  print_delimiter = false;
  current_rp = frp;
  mbstate_t mbs = { 0 };

  while (true)
    {
      grapheme c;		/* Each character from the file. */
      size_t count;

      c = fgetgr_count (stream, &mbs, &count);
      if (!use_bytes)
        count = 1;

      if (c.c == line_delim_wchar)
        {
          putgrapheme (c);
          character_idx = 0;
          print_delimiter = false;
          current_rp = frp;
        }
      else if (c.c == WEOF)
        {
          if (character_idx > 0)
            fputwcgr (line_delim_wchar, stdout);
          break;
        }
      else
        {
          for (size_t i = 0; i < count; i++)
            next_item (&character_idx);
          if (print_kth (character_idx))
            {
              if (output_delimiter_specified)
                {
                  if (print_delimiter && is_range_start_index (character_idx))
                    {
                      for (size_t i = 0; i < output_delimiter_length; i++)
                        fputgr (output_delimiter_string[i], stdout);
                    }
                  print_delimiter = true;
                }

              putgrapheme (c);
            }
        }
    }
}

/* Read from stream STREAM, printing to standard output any selected fields.  */

static void
cut_fields (FILE *stream)
{
  grapheme c;
  size_t field_idx = 1;
  bool found_any_selected_field = false;
  bool buffer_first_field;
  mbstate_t mbs = { 0 };

  current_rp = frp;

  c = fpeekgr (stream, &mbs);
  if (c.c == WEOF)
    return;

  c.c = L'\0';

  /* To support the semantics of the -s flag, we may have to buffer
     all of the first field to determine whether it is 'delimited.'
     But that is unnecessary if all non-delimited lines must be printed
     and the first field has been selected, or if non-delimited lines
     must be suppressed and the first field has *not* been selected.
     That is because a non-delimited line has exactly one field.  */
  buffer_first_field = (suppress_non_delimited ^ !print_kth (1));

  while (1)
    {
      if (field_idx == 1 && buffer_first_field)
        {
          ssize_t len;
          size_t n_bytes;

          len = grgetndelim2 (&field_1_buffer, &field_1_bufsize, 0,
                            GETNLINE_NO_LIMIT, delim.c, line_delim_wchar, stream, &mbs);
          if (len < 0)
            {
              free (field_1_buffer);
              field_1_buffer = NULL;
              if (ferror (stream) || feof (stream))
                break;
              xalloc_die ();
            }

          n_bytes = len;
          assert (n_bytes != 0);

          c.c = L'\0';

          /* If the first field extends to the end of line (it is not
             delimited) and we are printing all non-delimited lines,
             print this one.  */
          if (field_1_buffer[n_bytes - 1].c != delim.c)
            {
              if (suppress_non_delimited)
                {
                  /* Empty.  */
                }
              else
                {
                  for (size_t i = 0; i < n_bytes; i++)
                    putgrapheme (field_1_buffer[i]);
                  /* Make sure the output line is newline terminated.  */
                  if (field_1_buffer[n_bytes - 1].c != line_delim_wchar)
                    fputwcgr (line_delim_wchar, stdout);
                  c = grapheme_wchar (line_delim_wchar);
                }
              continue;
            }
          if (print_kth (1))
            {
              /* Print the field, but not the trailing delimiter.  */
              for (size_t i = 0; i < n_bytes - 1; i++)
                putgrapheme (field_1_buffer[i]);

              /* With -d$'\n' don't treat the last '\n' as a delimiter.  */
              if (delim.c == line_delim_wchar)
                {
                  grapheme last_c = fpeekgr (stream, &mbs);
                  if (last_c.c != WEOF)
                    {
                      found_any_selected_field = true;
                    }
                }
              else
                found_any_selected_field = true;
            }
          next_item (&field_idx);
        }

      grapheme prev_c = c;

      if (print_kth (field_idx))
        {
          if (found_any_selected_field)
            {
              for (size_t i = 0; i < output_delimiter_length; i++)
                fputgr (output_delimiter_string[i], stdout);
            }
          found_any_selected_field = true;

          while ((c = fgetgr (stream, &mbs)).c != delim.c && c.c != line_delim_wchar && c.c != WEOF)
            {
              putgrapheme (c);
              prev_c = c;
            }
        }
      else
        {
          while ((c = fgetgr (stream, &mbs)).c != delim.c && c.c != line_delim_wchar && c.c != WEOF)
            {
              prev_c = c;
            }
        }

      /* With -d$'\n' don't treat the last '\n' as a delimiter.  */
      if (delim.c == line_delim_wchar && c.c == delim.c)
        {
          grapheme last_c = fpeekgr (stream, &mbs);
          if (last_c.c == WEOF)
            c = last_c;
        }

      if (c.c == delim.c)
        next_item (&field_idx);
      else if (c.c == line_delim_wchar || c.c == WEOF)
        {
          if (found_any_selected_field
              || !(suppress_non_delimited && field_idx == 1))
            {
              if (c.c == line_delim_wchar || prev_c.c != line_delim_wchar
                  || delim.c == line_delim_wchar)
                fputwcgr (line_delim_wchar, stdout);
            }
          if (c.c == WEOF)
            break;
          field_idx = 1;
          current_rp = frp;
          found_any_selected_field = false;
        }
    }
}

static void
cut_stream (FILE *stream)
{
  if (operating_mode == byte_mode)
    cut_bytes (stream);
  else if (operating_mode == character_mode)
    cut_characters (stream, false);
  else if (operating_mode == character_byte_mode)
    cut_characters (stream, true);
  else
    cut_fields (stream);
}

/* Process file FILE to standard output.
   Return true if successful.  */

static bool
cut_file (char const *file)
{
  FILE *stream;

  if (STREQ (file, "-"))
    {
      have_read_stdin = true;
      stream = stdin;
    }
  else
    {
      stream = fopen (file, "r");
      if (stream == NULL)
        {
          error (0, errno, "%s", quotef (file));
          return false;
        }
    }

  fadvise (stream, FADVISE_SEQUENTIAL);

  cut_stream (stream);

  if (ferror (stream))
    {
      error (0, errno, "%s", quotef (file));
      return false;
    }
  if (STREQ (file, "-"))
    clearerr (stream);		/* Also clear EOF. */
  else if (fclose (stream) == EOF)
    {
      error (0, errno, "%s", quotef (file));
      return false;
    }
  return true;
}

int
main (int argc, char **argv)
{
  int optc;
  bool ok;
  bool delim_specified = false;
  char *spec_list_string IF_LINT ( = NULL);
  bool nosplit = false;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  operating_mode = undefined_mode;

  /* By default, all non-delimited lines are printed.  */
  suppress_non_delimited = false;

  delim = grapheme_wchar (L'\0');
  have_read_stdin = false;

  while ((optc = getopt_long (argc, argv, "b:c:d:f:nsz", longopts, NULL)) != -1)
    {
      switch (optc)
        {
        case 'b':
          /* Build the byte list. */
          if (operating_mode != undefined_mode)
            FATAL_ERROR (_("only one type of list may be specified"));
          operating_mode = byte_mode;
          spec_list_string = optarg;
          break;

        case 'c':
          /* Build the character list. */
          if (operating_mode != undefined_mode)
            FATAL_ERROR (_("only one type of list may be specified"));
          operating_mode = character_mode;
          spec_list_string = optarg;
          break;

        case 'f':
          /* Build the field list. */
          if (operating_mode != undefined_mode)
            FATAL_ERROR (_("only one type of list may be specified"));
          operating_mode = field_mode;
          spec_list_string = optarg;
          break;

        case 'd':
          /* New delimiter. */
          /* Interpret -d '' to mean 'use the NUL byte as the delimiter.'  */
          if (optarg[0] == '\0')
            {
              delim = grapheme_wchar (L'\0');
              delim_specified = true;
            }
          else
            {
              mbstate_t mbs = { 0 };
              const char *s = optarg;
              delim = grnext(&s, s + strlen(s), &mbs);
              if (delim.c == WEOF || *s != '\0')
                FATAL_ERROR (_("the delimiter must be a single character."));
              delim_specified = true;
            }
          break;

        case OUTPUT_DELIMITER_OPTION:
          output_delimiter_specified = true;
          /* Interpret --output-delimiter='' to mean
             'use the NUL byte as the delimiter.'  */
          if (optarg[0] == '\0')
            {
              static grapheme d[2];
              d[0] = grapheme_wchar (L'\0');
              d[1] = grapheme_wchar (L'\0');
              output_delimiter_string = d;
              output_delimiter_length = 1;
            }
          else
            {
              grapheme tmp[strlen(optarg) + 1];
              mbstogrs(tmp, optarg);
              output_delimiter_length = grslen(tmp);
              output_delimiter_string = grsdup(tmp);
            }
          break;

        case 'n':
          nosplit = true;
          break;

        case 's':
          suppress_non_delimited = true;
          break;

        case 'z':
          line_delim_byte = '\0';
          line_delim_wchar = L'\0';
          break;

        case COMPLEMENT_OPTION:
          complement = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (operating_mode == byte_mode && nosplit)
    operating_mode = character_byte_mode;

  if (operating_mode == undefined_mode)
    FATAL_ERROR (_("you must specify a list of bytes, characters, or fields"));

  if (delim_specified && operating_mode != field_mode)
    FATAL_ERROR (_("an input delimiter may be specified only\
 when operating on fields"));

  if (suppress_non_delimited && operating_mode != field_mode)
    FATAL_ERROR (_("suppressing non-delimited lines makes sense\n\
\tonly when operating on fields"));

  set_fields (spec_list_string,
              ( (operating_mode == field_mode) ? 0 : SETFLD_ERRMSG_USE_POS)
              | (complement ? SETFLD_COMPLEMENT : 0) );

  if (!delim_specified)
    delim = grapheme_wchar (L'\t');

  if (output_delimiter_string == NULL)
    {
      static grapheme d[2];
      d[0] = delim;
      d[1] = grapheme_wchar (L'\0');
      output_delimiter_string = d;
      output_delimiter_length = 1;
    }

  if (optind == argc)
    ok = cut_file ("-");
  else
    for (ok = true; optind < argc; optind++)
      ok &= cut_file (argv[optind]);


  if (have_read_stdin && fclose (stdin) == EOF)
    {
      error (0, errno, "-");
      ok = false;
    }

  IF_LINT (reset_fields ());

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
