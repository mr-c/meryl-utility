#!/usr/bin/env perl

###############################################################################
 #
 #  This file is part of canu, a software program that assembles whole-genome
 #  sequencing reads into contigs.
 #
 #  This software is based on:
 #    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 #    the 'kmer package' (http://kmer.sourceforge.net)
 #  both originally distributed by Applera Corporation under the GNU General
 #  Public License, version 2.
 #
 #  Canu branched from Celera Assembler at its revision 4587.
 #  Canu branched from the kmer project at its revision 1994.
 #
 #  Modifications by:
 #
 #    Brian P. Walenz beginning on 2016-JAN-11
 #      are a 'United States Government Work', and
 #      are released in the public domain
 #
 #  File 'README.licenses' in the root directory of this distribution contains
 #  full conditions and disclaimers for each license.
 ##

use strict;
use File::Compare;
use Cwd qw(getcwd);

my $cwd = getcwd();

my $major    = "1";
my $minor    = "4";
my $commits  = "0";
my $hash     = undef;   #  This from 'git describe'
my $dirty    = "";
my $firstRev = undef;   #  This from 'git rev-list'
my $revCount = 0;


#  If in a git repo, we can get the actual values.

if (-d "../.git") {

    open(F, "git rev-list HEAD |") or die "Failed to run 'git rev-list'.\n";
    while (<F>) {
        chomp;

        $firstRev = $_  if (!defined($firstRev));
        $revCount++;
    }
    close(F);

    open(F, "git describe --tags --long --dirty --always --abbrev=40 |") or die "Failed to run 'git describe'.\n";
    while (<F>) {
        chomp;
        if (m/^v(\d+)\.(\d+.*)-(\d+)-g(.{40})-*(.*)$/) {
            $major   = $1;
            $minor   = $2;
            $commits = $3;
            $hash    = $4;
            $dirty   = $5;
        } else {
            die "Failed to parse describe string '$_'.\n";
        }
    }
    close(F);

    if ($dirty eq "dirty") {
        $dirty = "ahead of github";
    } else {
        $dirty = "sync'd with guthub";
    }
}


#  If not in a git repo, we might be able to figure things out based on the directory name.

elsif ((! -e "../.git") &&
    ($cwd =~ m/canu-(.{40})\/src/)) {
    $hash     = $1;
    $firstRev = $1;
}


#  Otherwise, we know absolutely nothing.

else {
    $hash     = "unknown-hash-tag-no-repository-available";
    $firstRev = "unknown-hash-tag-no-repository-available";
}



#  Report what we found.  This is really for the gmake output.

if ($commits > 0) {
    print STDERR "Building snapshot v$major.$minor (+$commits changes) r$revCount $hash ($dirty)\n";
    print STDERR "\n";
} else {
    print STDERR "Building release v$major.$minor\n";
    print STDERR "\n";
}

#  Dump a new file, but don't overwrite the original.

open(F, "> canu_version.H.new") or die "Failed to open 'canu-version.H.new' for writing: $!\n";
print F "//  Automagically generated by canu_version_update.pl!  Do not commit!\n";
print F "#define CANU_VERSION_MAJOR     \"$major\"\n";
print F "#define CANU_VERSION_MINOR     \"$minor\"\n";
print F "#define CANU_VERSION_COMMITS   \"$commits\"\n";
print F "#define CANU_VERSION_REVISION  \"$revCount\"\n";
print F "#define CANU_VERSION_HASH      \"$firstRev\"\n";
close(F);

#  If they're the same, don't replace the original.

if (compare("canu_version.H", "canu_version.H.new") == 0) {
    unlink "canu_version.H.new";
} else {
    unlink "canu_version.H";
    rename "canu_version.H.new", "canu_version.H";
}

#  That's all, folks!

exit(0);

