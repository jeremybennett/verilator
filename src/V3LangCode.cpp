// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Options parsing
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2003-2012 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"

#include <V3Global.h>
#include <V3Options.h>


//######################################################################
// Language class

V3LangCode::V3LangCode (const char* textp) {
    // Return code for given string, or ERROR, which is a bad code
    for (int codei=V3LangCode::L_ERROR; codei<V3LangCode::_ENUM_END; ++codei) {
	V3LangCode code = (V3LangCode)codei;
	if (0==strcasecmp(textp,code.ascii())) {
	    m_e = code; return;
	}
    }
    m_e = V3LangCode::L_ERROR;
}
