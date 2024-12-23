/*
 *	
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2006-2018, IRIT UPS.
 * 
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software 
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
/*
 * EdgeDominance.h
 */

#ifndef EDGEDOMINANCE_H
#define EDGEDOMINANCE_H

#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ograph/GenGraph.h>
#include <otawa/graph/DiGraph.h>
#include <otawa/prog/WorkSpace.h>
// #include <elm/data/List.h>
#include <elm/data/HashTable.h>
#include "../debug.h"

#define VIRTUAL_ENTRY_EDGE NULL

// using namespace elm;
using namespace elm::color;
using otawa::graph::DiGraph;
using otawa::graph::Vertex;

io::Output& operator<<(io::Output& out, otawa::graph::Edge* e); // TODO

class Order {
public:
	typedef otawa::graph::Edge edge_t;
	// to implement
	virtual Vertex::EdgeIter pred(edge_t* e) const = 0;
	virtual Vertex::EdgeIter succ(edge_t* e) const = 0;

	// Order(DiGraph* g, edge_t* start) { /*process(g, start);*/ }
	const elm::List<edge_t*>& toList() const { return O; }
	bool ordered(edge_t* a, edge_t* b) const { // test a <= b
		if(!a) return true; // a=NULL: a <= b
		if(!b) return false; // b=NULL: a < b
		for(Iter i(O); i; i++) {
			if(*i == b) return true; // a <= b
			if(*i == a) return false; // a > b
		}
		ASSERTP(false, "Order::ordered found neither " << a << " nor " << b);
	}
	inline bool strictOrdered(edge_t* a, edge_t* b) const { return !ordered(b,a); } // a < b = !(b <= a)

	typedef elm::List<edge_t*>::Iter Iter;
protected:
	// void process(DiGraph *g, edge_t* start);
	void process(DiGraph *g, elm::List<edge_t*> S);
private:
	elm::List<edge_t*> O;
};

template <class O> class EdgeDominance;

// Reverse Post Order (for dominance)
class RPO : public Order {
public:
	RPO(DiGraph* g, const elm::List<edge_t*>& start) { process(g, start); }
	inline virtual Vertex::EdgeIter pred(edge_t* e) const { return e->source()->ins(); }
	inline virtual Vertex::EdgeIter succ(edge_t* e) const { return e->sink()->outs(); }
};
typedef EdgeDominance<RPO> EdgeDom;

// Pre(Post?) Order (for post dominance)
class PO : public Order {
public:
	PO(DiGraph* g, const elm::List<edge_t*>& start) { process(g, start); }
	inline virtual Vertex::EdgeIter pred(edge_t* e) const { return e->sink()->outs(); }
	inline virtual Vertex::EdgeIter succ(edge_t* e) const { return e->source()->ins(); }
};
typedef EdgeDominance<PO> EdgePostDom;

template <class O> // chosen order
class EdgeDominance {
public:
	typedef otawa::graph::Edge edge_t;

	EdgeDominance(DiGraph *g, elm::List<edge_t*> start) : o(g, start) {
		DominanceProblem(g, start);
	}
	edge_t* idom(edge_t* e) {
		ASSERT(doms.exists(e));
		return doms.get(e);
	}

private:
	void DominanceProblem(DiGraph *g, elm::List<edge_t*> start) {
		bool multiple_start = start.count() > 1;
		/* for all nodes, e // initialize the dominators array */
			/* doms[e] ← Undefined */
		doms.clear();
		/* doms[start edge] ← start edge */
		// doms.put(start, start);
		if(multiple_start) doms.put(VIRTUAL_ENTRY_EDGE, VIRTUAL_ENTRY_EDGE); // hack
		for(elm::List<edge_t*>::Iter i(start); i; i++)
			if(multiple_start) doms.put(*i, VIRTUAL_ENTRY_EDGE); // hack	
			else
				doms.put(*i, *i);
		/* Changed ← true */
		bool changed = true;
		/* while (Changed) */
		while(changed) {
			/* Changed ← false */
			changed = false;
			/* for all nodes, e, in reverse postorder (except start node) */
			for(typename O::Iter e(o.toList()); e; e++) {
				// if(e == start)
				if(start.contains(e))
					continue;
				/* new idom ← first (processed) predecessor of e // (pick one) */
				Option<edge_t*> new_idom = none;
				/* for all other predecessors, p, of e */
				for(Vertex::EdgeIter p(o.pred(e)); p; p++) {
					/* if doms[p] != Undefined // i.e., if doms[p] already calculated */
					if(doms.exists(p))
						/* new idom ← intersect(p, new idom) */
						new_idom = new_idom ? intersect(p, new_idom) : p;
				}
				ASSERT(new_idom.isOne());
				/* if doms[e] != new idom */
				if(!doms.exists(e) || doms.get(e) != *new_idom) {
					/* doms[e] ← new idom */
					doms.put(e, new_idom);
					/* Changed ← true */
					changed = true;
				}
			}
		}
	}

	edge_t* intersect(edge_t* b1, edge_t* b2) {
		// cout << "intersect(" << b1 << ", " << b2 << ")\n";
		/* finger1 ← b1 */
		/* finger2 ← b2 */
		edge_t *finger1 = b1, *finger2 = b2;
		/* while (finger1 != finger2) */
		while(finger1 != finger2) {
			/* while (finger1 < finger2) */
			while(o.strictOrdered(finger1, finger2)) {
				/* finger1 = doms[finger1] */
				finger1 = doms[finger1];
				if(!finger1) return VIRTUAL_ENTRY_EDGE;
				// DBG("finger1= " << finger1)
			}
			/* while (finger2 < finger1) */
			while(o.strictOrdered(finger2, finger1)) {
				/* finger2 = doms[finger2] */
				finger2 = doms[finger2];
				if(!finger2) return VIRTUAL_ENTRY_EDGE;
				// DBG("finger2= " << finger2)
			}
		}
		/* return finger1 */
		return finger1;
	}

	io::Output& print(io::Output& out) const {
		bool first = true;
		for(elm::HashTable<edge_t*, edge_t*>::PairIterator i(doms); i; i++) {
			out << (first?"":",  ") << (*i).fst << ":" << (*i).snd;
			first = false;
		}
		return out;
	}
	inline friend io::Output& operator<<(io::Output& out, const EdgeDominance& ed) { return ed.print(out); }
	
	O o;
	elm::HashTable<edge_t*, edge_t*> doms;
};

#endif /* EDGEDOMINANCE_H */
