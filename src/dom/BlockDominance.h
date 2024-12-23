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
 * BlockDominance.h
 */

#ifndef BLOCKDOMINANCE_H
#define BLOCKDOMINANCE_H

#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ograph/GenGraph.h>
#include <otawa/graph/DiGraph.h>
#include <otawa/prog/WorkSpace.h>
#include <elm/data/HashTable.h>

// using namespace elm;
using otawa::sgraph::DiGraph;
using otawa::sgraph::Vertex;
using otawa::graph::GenGraph;

io::Output& operator<<(io::Output& out, Vertex* b); // TODO

class BlockDominance {
public:
	class RPO {
	public:
		RPO(DiGraph* g) { process(g); }
		const elm::List<Vertex*>& toList() const { return O; }
		bool ordered(Vertex* a, Vertex* b) const { // test a <= b
			for(Iter i(O); i; i++) {
				if(*i == a) return true; // a <= b
				if(*i == b) return false; // a > b
			}
			ASSERTP(false, "rpo.ordered found neither " << a << " nor " << b);
		}
		inline bool strictOrdered(Vertex* a, Vertex* b) const { return !ordered(a,b); } // a < b = !(b <= a)

		typedef elm::List<Vertex*>::Iter Iter;

	private:
		void process(DiGraph *g);
		elm::List<Vertex*> O;
	};

	BlockDominance(DiGraph *g) : rpo(g) {
		process(g);
	}
	Vertex* idom(Vertex* v) {
		ASSERT(doms.exists(v));
		return doms.get(v);
	}

private:
	void process(DiGraph *g) {
		DominanceProblem(g);
	}
	// dominance (new)
	void DominanceProblem(DiGraph *g);
	Vertex* intersect(Vertex* b1, Vertex* b2);
	io::Output& print(io::Output& out) const; // print doms
	inline friend io::Output& operator<<(io::Output& out, const BlockDominance& ed) { return ed.print(out); }
	
	RPO rpo;
	elm::HashTable<Vertex*, Vertex*> doms;
};

template<class Vertex, class Edge>
class RPOGen {
public:
	RPOGen(GenGraph<Vertex, Edge>* g, Vertex* entry) { process(g, entry); }
	const elm::List<Vertex*>& toList() const { return O; }
	bool ordered(Vertex* a, Vertex* b) const { // test a <= b
		for(Iter i(O); i; i++) {
			if(*i == a) return true; // a <= b
			if(*i == b) return false; // a > b
		}
		ASSERTP(false, "rpo.ordered found neither " << a << " nor " << b);
	}
	inline bool strictOrdered(Vertex* a, Vertex* b) const { return !ordered(a,b); } // a < b = !(b <= a)

	typedef typename elm::List<Vertex*>::Iter Iter;

private:
	void process(GenGraph<Vertex, Edge> *g, Vertex* entry) {
		/* O ← [] */ // order
		O.clear();
		/* S ← [ε] */ // stack
		elm::List<Vertex*> S;
		S.add(entry);
		/* D ← { } */ // done
		Vector<Vertex*> D;
		/* while S ≠ [] do */
		while(S) {
			/* v ← hd S */
			Vertex* v = S.first();
			/* D ← D ∪ { v } */
			if(! D.contains(v))
				D.push(v);
			/* if ∃ (v, w) ∈ E, w ∉ D then */
			bool all_done = true;
			for(typename GenGraph<Vertex, Edge>::OutIterator e(v); e; e++) {
				Vertex* ee = g->sinkOf(e);
				if(! D.contains(ee)) {
					all_done = false;
					/* S ← w :: S */
					S.addFirst(g->sinkOf(e));
					break;
				}
			}
			/* else */
			if(all_done) {
				/* O ← v :: O */
				O.addFirst(v);
				/* S ← tl S */
				S.removeFirst();
			}
		}
	}
	elm::List<Vertex*> O;
};

template<class Vertex, class Edge>
class BlockDominanceGen {
public:
	BlockDominanceGen(GenGraph<Vertex, Edge> *g, Vertex* entry) : rpo(g, entry) {
		process(g, entry);
	}
	Vertex* idom(Vertex* v) {
		ASSERT(doms.exists(v));
		return doms.get(v);
	}

private:
	void process(GenGraph<Vertex, Edge> *g, Vertex* entry) {
		DominanceProblem(g, entry);
	}

	void DominanceProblem(GenGraph<Vertex, Edge> *g, Vertex* entry) {
		/* for all nodes, b // initialize the dominators array */
			/* doms[b] ← Undefined */
		doms.clear();
		/* doms[start node] ← start node */
		doms.put(entry,entry);
		/* Changed ← true */
		bool changed = true;
		/* while (Changed) */
		while(changed) {
			/* Changed ← false */
			changed = false;
			/* for all nodes, b, in reverse postorder (except start node) */
			for(typename RPOGen<Vertex, Edge>::Iter b(rpo.toList()); b; b++) {
				if(b == entry)
					continue;
				/* new idom ← first (processed) predecessor of b // (pick one) */
				Vertex* new_idom = NULL;
				/* for all other predecessors, p, of b */
				for(typename GenGraph<Vertex, Edge>::InIterator pe(b); pe; pe++) {
					/* if doms[p] != Undefined // i.e., if doms[p] already calculated */
					if(doms.exists(pe->source()))
						/* new idom ← intersect(p, new idom) */
						new_idom = (new_idom==NULL) ? pe->source() : intersect(pe->source(), new_idom);
				}
				ASSERT(new_idom);
				/* if doms[b] != new idom */
				if(doms.get(b, NULL) != new_idom) {
					/* doms[b] ← new idom */
					doms.put(b, new_idom);
					/* Changed ← true */
					changed = true;
				}
			}
		}
	}

	Vertex* intersect(Vertex* b1, Vertex* b2) {
		/* finger1 ← b1 */
		/* finger2 ← b2 */
		Vertex *finger1 = b1, *finger2 = b2;
		/* while (finger1 != finger2) */
		while(finger1 != finger2) {
			/* while (finger1 < finger2) */
			while(rpo.strictOrdered(finger1, finger2))
				/* finger1 = doms[finger1] */
				finger1 = doms[finger1];
			/* while (finger2 < finger1) */
			while(rpo.strictOrdered(finger2, finger1))
				/* finger2 = doms[finger2] */
				finger2 = doms[finger2];
		}
		/* return finger1 */
		return finger1;
	}

	io::Output& print(io::Output& out) const {
		bool first = true;
		for(typename elm::HashTable<Vertex*, Vertex*>::PairIterator i(doms); i; i++) {
			out << (first?"":",  ") << (*i).fst << ":" << (*i).snd;
			first = false;
		}
		return out;
	}
	inline friend io::Output& operator<<(io::Output& out, const BlockDominanceGen& ed) { return ed.print(out); }
	
	RPOGen<Vertex, Edge> rpo;
	elm::HashTable<Vertex*, Vertex*> doms;
};

#endif /* BLOCKDOMINANCE_H */
