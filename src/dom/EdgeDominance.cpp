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
 * EdgeDominance.cpp
 */

#include <elm/data/List.h>
#include "EdgeDominance.h"
#include "../cfg_features.h"

// using namespace elm;
using otawa::sgraph::Edge;

io::Output& operator<<(io::Output& out, otawa::sgraph::Edge* e) {
	return !e ? (out << "null") : (out << e->source()->index() << "->" << e->sink()->index());
}

// void Order::process(DiGraph *g, Edge* start)
void Order::process(DiGraph *g, elm::List<Edge*> S)
{
	/* O ← [] */ // order
	O.clear();
	/* S ← [e_ε] */ // stack
	// elm::List<Edge*> S;
	// S.add(start);
	/* D ← { } */ // done
	Vector<Edge*> D;
	/* while S ≠ [] do */
	while(S) {
		/* v ← hd S */
		Edge* e = S.first();
		/* D ← D ∪ { e } */
		if(! D.contains(e))
			D.push(e);
		/* if ∃ (e, f) ∈ V, f ∉ D then, for each, */
		bool all_done = true;
		for(Vertex::EdgeIter f(succ(e)); f; f++)
			if(! D.contains(*f)) {
				all_done = false;
				/* S ← f :: S */
				S.addFirst(*f);
				break;
			}
		/* else */
		if(all_done) {
			/* O ← e :: O */
			O.addFirst(e);
			/* S ← tl S */
			S.removeFirst();
		}
	}
}