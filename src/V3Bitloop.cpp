// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Analysis of bit- and part-select loops
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
// V3Bitloop's Transformations:
//
// Extract a graph of the *entire* netlist with cells expanded to show how
// variables are driven/drive through the logic. Similar to V3Gate, but
// instead of a node for each VarScope, there is a node for each unique bit-
// or part-select of a VarScope.
//
// We might be able to roll this into V3Gate in the future, but for
// development it is useful to make it a separate pass.
//
//*************************************************************************

#include "config_build.h"
#include "verilatedos.h"
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <list>

#include "V3Global.h"
#include "V3Bitloop.h"
#include "V3Ast.h"
#include "V3Graph.h"
#include "V3Const.h"
#include "V3Stats.h"

typedef list<AstNodeVarRef*> GateVarRefList;


//######################################################################
// Support classes

class BitloopEitherVertex : public V3GraphVertex {
    AstScope*	m_scopep;
public:
    BitloopEitherVertex(V3Graph* graphp, AstScope* scopep)
	: V3GraphVertex(graphp), m_scopep(scopep) {}
    virtual ~BitloopEitherVertex() {}
    // Accessors
    virtual string dotStyle() const { return ""; }
    AstScope* scopep() const { return m_scopep; }
};

class BitloopVarVertex : public BitloopEitherVertex {
    AstVarScope* m_varScp;
    bool	 m_isTop;
    bool	 m_isClock;
public:
    BitloopVarVertex(V3Graph* graphp, AstScope* scopep, AstVarScope* varScp)
	: BitloopEitherVertex(graphp, scopep), m_varScp(varScp), m_isTop(false)
	, m_isClock(false) {}
    virtual ~BitloopVarVertex() {}
    // Accessors
    AstVarScope* varScp() const { return m_varScp; }
    virtual string name() const {
	return (cvtToStr((void*)m_varScp) + " " + varScp()->name() + "\\n"
		+ m_varScp->fileline()->filebasename() + ":"
		+ cvtToStr(m_varScp->fileline()->lineno()));
    }
    virtual string dotColor() const { return "blue"; }
    bool isTop() const { return m_isTop; }
    void setIsTop() { m_isTop = true; }
    bool isClock() const { return m_isClock; }
    void setIsClock() { m_isClock = true; }
};

class BitloopLogicVertex : public BitloopEitherVertex {
    AstNode*	m_nodep;
    AstActive*	m_activep;	// Under what active; NULL is ok (under cfunc or such)
public:
    BitloopLogicVertex(V3Graph* graphp, AstScope* scopep, AstNode* nodep,
		    AstActive* activep)
	: BitloopEitherVertex(graphp,scopep), m_nodep(nodep), m_activep(activep) {}
    virtual ~BitloopLogicVertex() {}
    // Accessors
    virtual string name() const {
	return (cvtToStr((void *)m_nodep) + " " + m_nodep->typeName() + "@"
		+ scopep()->prettyName() + "\\n"
		+ m_nodep->fileline()->filebasename() + ":"
		+ cvtToStr(m_nodep->fileline()->lineno()));
    }
    virtual string dotColor() const { return "yellow"; }
    AstNode* nodep() const { return m_nodep; }
    AstActive* activep() const { return m_activep; }
};

//######################################################################
// Bitloop class functions

//! Visitor to build graph of bit loops
class BitloopVisitor : public AstNVisitor {
private:
    // NODE STATE
    //Entire netlist:
    // AstVarScope::user1p	-> BitloopVarVertex* for usage var, 0=not set yet
    // {statement}Node::user1p	-> BitloopLogicVertex* for this statement

    AstUser1InUse	m_inuser1;

    // STATE
    V3Graph		m_graph;	// Scoreboard of var usages/dependencies
    BitloopLogicVertex*	m_logicVertexp;	// Current statement being tracked, NULL=ignored
    AstScope*		m_scopep;	// Current scope being processed
    AstNodeModule*	m_modp;		// Current module
    AstActive*		m_activep;	// Current active
    bool		m_inSenItem;	// Underneath AstSenItem; any varrefs are clocks
    int                 m_lsb;		//!< LSB inside select
    int                 m_width;	//!< Width inside select (0 if none)

    // METHODS
    void iterateNewStmt(AstNode* nodep) {
	if (m_scopep) {
	    UINFO(4,"   STMT "<<nodep<<endl);
	    // m_activep is null under AstCFunc's, that's ok.
	    m_logicVertexp = new BitloopLogicVertex(&m_graph, m_scopep, nodep, m_activep);
	    nodep->iterateChildren(*this);
	    m_logicVertexp = NULL;
	}
    }

    BitloopVarVertex* makeVarVertex(AstVarScope* varscp) {
	BitloopVarVertex* vertexp = (BitloopVarVertex*)(varscp->user1p());
	if (!vertexp) {
	    UINFO(6,"New vertex "<<varscp<<endl);
	    vertexp = new BitloopVarVertex(&m_graph, m_scopep, varscp);
	    varscp->user1p(vertexp);
	}
	return vertexp;
    }

    void splitSignals();

    // VISITORS
    virtual void visit(AstNetlist* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	m_graph.dumpDotFilePrefixed("bitloop_pre");
	// Split per-bit sections into multiple vertices.
	splitSignals();
	m_graph.dumpDotFilePrefixed("bitloop_split");
    }
    virtual void visit(AstNodeModule* nodep, AstNUser*) {
	m_modp = nodep;
	nodep->iterateChildren(*this);
	m_modp = NULL;
    }
    virtual void visit(AstScope* nodep, AstNUser*) {
	UINFO(4," SCOPE "<<nodep<<endl);
	m_scopep = nodep;
	m_logicVertexp = NULL;
	nodep->iterateChildren(*this);
	m_scopep = NULL;
    }
    virtual void visit(AstActive* nodep, AstNUser*) {
	// Create required blocks and add to module
	UINFO(4,"  BLOCK  "<<nodep<<endl);
	m_activep = nodep;
	nodep->iterateChildren(*this);
	m_activep = NULL;
    }
    virtual void visit(AstNodeVarRef* nodep, AstNUser*) {
	if (m_scopep) {
	    if (!m_logicVertexp) nodep->v3fatalSrc("Var ref not under a logic block\n");
	    AstVarScope* varscp = nodep->varScopep();
	    if (!varscp) nodep->v3fatalSrc("Var didn't get varscoped in V3Scope.cpp\n");
	    if (m_width) {
		UINFO(0, "VarRef " << nodep->name() << ", lsbp: " << m_lsb
		      << ", width: " << m_width << endl);
	    }
	    else {
		UINFO(0, "VarRef " << nodep->name() << " w/o select" << endl);
	    }
	    BitloopVarVertex* vvertexp = makeVarVertex(varscp);
	    UINFO(5," VARREF to "<<varscp<<endl);
	    if (m_inSenItem) vvertexp->setIsClock();
	    // We use weight of one; if we ref the var more than once, when we
	    // simplify, the weight will increase
	    if (nodep->lvalue()) {
		new V3GraphEdge(&m_graph, m_logicVertexp, vvertexp, 1);
	    } else {
		new V3GraphEdge(&m_graph, vvertexp, m_logicVertexp, 1);
	    }
	}
    }
    virtual void visit(AstAlways* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstAlwaysPublic* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstCFunc* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstSenItem* nodep, AstNUser*) {
	// Note we look at only AstSenItems, not AstSenGate's
	// The gating term of a AstSenGate is normal logic
	m_inSenItem = true;
	if (m_logicVertexp) {  // Already under logic; presumably a SenGate
	    nodep->iterateChildren(*this);
	} else {  // Standalone item, probably right under a SenTree
	    iterateNewStmt(nodep);
	}
	m_inSenItem = false;
    }
    virtual void visit(AstSenGate* nodep, AstNUser*) {
	// First handle the clock part will be handled in a minute by visit
	// AstSenItem The logic gating term is delt with as logic
	iterateNewStmt(nodep);
    }
    virtual void visit(AstInitial* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstAssignAlias* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstAssignW* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstCoverToggle* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstTraceInc* nodep, AstNUser*) {
	iterateNewStmt(nodep);
    }
    virtual void visit(AstConcat* nodep, AstNUser*) {
	if (nodep->backp()->castNodeAssign() && nodep->backp()->castNodeAssign()->lhsp()==nodep) {
	    nodep->v3fatalSrc("Concat on LHS of assignment; V3Const should have deleted it\n");
	}
	nodep->iterateChildren(*this);
    }
    //! Record selector details for bit graph
    virtual void visit(AstSel *nodep, AstNUser*) {
	int  old_lsb = m_lsb;
	int  old_width = m_width;
	m_lsb = nodep->lsbConst();
	m_width = nodep->widthConst();
	UINFO(0, "Sel lsb: " << m_lsb << ", width: " << m_width << endl);
	nodep->iterateChildren(*this);
	m_lsb = old_lsb;
	m_width = old_width;
    }

    //--------------------
    // Default
    virtual void visit(AstNode* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }

public:
    // CONSTUCTORS
    BitloopVisitor(AstNode* nodep) {
	m_logicVertexp = NULL;
	m_scopep = NULL;
	m_modp = NULL;
	m_activep = NULL;
	m_inSenItem = false;
	m_lsb = 0;
	m_width = 0;
	nodep->accept(*this);
    }
    virtual ~BitloopVisitor() {
	// Add stats here if needed
    }
    static int debug() {
	static int level = -1;
	if (VL_UNLIKELY(level < 0)) {
	    level = v3Global.opt.debugSrcLevel(__FILE__);
	}
	return level;
    }
};

//----------------------------------------------------------------------

//! Rewrite the graph, splitting signals that are selected.

//! Many designs aggregate signals, and we cannot see the dependencies in the
//! netlist broken down into these sub-signals.

//! This is preliminary work towards dealing with UNOPTFLAT automatically.
void BitloopVisitor::splitSignals() {
    // Loop through all the vertices
    for (V3GraphVertex* itp = m_graph.verticesBeginp();
	 itp;
	 itp=itp->verticesNextp()) {
	if (BitloopVarVertex *vvertexp = dynamic_cast<BitloopVarVertex *>(itp)) {
	    // All VAR vertices are VarScopes. We need to look at the connected
	    // logic to work out which bits are being used!
	    string name = vvertexp->varScp()->varp()->name();
	    UINFO(0,"Vertex: " << name << endl);
	    for (V3GraphEdge *edgep = vvertexp->inBeginp();
		 edgep;
		 edgep = edgep->inNextp()) {
		// Sources are logic driving this variable as l-value. Only
		// possible logic elements are:
		// AstAlways
		// AstAlwaysPublic
		// AstCFunc
		// AstSenItem
		// AstSenGate
		// AstInitial
		// AstAssignAlias
		// AstAssignW
		// AstCoverToggle
		// AstAstTraceInc
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->fromp());
		AstNode *nodep = lvertexp->nodep();

		UINFO(0, "  Edge from " << nodep->typeName () << endl);

		// The edges into the logic node are the variables driving it
		// (i.e. r-values).
		for (V3GraphEdge *edge2p = lvertexp->inBeginp();
		     edge2p;
		     edge2p = edge2p->inNextp()) {
		    BitloopVarVertex *vvertex2p =
			dynamic_cast<BitloopVarVertex *>(edgep->top());
		    string name2 = vvertex2p->varScp()->varp()->name();
		    UINFO(0,"    Vertex: " << name2 << endl);
		}
	    }
	    for (V3GraphEdge *edgep = vvertexp->outBeginp();
		 edgep;
		 edgep = edgep->outNextp()) {
		// Sinks are logic driven by this variable as r-value. Only
		// possible logic elements are:
		// AstAlways
		// AstAlwaysPublic
		// AstCFunc
		// AstSenItem
		// AstSenGate
		// AstInitial
		// AstAssignAlias
		// AstAssignW
		// AstCoverToggle
		// AstAstTraceInc
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->top());
		AstNode *nodep = lvertexp->nodep();

		UINFO(0, "  Edge to " << nodep->typeName () << endl);
	    }
	}
    }
}


//######################################################################

//! Bitloop static method for invoking graph analysys
void V3Bitloop::bitloopAll(AstNetlist *nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    BitloopVisitor visitor (nodep);
}
