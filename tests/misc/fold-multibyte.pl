#!/usr/bin/perl
# Exercise fold with multibyte input

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
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;

(my $ME = $0) =~ s|.*/||;

my $limits = getlimits ();
my $UINTMAX_OFLOW = $limits->{UINTMAX_OFLOW};

(my $program_name = $0) =~ s|.*/||;
my $prog = 'fold';

my $locale = $ENV{LOCALE_FR_UTF8};
! defined $locale || $locale eq 'none'
  and CuSkip::skip "$ME: this test requires FR-UTF8 locale\n";


=pod
The first definition from Euclid's Elements, book 1:

    Σημεῖόν ἐστιν οὗ μέρος οὐθέν
    (A point is that which has no part)

    semeion / Σημεῖόν / mark (point)
    estin   / ἐστιν   / is
    hoo     / οὗ      / where
    meros   / μέρος   / part
    ouden   / οὐθέν   / none(nothing)
=cut
my $semeion = "\x{3A3}\x{3B7}\x{3BC}\x{3B5}\x{1FD6}\x{3CC}\x{3BD}" ;
my $estin   = "\x{1F10}\x{3C3}\x{3C4}\x{3B9}\x{3BD}" ;
my $hoo     = "\x{3BF}\x{1F57}" ;
my $meros   = "\x{3BC}\x{3AD}\x{3C1}\x{3BF}\x{3C2}" ;
my $ouden   = "\x{3BF}\x{1F50}\x{3B8}\x{3AD}\x{3BD}" ;

# mb-char counts:
#           7        5      2    5      5
my $def1 = "$semeion $estin $hoo $meros $ouden";



## NOTE about tests locales:
## Tests starting with 'mb' will have {ENV=>"LC_ALL=$locale"}
## added to them automatically - results are multibyte-aware.
## Tests starting with 'sb' have the same input but will be
## run under C locale and will be treated as single-bytes.
## This enables interleaving C/UTF8 tests
## (for easier comparison of expected results).

my @Tests =
  (
   ### GREEK SMALL LETTER ALPHA (U+03B1) - 2 octets

   # Large width - no folding
   ['mb1-1', '-w 12', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\xB1bbbb"}],
   ['st1-1', '-w 12', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\xB1bbbb"}],

   # Fold just after the MB character (octet-wise)
   # In multibyte we should see another 'b' (as the 2-octet MB-char is
   # treated as taking 1 column).
   ['mb1-2', '-w 7', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\xB1b\n" . "bbb"}],
   ['st1-2', '-w 7', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\xB1\n" . "bbbb"}],

   # Fold in the middle of the mb-char (octet-wise).
   # MB-aware folding should print the mb-char on the top line.
   # C folding should break the mb-char into two (invalid) octets.
   ['mb1-3', '-w 6', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\xB1\n" . "bbbb"}],
   ['st1-3', '-w 6', {IN=>"aaaaa\xCE\xB1bbbb"},
    {OUT=>"aaaaa\xCE\n" . "\xB1bbbb"}],


   ### THORN WITH STROKE  (U+A764) - 3 octets

   # Fold one character after the mb-char
   ['mb2-1', '-w 7', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4bbb"}],
   ['sb2-1', '-w 7', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4b\n" . "bb"}],

   # Fold at the end of the mb-char
   ['mb2-2', '-w 6', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4bb\n" . "b"}],
   ['sb2-2', '-w 6', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4\n" . "bbb"}],

   # Fold in the middle of the mb-char
   ['mb2-3', '-w 5', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4b\n" . "bb"}],
   ['sb2-3', '-w 5', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\n" . "\xA4bbb"}],

   # Fold in the middle of the mb-char
   ['mb2-4', '-w 4', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\x9D\xA4\n" . "bbb"}],
   ['sb2-4', '-w 4', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\xEA\n" . "\x9D\xA4bb\n" . "b"}],

   # Fold before the mb-char
   ['mb2-5', '-w 3', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\n" . "\xEA\x9D\xA4bb\n" . "b"}],
   ['sb2-5', '-w 3', {IN=>"aaa\xEA\x9D\xA4bbb"},
    {OUT=>"aaa\n" . "\xEA\x9D\xA4\n" . "bbb"}],


   ### GOTHIC LETTER LAGUS (U+1033B) - 4 octets

   # Fold one char after the mb-char
   ['mb3-1', '-w 8', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBBzzz"}],
   ['sb3-1', '-w 8', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBBz\n" . "zz"}],

   # Fold at the end of the mb-char
   ['mb3-2', '-w 7', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBBzzz"}],
   ['sb3-2', '-w 7', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBB\n" . "zzz"}],

   # Fold in the middle of the mb-char
   ['mb3-3', '-w 6', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBBzz\n" . "z"}],
   ['sb3-3', '-w 6', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\n" . "\xBBzzz"}],

   ['mb3-4', '-w 5', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBBz\n" . "zz"}],
   ['sb3-4', '-w 5', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\n" . "\x8C\xBBzzz"}],

   ['mb3-5', '-w 4', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\x90\x8C\xBB\n" . "zzz"}],
   ['sb3-5', '-w 4', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\xF0\n" . "\x90\x8C\xBBz\n" . "zz"}],

   # Fold before the MB char
   ['mb3-6', '-w 3', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\n" . "\xF0\x90\x8C\xBBzz\n" . "z"}],
   ['sb3-6', '-w 3', {IN=>"aaa\xF0\x90\x8C\xBBzzz"},
    {OUT=>"aaa\n" . "\xF0\x90\x8C\n" . "\xBBzz\n" . "z"}],



   ### Test space-aware multibyte-aware folding

   # no folding
   ['mb4-1', {IN=>$def1}, {OUT=>$def1}],

   # fold in the middle of the 4th word
   ['mb4-2', '-w 20', {IN=>$def1},
    {OUT=>"$semeion $estin $hoo \x{3BC}\x{3AD}\x{3C1}\n" .
         "\x{3BF}\x{3C2} $ouden"}],

   # fold with spaces
   ['mb4-3', '-w 20 -s', {IN=>$def1},
    {OUT=>"$semeion $estin $hoo \n" . "$meros $ouden"}],

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
if ($locale ne 'C')
  {
    my @new;
    foreach my $t (@Tests)
      {
        my ($tname) = @$t;
        if ($tname =~ /^mb/)
          {
            push @$t, ({ENV => "LC_ALL=$locale"},
                       {ERR_SUBST => "s/\xe2\x80[\x98\x99]/'/g"});
          }
      }
  }


my $save_temps = $ENV{DEBUG};
my $verbose = $ENV{VERBOSE};

my $fail = run_tests ($program_name, $prog, \@Tests, $save_temps, $verbose);
exit $fail;
