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

my $hash = "";
$hash = $ARGV[0] ? $ARGV[0] : "37dfc240b08b0889272f1f4510bf5690ceacdd8f";
#unistd.h has problem with the + signs
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
				if ($new_line =~ m/0 errors, 0 warnings/) {
					print BOLD GREEN $new_line, "\n";
					next;
				}
				if ($new_line =~ m/0 errors, \d* warnings/) {
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
#print $out;
