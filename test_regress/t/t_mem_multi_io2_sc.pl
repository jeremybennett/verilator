#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003-2009 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

top_filename("t/t_mem_multi_io2.v");

$Self->{vlt} or $Self->skip("Verilator only test");

compile (
	 make_top_shell => 0,
	 make_main => 0,
	 verilator_flags2 => ["--exe $Self->{t_dir}/t_mem_multi_io2.cpp --sc -Oi"],
    );

execute (
	 check_finished=>1,
    );

ok(1);
1;
