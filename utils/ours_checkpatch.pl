#!/usr/bin/perl -w

if ($#ARGV == 0 and ($ARGV[0] =~ m/-h/ or $ARGV[0] !~ m/^[0-9a-f]{40}$/)) {
	print "Usage: ours_checkpatch.pl SHA\n";
	exit 0;
}

use Term::ANSIColor qw(:constants);
local $Term::ANSIColor::AUTORESET = 1;

unless ( -d "./.git" ) {
	print "You should run this scipt in the top directory of the kernel\n";
	exit 1;
}

my $rev_list;
unless (defined $ARGV[0]) {
	print ON_RED "Trying to find your first commit\n";
	print ON_RED "Scripts assumes TAs have only one commit";
	$rev_list = `git rev-list HEAD --reverse`;
	print "\n";
}

@rev_list_split = split /\n/, $rev_list;
my $hash = "";
$hash = $ARGV[0] ? $ARGV[0] : $rev_list_split[1];
#unistd.h has problem with the + signs
print GREEN "Starting from commit with hash $hash\n";
my $skip = "flo-kernel/arch/arm/include/asm/unistd.h";
my $checkpath = "flo-kernel/scripts/checkpatch.pl";
my $out = `git diff --name-only $hash HEAD`;
my @lines = split /\n/, $out;
foreach my $line (@lines) {
	unless ( -B $line or $line eq $skip) {
		$stdout = `$checkpath -terse -no-tree -f $line`;
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
