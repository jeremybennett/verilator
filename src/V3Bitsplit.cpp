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
// V3Bitsplit's Transformations:
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
#include "V3Bitsplit.h"
#include "V3Ast.h"
#include "V3Graph.h"
#include "V3GraphAlg.h"
#include "V3Const.h"
#include "V3Stats.h"


// #############################################################################
// Graph Support classes

//! Base class for logic and variable vertices
class BitsplitEitherVertex : public V3GraphVertex {
    AstScope*	m_scopep;

public:
    BitsplitEitherVertex(V3Graph* graphp, AstScope* scopep)
	: V3GraphVertex(graphp), m_scopep(scopep) {}
    virtual ~BitsplitEitherVertex() {}
    // Accessors
    virtual string dotStyle() const { return ""; }
    AstScope* scopep() const { return m_scopep; }
};

//! Class for a variable vertex

//! These are always associated with a VarScope node. Each VarScope AST node
//! has a single entry in the graph originally.

//! We then duplicate the node, so it has one copy for each range (LSB and
//! width) in which it is used. At which point we associate the range with the
//! vertex, rather than the edge.
class BitsplitVarVertex : public BitsplitEitherVertex {
    AstVarScope* m_varScp;	//!< The AstVarScope associated with this vertex
    int		 m_lsb;		//!< LSB of range
    int          m_width;	//!< Width (0 if not set yet).
public:
    //! Constructor based on a varScope
    BitsplitVarVertex(V3Graph* graphp, AstScope* scopep, AstVarScope* varScp,
		     int lsb = 0, int width = 0)
	: BitsplitEitherVertex(graphp, scopep), m_varScp(varScp), m_lsb(lsb)
	, m_width(width) {}
    //! Constructor based on existing vertex
    BitsplitVarVertex(V3Graph* graphp, BitsplitVarVertex *vvp, int lsb = 0,
		     int width = 0)
	: BitsplitEitherVertex(graphp, vvp->scopep())
	, m_varScp(vvp->varScp()), m_lsb(lsb), m_width(width) {}
    //! Destructor (currently empty)
    virtual ~BitsplitVarVertex() {}
    // Accessors
    AstVarScope* varScp() const { return m_varScp; }
    virtual string name() const {
	string range = m_width ? "[" + cvtToStr(m_lsb + m_width - 1) + ":"
	    + cvtToStr(m_lsb) + "]"
	    : "";
	return (varScp()->prettyName() + range + "\\n"
		+ m_varScp->fileline()->filebasename() + ":"
		+ cvtToStr(m_varScp->fileline()->lineno()));
    }
    virtual string dotColor() const { return "blue"; }
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
class BitsplitLogicVertex : public BitsplitEitherVertex {
    AstNode*	m_nodep;	//!< The logic node associated with this vertex
public:
    //! Constructor
    BitsplitLogicVertex(V3Graph* graphp, AstScope* scopep, AstNode* nodep)
	: BitsplitEitherVertex(graphp, scopep), m_nodep(nodep) {}
    //! Destructor (currently empty)
    virtual ~BitsplitLogicVertex() {}
    // Accessors
    virtual string name() const {
	return (cvtToStr(m_nodep->typeName()) + "@"
		+ scopep()->prettyName() + "\\n"
		+ m_nodep->fileline()->filebasename() + ":"
		+ cvtToStr(m_nodep->fileline()->lineno()));
    }
    AstNode* nodep() const { return m_nodep; }
    virtual string dotColor() const { return "yellow"; }
};

//! Class for an edge between logic and var nodes

//! Derived from standard base class to capture bit- or part-select data of
//! the reference.
class BitsplitEdge : public V3GraphEdge {
    int    m_lsb;		//!< LSB of select
    int    m_width;		//!< Width of select (zero if no select)
    string m_dotLabel;		//!< User specified DOT label
public:
    //! Constructor (sets LSB and width), weight always 1
    BitsplitEdge(V3Graph *graphp, BitsplitEitherVertex *fromp,
		BitsplitEitherVertex *top, int lsb = 0,
		int width = 0)
	: V3GraphEdge(graphp, fromp, top, 1), m_lsb(lsb)
	, m_width(width), m_dotLabel("") {}
    //! Destructor (currently empty)
    virtual ~BitsplitEdge() {}
    //! Label accessor is any selection specified, else any user specified
    //! string.
    virtual string dotLabel() const {
	if (m_width) {
	    return ("[" + cvtToStr(m_lsb + m_width - 1) + ":" + cvtToStr(m_lsb)
		    + "]");
	} else {
	    return m_dotLabel;
	}
    }
    virtual void dotLabel(string s) { m_dotLabel = s; }
    int lsb() { return m_lsb; }
    void lsb(int lsb) { m_lsb = lsb; }
    int width() { return m_width; }
    void width(int width) { m_width = width; }
};


// #############################################################################
// Algorithm support classes

//! Class to split variable vertices

//! Where variable vertices have edges of different ranges, duplicate the
//! vertex so that each vertex only has edges of a particular range. Set the
//! range (LSB and width) of the vertex and remove it from the edge.

//! Since we are adding vertices, we need to mark them, or iterators may never
//! terminate, using vertex::user
//!  - existing var vertices OLD_VAR_VERTEX
//!  - new var vertices NEW_VAR_VERTEX
class GraphSplitVars : GraphAlg {

private:
    typedef pair<int, int> RangeType;	//!< LSB, width
    typedef std::map<RangeType, BitsplitVarVertex *> VarVertexMapType;

    // Enumeration for types of vertices.
    enum vt {
	OLD_VAR_VERTEX,		//!< Mark an existing vertex
	NEW_VAR_VERTEX		//!< Mark a new vertex
    };

    //! Iterate all vertices which are variable nodes to split the
    //! node. Because we are adding nodes, we need to distinguish between old
    //! and new nodes when adding.
    void main() {
	// Mark all existing var vertices
	for (V3GraphVertex* itp = m_graphp->verticesBeginp();
	     itp;
	     itp=itp->verticesNextp()) {
	    if (BitsplitVarVertex *vvp = dynamic_cast<BitsplitVarVertex *>(itp)) {
		vvp->user(OLD_VAR_VERTEX);
	    }
	}
	// Iterate to split the "old" vertices. We use a while loop, because
	// we need care at the end of each iteration to select the next
	// iteration *before* deleting the old node.
	V3GraphVertex* itp = m_graphp->verticesBeginp();
	while (itp) {
	    BitsplitVarVertex *vvp = dynamic_cast<BitsplitVarVertex *>(itp);
	    if (vvp && (OLD_VAR_VERTEX == vvp->user())) {
		// Select the next vertex now, since we'll delete the old
		// vertex in iterateVarVertex.
		itp=itp->verticesNextp();
		iterateVarVertex(vvp);
	    } else {
		itp=itp->verticesNextp();
	    }
	}
    }
    //! Split vertices for each edge

    //! Iterate through all the edges, if an edge range does not match an
    //! existing edge (in a map), then create a copy of the vertex with the
    //! new range.

    //! No one will use the vertex after this, so we are allowed to delete it.
    void iterateVarVertex(BitsplitVarVertex *origVarVertexp) {
	// We don't try to modify the existing vertex and edges, or we'll
	// confuse the iteration. We just make copies as we need them, then
	// delete the originals.
	VarVertexMapType  vvmap;

	// In edges
	for (BitsplitEdge *origEdgep =
		 dynamic_cast<BitsplitEdge *>(origVarVertexp->inBeginp());
	     origEdgep;
	     origEdgep = dynamic_cast<BitsplitEdge *>(origEdgep->inNextp())) {
	    if (followEdge(origEdgep)) {
		int lsb = origEdgep->lsb();
		int width = origEdgep->width();
		RangeType range(lsb, width);
		BitsplitVarVertex *newVarVertexp;
		if (vvmap.count(range)) {
		    newVarVertexp = vvmap[range];
		} else {
		    newVarVertexp = new BitsplitVarVertex(m_graphp,
							 origVarVertexp,
							 lsb, width);
		    newVarVertexp->user(NEW_VAR_VERTEX);
		    vvmap[range] = newVarVertexp;
		}
		BitsplitLogicVertex *fromp =
		    dynamic_cast<BitsplitLogicVertex *>(origEdgep->fromp());
		new BitsplitEdge(m_graphp, fromp, newVarVertexp);
	    }
	}
	// Out edges
	for (BitsplitEdge *origEdgep =
		 dynamic_cast<BitsplitEdge *>(origVarVertexp->outBeginp());
	     origEdgep;
	     origEdgep = dynamic_cast<BitsplitEdge *>(origEdgep->outNextp())) {
	    if (followEdge(origEdgep)) {
		int lsb = origEdgep->lsb();
		int width = origEdgep->width();
		RangeType range(lsb, width);
		BitsplitVarVertex *newVarVertexp;
		if (vvmap.count(range)) {
		    newVarVertexp = vvmap[range];
		} else {
		    newVarVertexp = new BitsplitVarVertex(m_graphp,
							 origVarVertexp,
							 lsb, width);
		    newVarVertexp->user(NEW_VAR_VERTEX);
		    vvmap[range] = newVarVertexp;
		}
		BitsplitLogicVertex *top =
		    dynamic_cast<BitsplitLogicVertex *>(origEdgep->top());
		new BitsplitEdge(m_graphp, newVarVertexp, top);
	    }
	}
	// Delete the old vertex
	origVarVertexp->unlinkDelete(m_graphp);
    }
	    

public:
    //! Constructor
    GraphSplitVars(V3Graph* graphp)
	: GraphAlg(graphp, &V3GraphEdge::followAlwaysTrue) {
	main();
    }
    //! Destructor (currently empty)
    ~GraphSplitVars() {}
};


//! Class to strip out logic vertices.

//! This is the code from rerouteEdges() in V3Graph.cpp, but using
//! BitsplitEdge for rerouting.
class GraphStripLogic : GraphAlg {

private:
    //! Iterate all vertices which are logic nodes. We use a while loop, so we
    //! can make sure we advance the iteration before deleting a node.
    void main() {
	V3GraphVertex* itp = m_graphp->verticesBeginp();
	while (itp) {
	    BitsplitLogicVertex *lp = dynamic_cast<BitsplitLogicVertex *>(itp);
	    itp=itp->verticesNextp();
	    if (lp) deleteLogicVertex(lp);
	}
    }
    //! Eliminate a logic node.

    //!  Label the edge with the name of the logic node being eliminated. We
    //!can safely delete it, since it is not used after this point.
    void deleteLogicVertex(BitsplitLogicVertex *lvertexp) {
	// Make new edges for each from/to pair
	for (V3GraphEdge* iedgep = lvertexp->inBeginp();
	     iedgep;
	     iedgep=iedgep->inNextp()) {
	    for (V3GraphEdge* oedgep = lvertexp->outBeginp();
		 oedgep;
		 oedgep=oedgep->outNextp()) {
		BitsplitVarVertex *fromp =
		    dynamic_cast<BitsplitVarVertex *>(iedgep->fromp());
		BitsplitVarVertex *top =
		    dynamic_cast<BitsplitVarVertex *>(oedgep->top());
		if (fromp && top) {
		    BitsplitEdge *edgep = new BitsplitEdge(m_graphp, fromp, top);
		    edgep->dotLabel(lvertexp->name());
		}
	    }
	}
	// Remove old vertex
	lvertexp->unlinkDelete(m_graphp);
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


//! Merge redundant edges

//! A simplified version of GraphRemoveRedundant, but which also merges the
//! edge labels.

//! By this time the vertices are all variable vertices. userp is used to mark
//! the vertices we have already found an edge to.
class GraphMergeEdges : GraphAlg {

private:
    void main() {
	for (V3GraphVertex* vertexp = m_graphp->verticesBeginp();
	     vertexp;
	     vertexp=vertexp->verticesNextp()) {
	    vertexIterate(vertexp);
	}
    }
    void vertexIterate(V3GraphVertex* vertexp) {
	// Clear marks
	for (V3GraphEdge* edgep = vertexp->outBeginp();
	     edgep;
	     edgep=edgep->outNextp()) {
	    edgep->top()->userp(NULL);
	}
	// Mark edges and detect duplications
	BitsplitEdge *edgep =
	    dynamic_cast<BitsplitEdge *>(vertexp->outBeginp());

	while (edgep) {
	    BitsplitEdge *nextp =
		dynamic_cast<BitsplitEdge *>(edgep->outNextp());
	    if (followEdge(edgep)) {
		V3GraphVertex *outVertexp = edgep->top();
		BitsplitEdge *prevEdgep = (BitsplitEdge *)outVertexp->userp();
		if (!prevEdgep) { // No previous assignment
		    outVertexp->userp(edgep);
		} else { // Duplicate. Merge labels and delete edge
		    string label = edgep->dotLabel();
		    string prevLabel = prevEdgep->dotLabel();
		    if (prevLabel.find(label) == string::npos) {
			prevEdgep->dotLabel(label + "\\n" + prevLabel);
		    }
		    edgep->unlinkDelete();
		    edgep = NULL;
		}
		edgep = nextp;
	    }
	}
    }
public:
    GraphMergeEdges(V3Graph* graphp)
	: GraphAlg(graphp, &V3GraphEdge::followAlwaysTrue) {
	main();
    }
    ~GraphMergeEdges() {}
};


// #############################################################################
// Bitsplit class functions

//! Visitor to build graph of bit loops

//! Node state used as follows
//! * AstVarScope::user1p -> BitsplitVarVertex* for usage var, 0=not set yet
class BitsplitVisitor : public AstNVisitor {
private:
    AstUser1InUse	m_inuser1;

    // STATE
    V3Graph		 m_graph;	 //!< Graph of var usages/dependencies
    BitsplitLogicVertex *m_logicVertexp; //!< Current statement being tracked,
					 //!< NULL=ignored
    AstScope		*m_scopep;	 //!< Current scope being processed
    int                  m_lsb;		 //!< LSB inside select
    int                  m_width;	 //!< Width inside select (0 if none)

    // METHODS
    void iterateNewStmt(AstNode* nodep) {
	if (m_scopep) {
	    UINFO(4,"   STMT "<<nodep<<endl);
	    m_logicVertexp = new BitsplitLogicVertex(&m_graph, m_scopep, nodep);
	    nodep->iterateChildren(*this);
	    m_logicVertexp = NULL;
	}
    }

    BitsplitVarVertex* makeVarVertex(AstVarScope* varscp) {
	BitsplitVarVertex* vertexp = (BitsplitVarVertex*)(varscp->user1p());
	if (!vertexp) {
	    UINFO(6,"New vertex "<<varscp<<endl);
	    vertexp = new BitsplitVarVertex(&m_graph, m_scopep, varscp);
	    varscp->user1p(vertexp);
	}
	return vertexp;
    }

    // VISITORS
    virtual void visit(AstNetlist* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
	m_graph.dumpDotFilePrefixed("bitsplit_pre");
	GraphSplitVars a1(&m_graph);
	GraphStripLogic a2(&m_graph);
	GraphMergeEdges a3(&m_graph);
	m_graph.dumpDotFilePrefixed("bitsplit_split");
    }
    virtual void visit(AstNodeModule* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
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
	nodep->iterateChildren(*this);
    }
    virtual void visit(AstNodeVarRef* nodep, AstNUser*) {
	if (m_scopep) {
	    if (!m_logicVertexp) nodep->v3fatalSrc("Var ref not under a logic block\n");
	    AstVarScope* varscp = nodep->varScopep();
	    if (!varscp) nodep->v3fatalSrc("Var didn't get varscoped in V3Scope.cpp\n");
	    BitsplitVarVertex* vvertexp = makeVarVertex(varscp);
	    UINFO(5," VARREF to "<<varscp<<endl);
	    // Width = 0 means we didn't see a SELECT, so use the natural
	    // width and lsb of the Var's basic type
	    int lsb   = m_lsb;		// What we will actually use
	    int width = m_width;
	    if (0 == m_width) {
		AstBasicDType *basicTypep = nodep->dtypep()->basicp();
		if (basicTypep->isRanged() && !basicTypep->rangep()) {
		    lsb = basicTypep->lsb();
		    width = basicTypep->msb() - lsb + 1;
		}
	    }
	    // We use weight of one; if we ref the var more than once, when we
	    // simplify, the weight will increase
	    if (nodep->lvalue()) {
		new BitsplitEdge(&m_graph, m_logicVertexp, vvertexp, lsb, width);
	    } else {
		new BitsplitEdge(&m_graph, vvertexp, m_logicVertexp, lsb, width);
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
	if (m_logicVertexp) {  // Already under logic; presumably a SenGate
	    nodep->iterateChildren(*this);
	} else {  // Standalone item, probably right under a SenTree
	    iterateNewStmt(nodep);
	}
    }
    //! AstSenItem The logic gating term is dealt with as logic
    virtual void visit(AstSenGate* nodep, AstNUser*) {
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
	int  oldLsb = m_lsb;
	int  oldWidth = m_width;
	// Range only meaningful if LSB and width are *both* constant
	if (nodep->lsbp()->castConst() && nodep->widthp()->castConst()) {
	    m_lsb = nodep->lsbConst();
	    m_width = nodep->widthConst();
	} else {
	    m_lsb = 0;
	    m_width = 0;
	}
	nodep->iterateChildren(*this);
	m_lsb = oldLsb;
	m_width = oldWidth;
    }
    //! Default visitor
    virtual void visit(AstNode* nodep, AstNUser*) {
	nodep->iterateChildren(*this);
    }

public:
    //! Constructor
    BitsplitVisitor(AstNode* nodep)
	: m_logicVertexp(NULL), m_scopep(NULL), m_lsb(0), m_width(0) {
	nodep->accept(*this);
    }
    //! Destructor
    virtual ~BitsplitVisitor() {
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

//! Bitsplit static method for invoking graph analysys
void V3Bitsplit::bitsplitAll(AstNetlist *nodep) {
    UINFO(2,__FUNCTION__<<": "<<endl);
    BitsplitVisitor visitor(nodep);
}
