#!/usr/bin/perl

# Exercise tr with multibyte input

# Copyright (C) 2017 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

use strict;

(my $ME = $0) =~ s|.*/||;

my $prog = 'tr';

# Turn off localization of executable's output.
@ENV{qw(LANGUAGE LANG LC_ALL)} = ('C') x 3;

my $locale = $ENV{LOCALE_FR_UTF8};
! defined $locale || $locale eq 'none'
  and CuSkip::skip "$ME: this test requires FR-UTF8 locale\n";


=pod
The following codepoints are used in the tests:

  Perl      UTF-8             CodePoint
  \x{A0}    \xC2\xA0          U+00A0 NO-BREAK SPACE
  \x{3A6}   \xCE\xA6          U+03A6 GREEK CAPITAL LETTER PHI
  \x{3C6}   \xCF\x86          U+03C6 GREEK SMALL LETTER PHI
  \x{3C7}   \xCF\x87          U+03C7 GREEK SMALL LETTER CHI
  \x{2006}  \xE2\x80\x86      U+2006 SIX-PER-EM SPACE
  \x{2700}  \xE2\x9C\x80      U+2700 BLACK SAFETY SCISSORS
  \x{1F600} \xF0\x9F\x98\x80  U+1F600 GRINNING FACE

Specify them as binary octets, so perl prints them as-is without any UTF8
translation (also avoids the "wide character in print" perl warning).
=cut
my $ascii = "Aa* 12";
my $nbsp = "\xC2\xA0";
my $PHI  = "\xCE\xA6";
my $phi  = "\xCF\x86";
my $CHI  = "\xCE\xA7";  # not used in the input, checked in output of some tests
my $chi  = "\xCF\x87";
my $em6  = "\xE2\x80\x86";
my $scis = "\xE2\x9C\x80";
my $grin = "\xF0\x9F\x98\x80";

my $in1 = $ascii . $nbsp . $PHI . $phi . $chi . $em6 . $scis . $grin;

my $in2 = "AAAAzzzz" . $PHI.$PHI.$PHI.$PHI . $chi.$chi.$chi.$chi;


my @Tests =
(
 ############################################################
 ##              Delete explicit characters                ##
 ############################################################

 ['d1', "-d '${PHI}${phi}'", {IN=>$in1},
  {OUT=> $ascii . $nbsp .               $chi . $em6 . $scis. $grin }],
 ['d2', "-d '$grin'", {IN=>$in1},
  {OUT=> $ascii . $nbsp . $PHI . $phi . $chi . $em6 . $scis }],

 # Delete complement
 ['d3', "-cd '$grin'", {IN=>$in1},
  {OUT=> $grin }],

 # Delete using an octal value - 'tr' should revert to unibyte processing
 # and delete octets directly (producting invalid multibyte sequences).
 # \x80 = \200 , \x86 = \206
 ['d4', "-d '\\200\\206'", {IN=>$in1},
  {OUT=> "Aa* 12" .
         "\xC2\xA0" .
         "\xCE\xA6" .
         "\xCF" .      # truncated mb character
         "\xCF\x87" .
         "\xE2"     .  # truncated mb character six-em-space
         "\xE2\x9C" .  # truncated mb character safety scissors
         "\xF0\x9F\x98"}], # truncated mb character grinning face

 # Same as above, with octets swapped in SET1
 ['d4-1', "-d '\\206\\200'", {IN=>$in1},
  {OUT=> "Aa* 12" .
         "\xC2\xA0" .
         "\xCE\xA6" .
         "\xCF" .      # truncated mb character
         "\xCF\x87" .
         "\xE2"     .  # truncated mb character six-em-space
         "\xE2\x9C" .  # truncated mb character safety scissors
         "\xF0\x9F\x98"}], # truncated mb character grinning face

 # Similar to above, except the octets represent a valid UTF-8 character
 # (\342\200\206 = \xE2\x80\x86 = U+2006 - SCISSORS)
 # This behaviour might be against POSIX, but was decided to maintain
 # backwards compatability: if a user specified octal values on the command-line,
 # he/she has been intending to edit 8-bit octets directly (since GNU tr was
 # first released).
 # See this thread: https://lists.gnu.org/r/coreutils/2017-09/msg00028.html
 ['d4-3', "-d '\\342\\200\\206'", {IN=>$in1},
  {OUT=> "Aa* 12" .
         "\xC2\xA0" .
         "\xCE\xA6" .
         "\xCF" .          # truncated mb character
         "\xCF\x87" .
         ""     .          # deleted mb char six-em-space
         "\x9C" .          # truncated mb character safety scissors
         "\xF0\x9F\x98"}], # truncated mb character grinning face


 ############################################################
 ##              Delete by character class                 ##
 ############################################################

 #my $in1 = $ascii . $nbsp . $PHI . $phi . $chi . $em6 . $scis . $grin;

 ['d5',  "-d   '[:alpha:]'", {IN=>$in1},
  {OUT=> "* 12" .  $nbsp                       . $em6 . $scis. $grin }],
 ['d5c', "-dc  '[:alpha:]'", {IN=>$in1},
  {OUT=> "Aa"              . $PHI . $phi . $chi }],

 ['d6', "-d '[:alnum:]'", {IN=>$in1},
  {OUT=> "* " .    $nbsp                       . $em6 . $scis. $grin }],

 # NOTE:
 # Tested under Ubuntu 16.04 / GLibc 2.23:
 #   Non-breaking-space does not match a '[:space:]' characeter class,
 #   but SIX-PER-EM-SPACE does.
 ['d7', "-d '[:space:]'", {IN=>$in1},
  {OUT=> "Aa*12"  . $nbsp . $PHI . $phi . $chi .        $scis . $grin}],

 ['d8', "-d '[:digit:]'", {IN=>$in1},
  {OUT=> "Aa* "   . $nbsp . $PHI . $phi . $chi . $em6 . $scis . $grin}],

 # NBSP is not printable, but SIX-PER-EM-SPACE is ?
 ['d9', "-d '[:print:]'", {IN=>$in1},
  {OUT=>            $nbsp  }],

 ['d10', "-d  '[:graph:]'", {IN=>$in1},
  {OUT=> " "      . $nbsp                       . $em6                }],
 ['d10c',"-dc '[:graph:]'", {IN=>$in1},
  {OUT=> "Aa*12"          . $PHI . $phi . $chi         . $scis . $grin}],


 ['d11', "-d  '[:lower:]'", {IN=>$in1},
  {OUT=> "A* 12"  . $nbsp . $PHI                . $em6 . $scis . $grin}],
 ['d12', "-d  '[:upper:]'", {IN=>$in1},
  {OUT=> "a* 12"  . $nbsp        . $phi . $chi  . $em6 . $scis . $grin}],


 ############################################################
 ##              Squeeze explicit characters               ##
 ############################################################
 #my $in2 = "AAAAzzzz" . $PHI.$PHI.$PHI.$PHI . $chi.$chi.$chi.$chi;

 ['s1', "-s 'A$PHI'", {IN=>$in2},
  {OUT=> "Azzzz" . $PHI                . $chi.$chi.$chi.$chi }],
 ['s2', "-cs 'A$PHI'", {IN=>$in2},
  {OUT=> "AAAAz" . $PHI.$PHI.$PHI.$PHI . $chi }],

 ############################################################
 ##              Squeeze character classes                 ##
 ############################################################

 ['s3', "-s '[:alpha:]'", {IN=>$in2},
  {OUT=> "Az" . $PHI                . $chi }],
 ['s4', "-cs '[:alpha:]'", {IN=>$in2},
  {OUT=> $in2 }],


 ############################################################
 ##        Delete and Squeeze explicit characters          ##
 ############################################################

 ['ds1', "-ds 'A$chi' 'z$PHI'", {IN=>$in2},
  {OUT=> "z" . $PHI }],



 ############################################################
 ##        Translate explicit characters                   ##
 ############################################################
 ['t1', "'A$PHI$chi' 'XYZ'", {IN=>$in1},
  {OUT=> "Xa* 12" . $nbsp . "Y" . $phi . "Z" . $em6 . $scis. $grin}],

 ['t2', "'A' '$grin'", {IN=>"aAa"},
  {OUT=> "a" . $grin . "a" }],

 ['t2c', "-c 'A' '$grin'", {IN=>"aAa"},
  {OUT=> $grin . "A" . $grin }],

 ['t3', "'$PHI' '$phi'", {IN=> $PHI . $phi . $PHI},
  {OUT=> $phi . $phi . $phi }],

 ['t3s', "-s '$PHI' '$phi'", {IN=> $PHI . $phi . $PHI},
  {OUT=> $phi }],

 ['t3c', "-c '$PHI' '$phi'", {IN=> $PHI . $phi . $PHI},
  {OUT=> $PHI . $phi . $PHI }],

 ['t4', "-c '$PHI' '$phi'", {IN=> $scis . $PHI . $phi . $PHI},
  {OUT=> $phi . $PHI . $phi . $PHI }],



 ############################################################
 ##        upper/lower translation                         ##
 ############################################################

 ['ul1', "'[:lower:]' '[:upper:]'", {IN=>$in1},
  {OUT=> "AA* 12" . $nbsp . $PHI . $PHI . $CHI . $em6 . $scis. $grin}],

 ['ul2', "'[:upper:]' '[:lower:]'", {IN=> $PHI.$phi.$CHI.$chi."Aa".$grin },
  {OUT=> $phi.$phi.$chi.$chi."aa".$grin }],

 ['ul3', "'[:upper:][:lower:]' '[:lower:][:upper:]'",
  {IN=>  $PHI.$phi.$CHI.$chi."Aa".$grin },
  {OUT=> $phi.$PHI.$chi.$CHI."aA".$grin }],

 # No-op but valid
 # TODO: currently not working
 #['ul4', "'[:upper:]' '[:upper:]'",
 # {IN=>  $PHI.$phi.$CHI.$chi."Aa".$grin },
 # {OUT=> $PHI.$phi.$CHI.$chi."Aa".$grin }],



 ############################################################
 ##         translate character classes                    ##
 ############################################################

 #my $in1 = $ascii . $nbsp . $PHI . $phi . $chi . $em6 . $scis . $grin;

 ['tc1',  "'[:alpha:]' 'z'", {IN=>$in1},
  {OUT=> "zz* 12" .  $nbsp . "z"  . "z".   "z"  . $em6 . $scis . $grin}],

 ['tc1c', "-c '[:alpha:]' 'z'", {IN=>$in1},
  {OUT=> "Aazzzz" .  "z"   . $PHI . $phi . $chi . "z"  . "z".    "z"}],


);

# Force multibyte locale in all tests.
#
# NOTE about the ERR_SUBST:
# The error tests above (e1/e2/e3/e4) expect error messages in C locale
# having single-quote character (ASCII 0x27).
# In UTF-8 locale, the error messages will use:
#  'LEFT SINGLE QUOTATION MARK'  (U+2018) (UTF8: 0xE2 0x80 0x98)
#  'RIGHT SINGLE QUOTATION MARK' (U+2019) (UTF8: 0xE2 0x80 0x99)
# So we replace them with ascii single-quote and the results will
# match the expected error string.
#
foreach my $t (@Tests)
{
    push @$t, ({ENV => "LC_ALL=$locale"},
	       {ERR_SUBST => "s/\xe2\x80[\x98\x99]/'/g"});
}


@Tests = triple_test \@Tests;

# tr takes its input only from stdin, not from a file argument, so
# remove the tests that provide file arguments and keep only the ones
# generated by triple_test (identifiable by their .r and .p suffixes).
@Tests = grep {$_->[0] =~ /\.[pr]$/} @Tests;

my $save_temps = $ENV{DEBUG};
my $verbose = $ENV{VERBOSE};

my $fail = run_tests ($prog, $prog, \@Tests, $save_temps, $verbose);
exit $fail;
