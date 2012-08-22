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
#include "V3GraphAlg.h"
#include "V3Const.h"
#include "V3Stats.h"


// #############################################################################
// Graph Support classes

//! Base class for logic and variable vertices
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

//! Class for a variable vertex

//! These are always associated with a VarScope node. Each VarScope AST node
//! has a single entry in the graph originally. We'll rewrite to split where
//! the node is referred to by different sizes in graphs.
class BitloopVarVertex : public BitloopEitherVertex {
    AstVarScope* m_varScp;	//!< The AstVarScope associated with this vertex
    bool	 m_isTop;	//!< TRUE if we are TOP scope
    bool	 m_isClock;	//!< TRUE for clocked cars
public:
    //! Constructor
    BitloopVarVertex(V3Graph* graphp, AstScope* scopep, AstVarScope* varScp)
	: BitloopEitherVertex(graphp, scopep), m_varScp(varScp), m_isTop(false)
	, m_isClock(false) {}
    //! Destructor (currently empty)
    virtual ~BitloopVarVertex() {}
    // Accessors
    AstVarScope* varScp() const { return m_varScp; }
    virtual string name() const {
	return (cvtToStr((void*)m_varScp) + " " + varScp()->prettyName() + "\\n"
		+ m_varScp->fileline()->filebasename() + ":"
		+ cvtToStr(m_varScp->fileline()->lineno()));
    }
    virtual string dotColor() const { return "blue"; }
    bool isTop() const { return m_isTop; }
    void setIsTop() { m_isTop = true; }
    bool isClock() const { return m_isClock; }
    void setIsClock() { m_isClock = true; }
};

//! Class for a logic vertex

//! The logic entities which can drive or be driven by a net are 
//! - AstAlways
//! - AstAlwaysPublic
//! - AstCFunc
//! - AstSenItem
//! - AstSenGate
//! - AstInitial
//! - AstAssignAlias
//! - AstAssignW
//! - AstCoverToggle
//! - AstAstTraceInc
class BitloopLogicVertex : public BitloopEitherVertex {
    AstNode*	m_nodep;	//!< The logic node associated with this vertex
    AstActive*	m_activep;	//!< Under what active; NULL is ok (under CFUNC
				//!< or such)
public:
    //! Constructor
    BitloopLogicVertex(V3Graph* graphp, AstScope* scopep, AstNode* nodep,
		       AstActive* activep)
	: BitloopEitherVertex(graphp,scopep), m_nodep(nodep)
	, m_activep(activep) {}
    //! Destructor (currently empty)
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

//! Class for an edge between logic and var nodes

//! Derived from standard base class to capture bit- or part-select data of
//! the reference.
class BitloopEdge : public V3GraphEdge {
    int  m_lsb;		//!< LSB of select
    int  m_width;	//!< Width of select (zero if no select)

public:
    //! Constructor (sets LSB and width)
    BitloopEdge(V3Graph *graphp, BitloopEitherVertex *fromp,
		BitloopEitherVertex *top, int weight, int lsb, int width)
	: V3GraphEdge (graphp, fromp, top, weight), m_lsb(lsb)
	, m_width(width) {}
    //! Destructor (currently empty)
    virtual ~BitloopEdge() {}
    //! Label accessor is any selection specified
    virtual string dotLabel() const {
	if (m_width) {
	    return ("[" + cvtToStr(m_lsb + m_width - 1) + ":" + cvtToStr(m_lsb)
		    + "]");
	} else {
	    return "";
	}
    }
    int lsb() { return m_lsb; }
    void lsb (int lsb) { m_lsb = lsb; }
    int width() { return m_width; }
    void width (int width) { m_width = width; }
};


// #############################################################################
// Algorithm support classes

//! Class to report the variable relationships in the origial graph.
class GraphReportOrigVars : GraphAlg {

private:
    //! Iterate all vertices which are VarRef nodes
    void main () {
	for (V3GraphVertex* itp = m_graphp->verticesBeginp();
	     itp;
	     itp=itp->verticesNextp()) {
	    // All VAR vertices are VarScopes. We need to look at the
	    // connected logic to work out which bits are being used
	    if (BitloopVarVertex *vp = dynamic_cast<BitloopVarVertex *>(itp)) {
		iterateVarVertex (vp);
	    }
	}
    }
    //! Find the logic connected to a var, and then all the driving/driven vars
    void iterateVarVertex (BitloopVarVertex *vvertexp) {
	string name = vvertexp->varScp()->varp()->prettyName();
	UINFO(0, name << endl);
	for (V3GraphEdge *edgep = vvertexp->inBeginp();
	     edgep;
	     edgep = edgep->inNextp()) {
	    if (followEdge(edgep)) {
		// Sources are logic driving this variable as l-value.
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->fromp());
		iterateLogicFromVertex(lvertexp);
	    }
	}
	for (V3GraphEdge *edgep = vvertexp->outBeginp();
	     edgep;
	     edgep = edgep->outNextp()) {
	    if (followEdge(edgep)) {
		// Sinks are logic driven by this variable as r-value.
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->top());
		iterateLogicToVertex(lvertexp);
	    }
	}
    }
    //! Iterate the edges of a logic vertext to find driving vars
    void iterateLogicFromVertex (BitloopLogicVertex *lvertexp) {
	for (V3GraphEdge *edgep = lvertexp->inBeginp();
	     edgep;
	     edgep = edgep->inNextp()) {
	    BitloopVarVertex *vvertexp =
		dynamic_cast<BitloopVarVertex *>(edgep->fromp());
	    string name = vvertexp->varScp()->varp()->prettyName();
	    UINFO(0,("  <- ") << name << endl);
	}
    }
    //! Iterate the edges of a logic vertext to find driven vars
    void iterateLogicToVertex (BitloopLogicVertex *lvertexp) {
	for (V3GraphEdge *edgep = lvertexp->outBeginp();
	     edgep;
	     edgep = edgep->outNextp()) {
	    BitloopVarVertex *vvertexp =
		dynamic_cast<BitloopVarVertex *>(edgep->top());
	    string name = vvertexp->varScp()->varp()->prettyName();
	    UINFO(0,("  -> ") << name << endl);
	}
    }
	    

public:
    //! Constructor
    GraphReportOrigVars(V3Graph* graphp)
	: GraphAlg(graphp, &V3GraphEdge::followAlwaysTrue) {
	main();
    }
    //! Destructor (currently empty)
    ~GraphReportOrigVars() {}
};


//! Class to strip out unneeded logic vertices.

//! Logic nodes with no edges connecting them can be removed. Logic nodes
//! connecting vars can be removed, with edges directly connecting the vars.

//! The only logic nodes which remain are sinks and sources.
class GraphStripLogic : GraphAlg {

private:
    //! Iterate all vertices which are logic nodes
    void main () {
	for (V3GraphVertex* itp = m_graphp->verticesBeginp();
	     itp;
	     itp=itp->verticesNextp()) {
	    // All VAR vertices are VarScopes. We need to look at the
	    // connected logic to work out which bits are being used
	    if (BitloopVarVertex *lp =
		dynamic_cast<BitloopVarVertex *>(itp)) {
		iterateVarVertex (lp);
	    }
	}
    }
    //! Try to eliminate logic nodes
    void iterateVarVertex (BitloopVarVertex *vvertexp) {
	string name = vvertexp->varScp()->varp()->prettyName();
	UINFO(0, name << endl);
	for (V3GraphEdge *edgep = vvertexp->inBeginp();
	     edgep;
	     edgep = edgep->inNextp()) {
	    if (followEdge(edgep)) {
		// Sources are logic driving this variable as l-value.
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->fromp());
		iterateLogicFromVertex(lvertexp);
	    }
	}
	for (V3GraphEdge *edgep = vvertexp->outBeginp();
	     edgep;
	     edgep = edgep->outNextp()) {
	    if (followEdge(edgep)) {
		// Sinks are logic driven by this variable as r-value.
		BitloopLogicVertex *lvertexp =
		    dynamic_cast<BitloopLogicVertex *>(edgep->top());
		iterateLogicToVertex(lvertexp);
	    }
	}
    }
    //! Iterate the edges of a logic vertext to find driving vars
    void iterateLogicFromVertex (BitloopLogicVertex *lvertexp) {
	for (V3GraphEdge *edgep = lvertexp->inBeginp();
	     edgep;
	     edgep = edgep->inNextp()) {
	    BitloopVarVertex *vvertexp =
		dynamic_cast<BitloopVarVertex *>(edgep->fromp());
	    string name = vvertexp->varScp()->varp()->prettyName();
	    UINFO(0,("  <- ") << name << endl);
	}
    }
    //! Iterate the edges of a logic vertext to find driven vars
    void iterateLogicToVertex (BitloopLogicVertex *lvertexp) {
	for (V3GraphEdge *edgep = lvertexp->outBeginp();
	     edgep;
	     edgep = edgep->outNextp()) {
	    BitloopVarVertex *vvertexp =
		dynamic_cast<BitloopVarVertex *>(edgep->top());
	    string name = vvertexp->varScp()->varp()->prettyName();
	    UINFO(0,("  -> ") << name << endl);
	}
    }
	    

public:
    //! Constructor
    GraphStripLogic(V3Graph* graphp)
	: GraphAlg(graphp, &V3GraphEdge::followAlwaysTrue) {
	main();
    }
    //! Destructor (currently empty)
    ~GraphStripLogic() {}
};


// #############################################################################
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

    // VISITORS
    virtual void visit(AstNetlist* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	GraphReportOrigVars alg (&m_graph);
	m_graph.dumpDotFilePrefixed("bitloop_pre");
	GraphStripLogic alg2 (&m_graph);
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
	    BitloopVarVertex* vvertexp = makeVarVertex(varscp);
	    UINFO(5," VARREF to "<<varscp<<endl);
	    if (m_inSenItem) vvertexp->setIsClock();
	    // Width = 0 means we didn't see a SELECT, so use the natural
	    // width and lsb of the Var's basic type
	    if (0 == m_width) {
		AstBasicDType *basicTypep = nodep->dtypep()->basicp();
		if (basicTypep->isRanged() && !basicTypep->rangep()) {
		    m_lsb = basicTypep->lsb();
		    m_width = basicTypep->msb() - m_lsb + 1;
		}
	    }
	    // We use weight of one; if we ref the var more than once, when we
	    // simplify, the weight will increase
	    if (nodep->lvalue()) {
		new BitloopEdge(&m_graph, m_logicVertexp, vvertexp, 1, m_lsb,
				m_width);
	    } else {
		new BitloopEdge(&m_graph, vvertexp, m_logicVertexp, 1, m_lsb,
				m_width);
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


// #############################################################################

//! Bitloop static method for invoking graph analysys
void V3Bitloop::bitloopAll(AstNetlist *nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    BitloopVisitor visitor (nodep);
}
