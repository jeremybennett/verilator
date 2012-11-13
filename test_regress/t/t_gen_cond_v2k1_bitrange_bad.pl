#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2003 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

top_filename("t/t_gen_cond_v2k1_bitrange.v");

# This is a compile only test.
compile (
    verilator_flags2 => ["--language 1800-2005"],
    fails => 1
    );

ok(1);
1;
