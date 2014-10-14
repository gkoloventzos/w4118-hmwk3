#!/usr/bin/perl -w
use File::Basename;
use Term::ANSIColor qw(:constants);
local $Term::ANSIColor::AUTORESET = 1;

if ($#ARGV == 0 and ($ARGV[0] =~ m/-h/ or $ARGV[0] !~ m/^[0-9a-f]{40}$/)) {
	print "Usage: ours_checkpatch.pl SHA\n";
	exit 0;
}

unless ( -d "./.git" ) {
	print "You should run this scipt in the top directory of the kernel\n";
	exit 1;
}

my %skip_list = ();
$skip_list{"flo-kernel/arch/arm/include/asm/unistd.h"} = 1;

my $rev_list;
my $rev_hash_f;
unless (defined $ARGV[0]) {
	print ON_RED "Searching for first commit with no TAs mail";
	$rev_list = `git rev-list HEAD --reverse`;
	print "\n";
	foreach my $rev_hash (split /\n/, $rev_list) {
		my $oo = `git --no-pager show -s --format='%ae' $rev_hash`;
		if ($oo !~ m/shihwei\@cs\.columbia\.edu/) {
			$rev_hash_f = $rev_hash;
			last;
		}
	}
}

my $hash = $ARGV[0] ? $ARGV[0] : $rev_hash_f;
#unistd.h has problem with the + signs
print GREEN "Starting from commit with hash $hash\n";
my $skip = "flo-kernel/arch/arm/include/asm/unistd.h";
my $checkpath = "flo-kernel/scripts/checkpatch.pl";
my $out = `git diff --name-only $hash HEAD`;
my $gdiff = "--ignore FILE_PATH_CHANGES -terse --no-signoff -no-tree";
my @lines = split /\n/, $out;
foreach my $line (@lines) {
	unless (-B $line) {
		if (exists $skip_list{$line}) {
			my $file = basename($line);
			$std = `git diff $hash -- $line > /tmp/$file`;
			$stdout =
			`$checkpath $gdiff /tmp/$file`;
			`rm -f /tmp/$file`;
		} else {
			$stdout = `$checkpath -terse -no-tree -f $line`;
		}
		my @stdout_in_lines = split /\n/, $stdout;
		foreach my $new_line (@stdout_in_lines){
			if ($new_line =~ m/^total/) {
				if ($new_line =~ m/0 errors/) {
					print BOLD YELLOW $new_line, "\n";
					next;
				}
				print BOLD RED $new_line, "\n";
			}
			else {
				print "$new_line\n";
			}
		}
	}
}
exit 0;
