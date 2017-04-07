Text
====

The first 4 definitions from Euclid's "Elements", book 1:

http://aleph0.clarku.edu/~djoyce/java/elements/bookI/bookI.html


A point is that which has no part
Σημεῖόν ἐστιν οὗ μέρος οὐθέν

$semeion = "\u03A3\u03B7\u03BC\u03B5\u1FD6\u03CC\u03B" ; # Σημεῖόν / point
$estin   = "\u1F10\u03C3\u03C4\u03B9\u03BD" ;            # ἐστιν   / is
$hoo     = "\u03BF\u1F57" ;                              # οὗ      / where
$meros    = "\u03BC\u03AD\u03C1\u03BF\u03C2" ;           # μέρος   / part
$ouden    = "\u03BF\u1F50\u03B8\u03AD\u03BD";            # οὐθέν   / nothing





General
======

List of unicode letters in different languages:
http://www.ltg.ed.ac.uk/~richard/unicode-sample.html

UTF-8 Sampler:
http://www.columbia.edu/~fdc/utf8/


UTf8 Everywhere:
http://utf8everywhere.org/

http://www.i18nguy.com/unicode/unicode-example-intro.html

Unicode Toys (online conversion):
http://qaz.wtf/u/


OpenBSD removes non-utf8 locales:
  http://marc.info/?l=openbsd-cvs&m=143956261214725&w=2

http://unix.stackexchange.com/questions/90100/convert-between-unicode-normalization-forms-on-the-unix-command-line

Send updates about 'unorm',
but also coreutils' printf "\u" \U" to:
  https://www.cl.cam.ac.uk/~mgk25/unicode.html

Build "unicode.housegordon.org"
pointing to many resources:
  http://www.fileformat.info/info/unicode/char/FB01/index.htm
  https://codepoints.net/U+FB01
  http://unicode.org/cldr/utility/character.jsp?a=FB01

Fantastic Gift Idea: Adopt a Unicode Character!
  http://www.unicode.org/consortium/adopted-characters.html

GNU UniFont + Unicode tutorial:
  http://unifoundry.com/index.html
  http://unifoundry.com/unicode-tutorial.html

TODO: UTF-16 Surrogates:
  1. printf can generate them, but on wchar_t/64-bit systems,
     mbrtowc(3) can't decode them:
       $ printf '\ud800\n' | iconv -f utf-8
       iconv: illegal input sequence at position 0

  2. A file containing:
       printf '\ud800\udc000\n' > 1.txt
     will be interpreted as 6 invalid octets on 64bit systems,
     and as either 'U+100000' or 'U+D800 U+DC00' on cygwin.
     which is correct ?

  3. On Cygwin, this input can be detected (and rejected to maintain
     consistency) by checking mbstate_t.__count==4 .
     What about other systems ?

TODO:
   wcwidth() on chars in SMP returns -1 on Glibc, but 1 on Mac OS X?
   (checked in multibyte-test).

   wcwith() on LATIN-EXtended-B digraph characters,e.g:
      \u01c4 Latin Capital Letter DZ with caron
   see 'expand' section.


Converting UTF to \U sequences (using 'uconv' from ICU packagE):

    $ printf "Σημεῖόν" | uconv -x 'hex-any ; any-hex' ; echo
    \u03A3\u03B7\u03BC\u03B5\u1FD6\u03CC\u03BD

Converting UTF-8 to named unicode charactes:

    $ printf "Σημεῖόν" | uconv -x 'hex-any ; any-name' ; echo
    \N{GREEK CAPITAL LETTER SIGMA}\N{GREEK SMALL LETTER ETA}
    \N{GREEK SMALL LETTER MU}\N{GREEK SMALL LETTER EPSILON}
    \N{GREEK SMALL LETTER IOTA WITH PERISPOMENI}
    \N{GREEK SMALL LETTER OMICRON WITH TONOS}\N{GREEK SMALL LETTER NU}

List of possible transliterations in 'uconv'/ICU:

     uconv -L | tr ' ' '\n' | grep -i any | sort -f |  less

eg.
    uconv -x 'hex-any ; any-hex/perl'
    uconv -x 'hex-any ; any-hex/java'
    uconv -x 'hex-any ; any-hex/c'


Print unicode:

   perl -e 'print "\x{3A3}\n"
   perl -e 'print "\N{U+03A3}\n"
   perl -e 'print "\N{GREEK CAPITAL LETTER SIGMA}\n"

   printf '\u03A3\n'
   printf '\U000003A3\n'






Show different unicode font implementation/support in terminals:

1. Easy case: 'e' + combining mark (where a pre-combined 'e' exists):

     $ printf 'e\u0301\n'
     é

   Works on gnome-terminal, mac-os-x-terminal, xterm.
   doesn't work on 'st' (simple-terminal from st.suckless.org),
   prints 'e' followed by empty 'grave'.

2. Advanced support: any letter (regardless of pre-combined letter
   support:

      $ printf 'x\u0301e\n'

   On gnome-terminal,mac-os-terminal, prints 'x' with grave
   (nonsensical, but graphically correct) followed by 'b'.

   On xterm, simple-term: prints 'xe'

TODO:
learn from grep's multibyte-white-space test

Cygwin
======

Cygwin UTF-16 problems:
    https://cygwin.com/ml/cygwin/2011-02/msg00037.html

But iswalpha takes 'wint_t' which IS int32_t - perhaps do conversion
manually?
   https://cygwin.com/ml/cygwin/2011-02/msg00039.html
   https://cygwin.com/ml/cygwin/2011-02/msg00044.html

expand
======

Page 438: 'interlinear' markers in 'expand' ?

TODO: 'expand' fix this:

  likely due to 'wcwidth(\004)==-1' ?? which is a control character ?
    $ printf 'a\034aa\tb\naaa\tb\n' | ./src/expand
    a./src/expand: input line is too long


TODO: Skin-tone modifiers (not zero width, but optionally is if
      it follows an face/hand emoji??)


      https://codepoints.net/U+1F3FB?lang=en

  This is EMOJI MODIFIER FITZPATRICK TYPE-1-2' (U+1F3FB)

    # With space (non-emoji) before the modified,it is a normal character:
      $ printf '\U0001F466 \U0001F3FB\n'
      👦 🏻

    # with an emoji, it is combined:
      $ printf '\U0001F466\U0001F3FB\n'
      👦🏻

   # but it is considered a wcwidth(x)==1 ONLY on MAC OS X

    $ printf '\U0001F466 \U0001F3FB\n' | ./src/mbbuffer-test -r
    ofs  line colB colC V wc(dec) wc(hex) Ch n octets
    0    1    1    1    y  128102 0x1f466 👦   1 4 0xf0 0x9f 0x91 0xa6
    4    1    5    2    y      32 0x00020 =   1 1 0x20
    5    1    6    3    y  127995 0x1f3fb =   1 4 0xf0 0x9f 0x8f 0xbb
    9    1    10   4    y      10 0x0000a =  -1 1 0x0a

    $ printf '\U0001F466\U0001F3FB\n' | ./src/mbbuffer-test -r
    ofs  line colB colC V wc(dec) wc(hex) Ch n octets
    0    1    1    1    y  128102 0x1f466 👦   1 4 0xf0 0x9f 0x91 0xa6
    4    1    5    2    y  127995 0x1f3fb =   1 4 0xf0 0x9f 0x8f 0xbb
    8    1    9    3    y      10 0x0000a =  -1 1 0x0a

  # On GLIBC, wcwidth() of these SMP characters is -1 !!

    Should we use 'wcswidth' , or alternatively,
    Process "EmojiModifiers" propery? 'http://unicode.org/reports/tr51/#Data_Files'
    (but then, the list of possible properies is endless).

Problematic alignment (looks different between Mac OSX terminal and gnome-terminal 3.6.2):

    $ printf 'a\U0001F466\U0001F3FBaa\tb\naaaa\tb\n'
    a👦🏻aa   b
    aaaa    b


TODO:
        Other "Modifier Symbols" (Category Sk):
        https://codepoints.net/search?gc=Sk
8128



TODO: Expand correctly handles zero-width combiners:
   $ printf 'e\u0301aaa\tb\naaaa\tb\n' | ./src/expand
   éaaa    b
   aaaa    b


TODO: joiner such as:
   U+20E0 COMBINING ENCLOSING CIRCLE BACKSLASH, as an overlaid    ⃠, to indicate a prohibition or “NO”
   For example:


  🎙⃠ <U+1F399 U+20E0>no microphones
  📸⃠ <U+1F4F8 U+20E0> no flashes
  🔫⃠ <U+1F52B U+20E0> no guns


    see: http://unicode.org/reports/tr51/#Diversity

TODO:
        !iswgrpah(x) characters will have wcwidth(x)==-1 !! (not zero)


TODO: How many characters are NON-PRINTABLE (i.e. wcwidth()==-1),
      but in expand we do not treat them properly?
      when adding columns in 'expand', ensure wcwidth>0 ??
      Do all "word_break" or "line_break" characters have wcwidth()==-1 ?

TODO: A character that is combining (e.g. should be zero width?)
      but does shift all characters on space further:

      'COMBINING ENCLOSING KEYCAP' (U+20E3)
      http://www.fileformat.info/info/unicode/char/20e3/index.htm

      $ env printf 'a\u20E3aa\tb\naaaa\tb\n'
      a⃣aa     b
      aaaa    b

     $ env printf 'a\u20E3aa\tb\naaaa\tb\n'  | ./src/expand
     a⃣aa     b
     aaaa    b

     $ \printf 'X\u20E3\n' | ./src/mbbuffer-test -r
     ofs  line colB colC V wc(dec) wc(hex) Ch n octets
     0    1    1    1    y      88 0x00058 X   1 1 X
     1    1    2    2    y    8419 0x020e3  ⃣   0 3 0xe2 0x83 0xa3
     4    1    5    3    y      10 0x0000a =  -1 1 0x0a

     TODO: Test if as an entire string, wcswidth can return the correct length ??

TODO:

    check wcwidth() on CJK idograms (does it return 2?)

   wcwidth() on LATIN-EXtended-B  characters:
      \u01c4 Latin Capital Letter DZ with caron
      \u01c5 Latin Capital Letter D with Small Letter Z with caron
      \u01c6 Latin Small Letter DZ with caron
      \u01c7 Latin Capital Letter LJ
      \u01c8 Latin Capital Letter L with Small Letter J
      \u01c9 Latin Small Letter LJ
      \u01ca Latin Capital Letter NJ
      \u01cb Latin Capital Letter N with Small Letter J
      \u01cc Latin Small Letter NJ

      \u01f1 Latin Capital Letter DZ
      \u01f2 Latin Capital Letter D with Small Letter
      \u01f3 Latin Small Letter DZ

      \u1f6 Latin Capital Letter Hwair (seems to work through with wcwidth==1)

      \u01fC Latin Capital Letter AE with acute Ǽ (seems to woth with wcwidth==1)

      \u0238 Latin Small Letter DB Digraph ȸ (seems ok) - despite being called digraphs
      \u0239 Latin Small Letter QP Digraph ȹ (seems ok)

      \u2A3 Latin Small Letter DZ Digraph ʣ - does this translates to DZ, or
            to the Latin-Extended-B 'DZ' latter?
      up to and including:
      \u2AB Latin Small Letter LZ Digraph


   on gnome-terminal, the 'DZ' is rendered as two characters,
   but following character ('a') is rendered ON TOP of the 'Z' -
   the 'DZ' did not consume two spaces:
       $ printf '\u01c4a\n'
       Ǆa


   see https://en.wikipedia.org/wiki/D%C5%BE
       https://en.wikipedia.org/wiki/Latin_Extended-B

   On Ubuntu 14.04/glibc, wcwidth==1 for all these,
   despite being displayed as 2 charactesrs on gnome-terminal
   (but as invalid/empty square on xterm).


What about wcwidth of "full width" characters:
    https://en.wikipedia.org/wiki/Halfwidth_and_fullwidth_forms#In_Unicode
    e.g.
       \uFF21 full-width capital A

   On glibc, wcwidth gives 2 (which is great):

    $ printf '\uff21a\n' | ./src/mbbuffer-test -r
    ofs  line colB colC V wc(dec) wc(hex) Ch  W n octets
    0    1    1    1    y   65313 0x0ff21 Ａ  2 3 0xef 0xbc 0xa1
    3    1    4    2    y      97 0x00061 a   1 1 a
    4    1    5    3    y      10 0x0000a =  -1 1 0x0a

   TODO: test on Mac.



WC
==

Special option in 'wc' to count non-zero width characters??
(But then, what about optional modifiers, e.g. skin-color and family-joiner?)

TODO: use 'iswspace()' in WC to count words? does this include other space
      characters?
      Find all chararects with WB/SB properties.
      (example: grep -i space PropList.

Page 438: 'interlinear' markers.

cut
====
for 'cut' - option to never cut in a combining-mark?
     (or technically, only cut in clear graphmeme? e.g. never before ZWJ, BIDI mark, etc.)?

Page 438: 'interlinear' markers.


tr
==

Uppercase mapping of ligatures turns into TWO letters?

e.g:
    U+FB01 LATIN SMALL LIGATURE FI (ﬁ)
    printf '\uFB01' |

POSIX says this must work,
in "Extended DEscription" under:
" Multi-byte characters require multiple, concatenated escape sequences of this type, including the leading <backslash> for each byte."

  $ printf "\xc3\xA4\n" | gtr '\xc3\xa4' X
  ä


Chracter Ranges are UNDEFINED in non-posix locale:
"c-c"
   "In locales other than the POSIX locale, this construct has unspecified behavior."

What does this mean??
"The ISO POSIX-2:1993 standard had a -c option that behaved similarly to the -C option, but did not supply functionality equivalent to the -c option specified in POSIX.1-2008.

The earlier version also said that octal sequences referred to collating elements and could be placed adjacent to each other to specify multi-byte characters. However, it was noted that this caused ambiguities because tr would not be able to tell whether adjacent octal sequences were intending to specify multi-byte characters or multiple single byte characters. POSIX.1-2008 specifies that octal sequences always refer to single byte binary values when used to specify an endpoint of a range of collating elements.
"

Checking "equivalent class" in FreeBSD's join
requires intimiate knowledge of Libc.

GNU sed insead of TR (find mailing list URLs):

    Upper/lower conversion
    tr -dc 'MBSEQ'
    Using equivalent classes


fold/fmt
========

character 'WJ' (word-joiner) - special treatment in 'fold / fmt'?

Does any 'space' character is space, or 'iswspace',
or only ASCII 0x20,0x09,0x0d ?



join
=====

FreeBSD's join bails out on invalid sequences:
 function 'mbssep()':
 https://opengrok.teamerlich.org/source/xref/freebsd-src/usr.bin/join/join.c#mbssep



unorm
=====

Check normalization according to NormalizationTest.txt

Check compatiblity of:

    U+00B5 MICRO SIGN
    U+03BC GREEK SMALL LETTER MU

    U+00C5 LATIN CAPITAL LETTER A WITH RING ABOVE
    U+212B ANGSTROM SIGN

    U+03B2 GREEK CAPITAL LETTER BETA
    U+00DF LATIN SMALL LETTER SHARP S

    U+03A9 GREEK CAPITAL LETTER OMEGA
    U+2126 OHM SIGN

    U+03B5 GREEK SMALL LETTER EPSILON
    U+2208 ELEMENT OF0xEA

    U+005C REVERSE SOLIDUS
    U+FF3C FULLWIDTH REVERSE SOLIDUS

Unexpected?? U+00E6 LATIN SMALL LETTER AE (æ) is NOT decomposed:
    "Originally a ligature representing a Latin diphthong, it has been
    promoted to the full status of a letter in the alphabets of some
    languages, including Danish, Norwegian, Icelandic and Faroese."
      - https://codepoints.net/U+00E6

    unlike:
      U+FB01 LATIN SMALL LIGATURE FI (ﬁ)
      https://codepoints.net/U+FB01

Check input with:
https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt

Check decomposition of:
   wcwidth() on LATIN-EXtended-B  characters:
      \u01c4 Latin Capital Letter DZ with caron
      \u01c5 Latin Capital Letter D with Small Letter Z with caron
      \u01c6 Latin Small Letter DZ with caron
      \u01c7 Latin Capital Letter LJ
      \u01c8 Latin Capital Letter L with Small Letter J
      \u01c9 Latin Small Letter LJ
      \u01ca Latin Capital Letter NJ
      \u01cb Latin Capital Letter N with Small Letter J
      \u01cc Latin Small Letter NJ

      \u01f1 Latin Capital Letter DZ
      \u01f2 Latin Capital Letter D with Small Letter
      \u01f3 Latin Small Letter DZ

      \u1f6 Latin Capital Letter Hwair (seems to work through with wcwidth==1)

      \u01fC Latin Capital Letter AE with acute Ǽ (seems to woth with wcwidth==1)

      \u0238 Latin Small Letter DB Digraph ȸ (seems ok) - despite being called digraphs
      \u0239 Latin Small Letter QP Digraph ȹ (seems ok)


  ESPECIALLY the "DZ with Caron" - is it decomposed to D,Z,Caron or D,z-with-caron ?

and these two: does decompistion results in O,dot,macron ?

    \u0230 Latin Capital Letter O with dot above and macron Ȱ -
    \u0231 Latin Small Letter O with dot above and macron ȱ


Check decomposition/compatiblity of IPA block, e.g.
    \u2A3 Latin Small Letter DZ Digraph ʣ - does this translates to DZ, or
          to the Latin-Extended-B 'DZ' latter?
    up to and including:
    \u2AB Latin Small Letter LZ Digraph

Check compatability and decomposition of 'fullwidth' characters,
   see https://en.wikipedia.org/wiki/Halfwidth_and_fullwidth_forms#In_Unicode
   e.g. does '\uFF21' (full-width A) decomposes to ascii 'A' ?


Check compatibility/decomposition of entire block:
    https://en.wikipedia.org/wiki/Alphabetic_Presentation_Forms


OD
==

In GNU:

  printf "\u03a8\n" | od -tx1c
  0000000  ce  a8  0a
          316 250  \n
          0000003

In Mac/FreeBSD (unless in C locale):
  $ gprintf  "\u03A8\n" | /usr/bin/od -t x1c
  0000000    ce  a8  0a
             Ψ  **  \n

in Mac/FreeBSD: invalid mb-seqeuences:
  $ gprintf  "\xce\xce\n" | /usr/bin/od -t x1c
  0000000    ce  ce  0a
            316 316  \n






mbbuffer
========

Modified NULL (\xC0\x80)

(page 282): Unicode conformance
     UNAssigned Code Points (C4)
     Test unassigned codes (don't generate, don't change) in all programs.
     Test non-characters (U+FFFE, U+FFFF)
     Test surrogate codes

Surrogate codepoints treated as invalid on "normal" unixes:

    $ printf '\uD800\n' | ./src/mbbuffer-test -r
    ofs  line colB colC V wc(dec) wc(hex) Ch w n octets
    0    1    1    1    n       *       * *  * 1 0xed
    1    1    2    2    n       *       * *  * 1 0xa0
    2    1    3    3    n       *       * *  * 1 0x80
    3    1    4    4    y      10 0x0000a =  -1 1 0x0a

  but on Cygwin:

    Administrator@WIN-9FFSHRJAFVN ~/coreutils-8.25.71-1437c
    $  printf '\uD800\n' | ./src/mbbuffer-test -r
    ofs  line colB colC V wc(dec) wc(hex) Ch  W n octets
    0    1    1    1    y   55296 0x0d800 =  -1 3 0xed 0xa0 0x80
    3    1    4    2    y      10 0x0000a =  -1 1 0x0a


page 437: Check all special characters
     and their effects in various programs.


TODO for Book:
     Show examples of conversion cases in page 501/502.

     sed (and grep/gawk) NEVER match regular expressions to invalid
       multibyte sequences. To Force matching, use LC_ALL=C.

     $ printf '\xe1\xbc\x11' | LC_ALL=C ./sed/sed 's/./X/g' | od -tx1
     0000000 58 58 58
     0000003



TODO: Special handling for "modified UTF-8" with NULL as
      UTF-8 "\xC0\x80" ?
      system's native mbrtowc does not handle it,
      and will return -1 .

TODO: prepare for all types of invalid sequences:
      https://en.wikipedia.org/wiki/UTF-8#Invalid_byte_sequences :
      ----
      Not all sequences of bytes are valid UTF-8. A UTF-8 decoder should be prepared for:
      * the red invalid bytes in the above table
      * an unexpected continuation byte
      * a leading byte not followed by enough continuation bytes (can happen in
        simple string truncation, when a string is too long to fit when copying it)
      * an overlong encoding as described above
      * a sequence that decodes to an invalid code point as described below

      https://en.wikipedia.org/wiki/UTF-8#Invalid_code_points :
      ------
      Since RFC 3629 (November 2003), the high and low
      surrogate halves used by UTF-16 (U+D800 through U+DFFF) and code
      points not encodable by UTF-16 (those after U+10FFFF) are not
      legal Unicode values, and their UTF-8 encoding must be treated
      as an invalid byte sequence.

      Not decoding surrogate halves makes it impossible to store
      invalid UTF-16, such as Windows filenames, as UTF-8. Therefore,
      detecting these as errors is often not implemented and there are
      attempts to define this behavior formally (see WTF-8 and CESU
      below).

Check input with:
  https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt

  Section3.3  Sequences with last continuation byte missing:

    All bytes of an incomplete sequence should be signalled as a single
    malformed sequence, i.e., you should see only a single replacement
    character in each of the next 10 tests. (Characters as in section 2).

  Mbbuffer currently reports EACH invalid octet instead
  of just one per incomplete sequence.

  TODO: Does incomplete sequence in the middle of the file
  reported as incomplete (mbrtowc==-2) or invalid (mbrtowc==-1) ?

  If we report on the FIRST octet (including line,byte/char offset),
  the user (needing low-level processing) won't be able to tell
  the differences without further processing. By reporting
  all octets, we provide easier work-arounds
  (but we also 'pollute' stdout with more "invalid char" markers
  than needed). Perhaps add this as an option?

  On Ubuntu 14.04 with xterm 322, terminal prints only one "invalid char":
    $ printf '\ud800\n'
    �

  On Ubuntu 14.04 with gnome-terminal 3.6.2, nothing is printf.

  Mac OS X terminal prints 3 question marks:
    $ printf '\ud800\n'
    ???

  Web browsers print 3 characters:
    visit https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
    and view section 5.1.1.
  On Linux: Google Chrom 51.0, Firefox 48.0
  On Mac: Safari 8.0.7, Chrome 47.0, IceCat 37.0

  $ printf '\367\277\277\n' | ./src/mbbuffer-test.exe -r
  ofs  line colB colC V wc(dec) wc(hex) Ch  W n octets
  0    1    1    1    n       *       * *   * 1 0xf7
  1    1    2    2    n       *       * *   * 1 0xbf
  2    1    3    3    n       *       * *   * 1 0xbf
  3    1    4    4    y      10 0x0000a =  -1 1 0x0a



Also see seciont 5.1 (Single UTF-16 surrogates)
  where he claims each invalid sequence should result in ONE
  "invalid character" output:
  e.g.
       5.1.1  U+D800 = ed a0 80 = "���"
  In such cases 'mbrtowc' return -1 THREE times (or is it because
  we reset mbstate_t after each failure?)



Resources
==========
UTF-8 and Unicode FAQ for Unix/Linux:
   https://www.cl.cam.ac.uk/~mgk25/unicode.html

UTF-8 decoder capability and stress test:
   https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt


TODO:

    "[...] U+FFFE and U+FFFF must not occur in normalUTF-8 or UCS-4
    data. UTF-8 decoders should treat them like malformedor overlong
    sequences for safety reasons."
    (http://m.blog.csdn.net/article/details?id=50910387)



Unicode Book
============

page 11: Finish har har har - change with char-classes, regex, normalization, upper/lower cases

page 12: ligature "fi": change for normalization, char classes

page 23: German lower-case "strasse" (sharp-S?) becomes "SS" in upper-case (two characters).
         Also differ from greek "beta" glyph.
         '\u00DF'

page23: 0-with-cross is diameter in mechanical writing, or a letter in Nordic
        languages?

Length of BIDI markers (zero width?)

page 29 (2nd paragraph from bottom): Greek Sigma in middle vs final form.
        If there's no equivalence between them, how about sort order ?

page 29 (top): initial/middle/final/separate contextual forms (e.g hebrew/arabic)
        Sort order ?

Page 143: Transcoding tools
     http://www.unicode.org/Public/MAPPINGS
     TODO: Download these for offline processing.

Page 145: Repertiore requirements
      Characters in each language:
      http://www.eki.ee/letter

Page 169: Named Sequences
     http://www.unicode.org/Public/UNIDATA/NamedSequences.txt

Page 178: Table 4.3: Code-point Classification
     TODO: Test unassigned, Surrogate, Private-Use input.
           Ensure no bugs, should be passed as it.
           What about "wc" and "cut" ?

Page 182: DiGraph
     e.g "ll" in Spanish, "Ch" in some others - two distinct characters
         logically treated as one by native language speakers.
     VS
     æ (\u00E6) which is one character for "ae".
     ĳ (\u0133) which is small latin ligature "ij".
     TODO: Check unicode-normalization-decomposition.

Page 185: unicode standard - chapters
     Chapter 5: Implementation Guidelines

Page 194: Varient Selectors
     Unicode markers affecting the precending code-point,
     ∩︀ (\u2229 - "intersection" symbol) followed by \uFE00
     ("variant selector" VS1). Affect font in applicaiton ?
     IS this Zero width character ?
     TODO: check with 'expand', 'cut', 'wc'.

Page 195: Ligatures.
     in Danish/Norwegian, æ (\u00E6) is an independent letter,
     vs just a ligature of two letters "ae" in other languages.
     TODO: Test sort order with such input in Danish-vs-other locales.
     TODO: in Danish locale, should unicode normaliztion NOT decompose it??
     Unicode "Alphabetic PResentation Form" block (U+FB00..U+FB4F).

     TODO: Test decomposition of ligatures in that block (e.g. hebrew ligatures?)
     $ printf '\ufb00\n'
     ﬀ
     $ printf '\ufb03\n'
     ﬃ
     $ printf '\ufb4a\n'
תּ

     ZWJ (\u200d) should instruct the application to join the
     characters before/afer into legature.
     Doesn't seem to work (on Mac OS):
        $ printf 'f\u200Di\n'
        f i

     Similarly, ZWNJ (\u200C) should prevent joining.
     TODO: test ZWJ,ZWNJ (zero width or "invisible control" chars?)
     in cut/expand/wc.


Page 196: Vowels vs Marks
     Hebrew+Arabic: Nikud.

     Hindi (Devanagaris script):
     \u092A (pa) followed by \u0942 (uu) appears as one glyph (puu).

Page 211, Table 5-1: General Category VAlues.

Page 216: Character Property 'ea' = Asian Width Full,Half,narrow.
     Affects 'expand' ?

Page 216: Grapheme Clusters? for 'fold/fmt/cut' ?

Page 219,220: Use 'WB' (WordBreak)' or 'WS' (Whitespace) 'SB' (Sentense-break)
     properties for counding-words in 'wc' ?

     gfdafda d dfsa fdsa fdsa
     fdafda fdsafdsa

Page 220: Property 'SFC' (Simple-Case-Folding): Upper/Lower case are simple.

Page 227: Canonical vs Compatability mapping
       Canonical: different encoding for SAME symbol.
       Compatibility: fundamentally similar characters, differ in rendering/
                      usage (and sometimes in meaning)

     Examples in Book
     \u2126 = \u03a9

Page 231:
     Iterative decompisition:

     ANGSTRAM (U+212b):
       $ printf '\u212b\n'
       Å
     Is canonical-mapped to 'A-with-Ring U+00C5':
       $ printf '\u00c5\n'
       Å
     Which is canonical mapped to 'A + combining mark ring (U+030a)':
       $ printf 'A\u30a\n'
       Å

Page 233:

     Decomposition of 'VULGAR HALF', 'MICRO SIGN', 'E WITH GRAVE':

       $ printf '\u00BD\u00b5\u00e8\n'
       ½µè

     Becomes:
        VULGAR HALF => 1 'FRACTION SLASH' 2
        MICRO SIGN  => greek mu
        'E WITH GRAVE' => 'E' 'COMBING MARK GRAVE'

    With decomposition (E + combing grave mark):
      $ printf '\u00BD\u00b5\u00e8\n'| ./src/unorm -n nfkd  | iconv -t ucs-2le | od -tx2 -An
      0031 2044 0032 03bc 0065 0300 000a

    Without decomposition ('E WITH GRAVE' stays as-is):
      $ printf '\u00BD\u00b5\u00e8\n'| ./src/unorm -n nfkc  | iconv -t ucs-2le | od -tx2
      0031 2044 0032 03bc 00e8 000a

    TODO: for 'sort', 'uniq', 'join':
       Test the above strings as 'equivalent' (strxfrm/strcoll) ?

Page 249: Collation order
     no official collation order.
     Unicode Technical STandard #10
     http://www.unicode.org/reports/tr30/tr30-4.html

Page 256: Text Boundaries
     See the files in /Users/gordon/projects/unicode-mapping/www.unicode.org/Public/9.0.0/ucd/auxiliary
     like WordBreakProprty.txt
     includes test files
     TODO: for wc,fold,fmt,cut ?
     TODO: instead of 'iswspace' is unicode ''Alphabetic' Property?

     For Book: document exceptions for Thai/Lao/Hiragana ?

Page 276: Line-BReaking rules
     for fold/fmt ?

Page 282: Unicode Conformance requirements
     TODO: Test unassigned codes (don't generate, don't change) in all programs.

Page 285:
     Conformance: C12a: unorm is conformant.

Page 286:
     Conformance: C14,15,16 (normalization): unorm is conformant.

page 287:
     Conformance: When mentioning normalization, use proper terms
                  (for unorm)

Page 299: UTF-8 vs ISO-8859-1
     For Book

Page 300: Duplicate Octet Range rable, add octal

Page 392: Duplicate table of control characters, add octal
     mention sed, printf, od
     for book/ /website

Page 414: Fixed with charachers
     (e.g. em/en dashes)
     TODO: How to treat in 'expand' ?

page 426: Line-break chracters in unicode
     for fold/fmt , what about 'wc' ?
     LS (U+2028) Line Separator
     PS (U+2029) Paragraph separator
     For Book/website

Page 426: mathenatical and technical symbols
     For Book/website: canocnical compatiblity with other chars.

Page 438:
     'other' non alphabetical markers, should they be counted as words?

     $ env printf '\ufff9assaf\ufffagordon\ufffb\n' |  wc
     1       1      21

     in HTML this would be rendered as two words, assaf/gordon.

Page 468: Invisible characters ?

Page 469:
     MArkup vs plaintext:
     Table 9-2: should these characters be counted in 'wc',
     skipped in 'expand', non-break with 'cut' ?

     Are these considerd "word break" properties?
     should SED's "\b \B \< \>" regex operators support them?

Page 592: Patterns, regex patterns.
     TODO: ensure tests cases according to page 594.

page 597: "Basic Unicode Support"
     TODO: Check which coreutils fall under the requirements,
           and whether they comply.
