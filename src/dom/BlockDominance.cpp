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
 * BlockDominance.cpp
 */

#include "BlockDominance.h"

io::Output& operator<<(io::Output& out, Vertex* b) {
	return (out << b->index());
}

void BlockDominance::RPO::process(DiGraph *g)
{
	/* O ← [] */ // order
	O.clear();
	/* S ← [ε] */ // stack
	elm::List<Vertex*> S;
	S.add(g->entry());
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
		for(Vertex::EdgeIter e(v->outs()); e; e++)
			if(! D.contains(e->sink())) {
				all_done = false;
				/* S ← w :: S */
				S.addFirst(e->sink());
				break;
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

void BlockDominance::DominanceProblem(DiGraph *g) {
	/* for all nodes, b // initialize the dominators array */
		/* doms[b] ← Undefined */
	doms.clear();
	/* doms[start node] ← start node */
	doms.put(g->entry(),g->entry());
	/* Changed ← true */
	bool changed = true;
	/* while (Changed) */
	while(changed) {
		/* Changed ← false */
		changed = false;
		/* for all nodes, b, in reverse postorder (except start node) */
		for(RPO::Iter b(rpo.toList()); b; b++) {
			if(b == g->entry())
				continue;
			/* new idom ← first (processed) predecessor of b // (pick one) */
			Vertex* new_idom = NULL;
			/* for all other predecessors, p, of b */
			for(Vertex::EdgeIter pe(b->ins()); pe; pe++) {
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

Vertex* BlockDominance::intersect(Vertex* b1, Vertex* b2) {
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

io::Output& BlockDominance::print(io::Output& out) const {
	bool first = true;
	for(elm::HashTable<Vertex*, Vertex*>::PairIterator i(doms); i; i++) {
		out << (first?"":",  ") << (*i).fst << ":" << (*i).snd;
		first = false;
	}
	return out;
}
