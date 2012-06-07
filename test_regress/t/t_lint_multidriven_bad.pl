#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2008 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

$Self->{vlt} or $Self->skip("Verilator only test");

compile (
    v_flags2 => ["--lint-only"],
    fails=>1,
    verilator_make_gcc => 0,
    make_top_shell => 0,
    make_main => 0,
    expect=>
'%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: Signal has multiple driving blocks: v.mem
%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: ... Location of first driving block
%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: ... Location of other driving block
%Warning-MULTIDRIVEN: Use ".*" and lint_on around source to disable this message.
%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: Signal has multiple driving blocks: out2
%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: ... Location of first driving block
%Warning-MULTIDRIVEN: t/t_lint_multidriven_bad.v:\d+: ... Location of other driving block
%Error: Exiting due to.*',
    );

ok(1);
1;
