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
 * GlobalDominance.h
 */

#ifndef GLOBALDOMINANCE_H
#define GLOBALDOMINANCE_H

#include <otawa/cfg/features.h>
#include <otawa/prog/WorkSpace.h>
#include <otawa/pcg/PCG.h>
#include <otawa/pcg/PCGBuilder.h>
#include "BlockDominance.h"
#include "EdgeDominance.h"
#include "../cfg_features.h"

// using namespace elm;
using otawa::Block;
extern bool cfg_follow_calls; // dirty hack that can be removed if we always enable CFG virtualization

class GlobalDominance
{
public:
	enum {
		BLOCK_DOM 	  = 1 << 0,
		EDGE_DOM 	  = 1 << 1,
		EDGE_POSTDOM  = 1 << 2,
	};

	GlobalDominance(const otawa::CFGCollection *cfgs, int flags) {
		for(otawa::CFGCollection::Iter cfg(cfgs); cfg; cfg++) {
			DBG("Computing dominance for CFG: " << cfg->name())

			if(flags&BLOCK_DOM) {
				bdoms.put(*cfg, new BlockDominance(*cfg));
				if(! (dbg_flags&DBG_DETERMINISTIC) )
					DBG("\tbdoms: " << **bdoms[*cfg])
			}

			if(flags&EDGE_DOM) {
				edoms.put(*cfg, new EdgeDom(*cfg, 
					singleton<otawa::sgraph::Edge*>(theOnly(cfg->entry()->outs()))
				));
				if(! (dbg_flags&DBG_DETERMINISTIC) )
					DBG("\tedoms: " << **edoms[*cfg])
			}

			if(flags&EDGE_POSTDOM) {
				elm::List<otawa::sgraph::Edge*> sl; 
				for(Block::EdgeIter i(cfg->exit()->ins()); i; i++)
					sl += *i;
				epdoms.put(*cfg, new EdgePostDom(*cfg, sl));
				if(! (dbg_flags&DBG_DETERMINISTIC) )
					DBG("\tepdoms: " << **epdoms[*cfg])
			}
		}
	}

	~GlobalDominance() {
		for(elm::HashTable<CFG*, BlockDominance*>::PairIterator i(bdoms); i; i++)
			delete (*i).snd;
		for(elm::HashTable<CFG*, EdgeDom*>::PairIterator i(edoms); i; i++)
			delete (*i).snd;
		for(elm::HashTable<CFG*, EdgePostDom*>::PairIterator i(epdoms); i; i++)
			delete (*i).snd;
	}

	bool dom(Block* b1, Block* b2) const {
		while(b2->cfg() != b1->cfg()) {
			if(!cfg_follow_calls)
				return false;
			b2 = getCaller(b2, NULL);
			if(!b2) // reached main, still didn't match b1's cfg, so b2 isn't even indirectly called by b1
				return false;
		}
		while(b2 != b1) {
			Block* prev_b2 = b2;
			b2 = static_cast<Block*>((*bdoms.get(b2->cfg()))->idom(b2));
			if(b2 == prev_b2) // reached entry
				return false;
		}
		return true;
	}
	bool dom(otawa::Edge* e1, otawa::Edge* e2) const {
		// a nice thing with dominance is that we don't need to check that the entry dominates the edge, it's always the case
		while(e2->source()->cfg() != e1->source()->cfg()) {
			if(!cfg_follow_calls)
				return false;
			Block* b = getCaller(e2->source()->cfg(), NULL);
			if(!b) // reached main, still didn't match e1's cfg, so e2 isn't even indirectly called by e1
				return false;
			e2 = theOnly(b->ins());
		}
		while(e2 != e1) {
			otawa::Edge* prev_e2 = e2;
			e2 = static_cast<otawa::Edge*>((*edoms.get(e2->source()->cfg()))->idom(e2));
			if(e2 == prev_e2) // reached entry
				return false;
		}
		return true;
	}
	bool postdom(otawa::Edge* e1, otawa::Edge* e2) const {
		// now we're going to have to check that the entry is post-dominated by the edge, everytime we go up in the caller tree
		while(e1->source()->cfg() != e2->source()->cfg()) {
			if(!cfg_follow_calls)
				return false;
			if(! postdom(e1, theOnly(e1->source()->cfg()->entry()->outs())) )
				return false; // e1 must post-dominate the entry edge of its cfg!
			Block* b = getCaller(e1->source()->cfg(), NULL);
			if(!b) // reached main, still didn't match e2's cfg, so e1 isn't even indirectly called by e2
				return false;
			e1 = theOnly(b->ins());
		}
		while(e2 != e1) {
			otawa::Edge* prev_e2 = e2;
			e2 = static_cast<otawa::Edge*>((*epdoms.get(e2->source()->cfg()))->idom(e2));
			if(!e2 || e2 == prev_e2) // reached (virtual) exit
				return false;
		}
		return true;
	}

private:
	elm::HashTable<CFG*, BlockDominance*> bdoms;
	elm::HashTable<CFG*, EdgeDom*> edoms;
	elm::HashTable<CFG*, EdgePostDom*> epdoms;
	// BlockDominanceGen<PCGBlock, PCGEdge> pcg_dom;
};

#endif