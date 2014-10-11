#!/usr/bin/perl -w

unless ( -d "./.git" ) {
	print "You should run this scipt in the top directory\n";
	exit 1;
}

my $checkpath = "flo-kernel/scripts/checkpatch.pl";
my $out = `git diff --name-only 37dfc240b08b0889272f1f4510bf5690ceacdd8f HEAD`;
my @lines = split /\n/, $out;
foreach my $line (@lines) {
	unless ( -B $line or $line eq "flo-kernel/arch/arm/include/asm/unistd.h") {
		$stdout = `$checkpath -terse -no-tree -f $line`;
		print $stdout;
	}
}
exit 0;
#print $out;
