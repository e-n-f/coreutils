#!/usr/bin/perl
# Exercise fold.

# Copyright (C) 2003-2017 Free Software Foundation, Inc.

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

(my $program_name = $0) =~ s|.*/||;

# Turn off localization of executable's output.
@ENV{qw(LANGUAGE LANG LC_ALL)} = ('C') x 3;

my $locale = $ENV{LOCALE_FR_UTF8};
! defined $locale || $locale eq 'none'
  and $locale = 'C';

my @Tests =
  (
   ['s1', '-w2 -s', {IN=>"a\t"}, {OUT=>"a\n\t"}],
   ['s2', '-w4 -s', {IN=>"abcdef d\n"}, {OUT=>"abcd\nef d\n"}],
   ['s3', '-w4 -s', {IN=>"a cd fgh\n"}, {OUT=>"a \ncd \nfgh\n"}],
   ['s4', '-w4 -s', {IN=>"abc ef\n"}, {OUT=>"abc \nef\n"}],
  );


# Repeat all tests with multibyte locale ('mbl-' prefix),
# to ensure there are no rergessions.
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
        my ($tname, @tparams) = @$t;

        push @new, [ "mbl-$tname", @tparams,
                     {ENV => "LC_ALL=$locale"},
                     {ERR_SUBST => "s/\xe2\x80[\x98\x99]/'/g"}];
      }
    push @Tests, @new;
  }


my $save_temps = $ENV{DEBUG};
my $verbose = $ENV{VERBOSE};

my $prog = 'fold';
my $fail = run_tests ($program_name, $prog, \@Tests, $save_temps, $verbose);
exit $fail;
