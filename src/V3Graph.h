// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Graph optimizations
//
// Code available from: http://www.veripool.org/verilator
//
//*************************************************************************
//
// Copyright 2003-2013 by Wilson Snyder.  This program is free software; you can
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

#ifndef _V3GRAPH_H_
#define _V3GRAPH_H_ 1
#include "config_build.h"
#include "verilatedos.h"
#include "V3Error.h"
#include "V3List.h"
#include "V3Ast.h"
#include <vector>
#include <algorithm>

class V3Graph;
class V3GraphVertex;
class V3GraphEdge;
class GraphAcycEdge;
class OrderEitherVertex;
class OrderLogicVertex;

//=============================================================================
// Most graph algorithms accept an arbitrary function that returns
// True for those edges we should honor.

typedef bool (*V3EdgeFuncP)(const V3GraphEdge* edgep);

//============================================================================

class V3Graph {
private:
    // STATE
    V3List<V3GraphVertex*> m_vertices;	// All vertices
    static int s_debug;
protected:
    friend class V3GraphVertex;    friend class V3GraphEdge;
    friend class GraphAcyc;
    // METHODS
    void acyclicDFS();
    void acyclicDFSIterate(V3GraphVertex *vertexp, int depth, uint32_t currentRank);
    void acyclicCut();
    void acyclicLoop(V3GraphVertex* vertexp, int depth);
    double orderDFSIterate(V3GraphVertex* vertexp);
    void dumpEdge(ostream& os, V3GraphVertex* vertexp, V3GraphEdge* edgep);
    void verticesUnlink() { m_vertices.reset(); }
    // ACCESSORS
    static int debug();
public:
    V3Graph();
    virtual ~V3Graph();
    static void debug(int level) { s_debug = level; }
    virtual string dotRankDir() { return "TB"; }	// rankdir for dot plotting

    // METHODS
    void clear();	// Empty it of all vertices/edges, as if making a new object
    void clearColors();

    V3GraphVertex* verticesBeginp() const { return m_vertices.begin(); }

    // METHODS - ALGORITHMS

    /// Assign same color to all vertices in the same weakly connected component
    /// Thus different color if there's no edges between the two subgraphs
    void weaklyConnected(V3EdgeFuncP edgeFuncp);

    /// Assign same color to all vertices that are strongly connected
    /// Thus different color if there's no directional circuit within the subgraphs.
    /// (I.E. all loops will occur within each color, not between them.)
    void stronglyConnected(V3EdgeFuncP edgeFuncp);

    /// Assign same color to all destination vertices that have same
    /// subgraph feeding into them
    /// (I.E. all "from" nodes are common within each color)
    /// See V3ClkGater if this is needed again; it got specialized

    /// Assign a ordering number to all vertexes in a tree.
    /// All nodes with no inputs will get rank 1
    void rank(V3EdgeFuncP edgeFuncp);
    void rank();

    /// Sort all vertices and edges using the V3GraphVertex::sortCmp() function
    void sortVertices();
    /// Sort all edges and edges using the V3GraphEdge::sortCmp() function
    void sortEdges();

    /// Order all vertices by rank and fanout, lowest first
    /// Sort all vertices by rank and fanout, lowest first
    /// Sort all edges by weight, lowest first
    void order();

    /// Make acyclical (into a tree) by breaking a minimal subset of cutable edges.
    void acyclic(V3EdgeFuncP edgeFuncp);

    /// Delete any nodes with only outputs
    void deleteCutableOnlyEdges();

    /// Any cutable edged become non-cutable
    void makeEdgesNonCutable(V3EdgeFuncP edgeFuncp);

    /// Remove any redundant edges, weights become MAX of any other weight
    void removeRedundantEdges(V3EdgeFuncP edgeFuncp);

    /// Remove any redundant edges, weights become SUM of any other weight
    void removeRedundantEdgesSum(V3EdgeFuncP edgeFuncp);

    /// Call loopsVertexCb on any loops starting where specified
    void reportLoops(V3EdgeFuncP edgeFuncp, V3GraphVertex* vertexp);

    /// Debugging
    void dump(ostream& os=cout);
    void dumpDotFile(const string& filename, bool colorAsSubgraph);
    void dumpDotFilePrefixed(const string& nameComment, bool colorAsSubgraph=false);
    void userClearVertices();
    void userClearEdges();
    static void test();

    // CALLBACKS
    virtual void loopsMessageCb(V3GraphVertex* vertexp) { v3fatalSrc("Loops detected in graph: "<<vertexp); }
    virtual void loopsVertexCb(V3GraphVertex* vertexp);
};

//============================================================================

class V3GraphVertex : public AstNUser {
    // Vertices may be a 'gate'/wire statement OR a variable
protected:
    friend class V3Graph;    friend class V3GraphEdge;
    friend class GraphAcyc;  friend class GraphAlgRank;
    V3ListEnt<V3GraphVertex*>	m_vertices;// All vertices, linked list
    V3List<V3GraphEdge*> m_outs;	// Outbound edges,linked list
    V3List<V3GraphEdge*> m_ins;		// Inbound edges, linked list
    double		m_fanout;	// Order fanout
    uint32_t		m_color;	// Color of the node
    uint32_t		m_rank;		// Rank of edge
    union {
	void*		m_userp;	// Marker for some algorithms
	uint32_t	m_user;		// Marker for some algorithms
    };
    // METHODS
    void verticesPushBack(V3Graph* graphp);
    // ACCESSORS
    void fanout(double fanout) { m_fanout = fanout; }
    void rank(uint32_t rank) { m_rank = rank; }
    void inUnlink() { m_ins.reset(); }	 // Low level; normally unlinkDelete is what you want
    void outUnlink() { m_outs.reset(); } // Low level; normally unlinkDelete is what you want
public:
    // CONSTRUCTION
    V3GraphVertex(V3Graph* graphp);
    virtual ~V3GraphVertex() {}
    void	unlinkEdges(V3Graph* graphp);
    void	unlinkDelete(V3Graph* graphp);
    // ACCESSORS
    virtual string name() const { return ""; }
    virtual string dotColor() const { return "black"; }
    virtual string dotShape() const { return ""; }
    virtual string dotStyle() const { return ""; }
    virtual string dotName() const { return ""; }
    virtual int sortCmp(const V3GraphVertex* rhsp) const {
	// LHS goes first if of lower rank, or lower fanout
	if (m_rank < rhsp->m_rank) return -1;
	if (m_rank > rhsp->m_rank) return 1;
	if (m_fanout < rhsp->m_fanout) return -1;
	if (m_fanout > rhsp->m_fanout) return 1;
	return 0;
    }
    uint32_t	color() const { return m_color; }
    void	color(uint32_t color) { m_color = color; }
    uint32_t	rank() const { return m_rank; }
    double	fanout() const { return m_fanout; }
    void	user(uint32_t user) { m_user = user; }
    uint32_t	user() const { return m_user; }
    void	userp(void* userp) { m_userp = userp; }
    void*	userp() const { return m_userp; }
    // ITERATORS
    V3GraphVertex* verticesNextp() const { return m_vertices.nextp(); }
    V3GraphEdge* inBeginp() const { return m_ins.begin(); }
    bool 	 inEmpty() const { return inBeginp()==NULL; }
    bool 	 inSize1() const;
    uint32_t	 inHash() const;
    V3GraphEdge* outBeginp() const { return m_outs.begin(); }
    bool 	 outEmpty() const { return outBeginp()==NULL; }
    bool 	 outSize1() const;
    uint32_t	 outHash() const;
    // METHODS
    void	rerouteEdges(V3Graph* graphp);	///< Edges are routed around this vertex to point from "from" directly to "to"
};

ostream& operator<<(ostream& os, V3GraphVertex* vertexp);

//============================================================================

class V3GraphEdge {
    // Wires/variables aren't edges.  Edges have only a single to/from vertex
protected:
    friend class V3Graph;	friend class V3GraphVertex;
    friend class GraphAcyc;	friend class GraphAcycEdge;
    V3ListEnt<V3GraphEdge*> m_outs;	// Next Outbound edge for same vertex (linked list)
    V3ListEnt<V3GraphEdge*> m_ins;	// Next Inbound edge for same vertex (linked list)
    //
    V3GraphVertex*	m_fromp;	// Vertices pointing to this edge
    V3GraphVertex*	m_top;		// Vertices this edge points to
    int			m_weight;	// Weight of the connection
    bool		m_cutable;	// Interconnect may be broken in order sorting
    union {
	void*		m_userp;	// Marker for some algorithms
	uint32_t	m_user;		// Marker for some algorithms
    };
    // METHODS
    void cut() { m_weight = 0; }   // 0 weight is same as disconnected
    void outPushBack();
    void inPushBack();
public:
    // ENUMS
    enum Cuttable { NOT_CUTABLE = false, CUTABLE = true };	// For passing to V3GraphEdge
    // CONSTRUCTION
    // Add DAG from one node to the specified node
    V3GraphEdge(V3Graph* graphp, V3GraphVertex* fromp, V3GraphVertex* top, int weight, bool cutable=false);
    virtual ~V3GraphEdge() {}
    // METHODS
    virtual string name() const { return m_fromp->name()+"->"+m_top->name(); }
    virtual string dotLabel() const { return ""; }
    virtual string dotColor() const { return cutable()?"yellowGreen":"red"; }
    virtual string dotStyle() const { return cutable()?"dashed":""; }
    virtual int sortCmp(const V3GraphEdge* rhsp) const {
	if (!m_weight || !rhsp->m_weight) return 0;
	return top()->sortCmp(rhsp->top());
    }
    void	unlinkDelete();
    V3GraphEdge* relinkFromp(V3GraphVertex* newFromp);
    // ACCESSORS
    int		weight() const { return m_weight; }
    void	weight(int weight) { m_weight=weight; }
    bool	cutable() const { return m_cutable; }
    void	cutable(bool cutable) { m_cutable=cutable; }
    void 	userp(void* user) { m_userp = user; }
    void*	userp() const { return m_userp; }
    void 	user(uint32_t user) { m_user = user; }
    uint32_t	user() const { return m_user; }
    V3GraphVertex*	fromp() const { return m_fromp; }
    V3GraphVertex*	top() const { return m_top; }
    // STATIC ACCESSORS
    static bool	followNotCutable(const V3GraphEdge* edgep) { return !edgep->m_cutable; }
    static bool	followAlwaysTrue(const V3GraphEdge*) { return true; }
    // ITERATORS
    V3GraphEdge* outNextp() const { return m_outs.nextp(); }
    V3GraphEdge* inNextp() const { return m_ins.nextp(); }
};

//============================================================================

#endif // Guard
