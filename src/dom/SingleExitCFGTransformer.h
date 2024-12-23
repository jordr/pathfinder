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
	SingleExitCFGTransformer.h
*/

#ifndef SINGLEEXITCFGTRANSFORMER_H
#define SINGLEEXITCFGTRANSFORMER_H

#include <otawa/cfg/CFGTransformer.h>

class MyTransformer : public otawa::CFGTransformer {

Block* clone(Block *b) {
		// end case
		if(b->isEnd()) {
			if(b->isEntry())
				return cur->entry();
			else if(b->isExit()) {
				return cur->exit();
				Block::EdgeIter exits(cur->exit()->ins());
				otawa::Edge* first_exit_edge = *exits;
				ASSERT(first_exit_edge);
				if(++exits)
				{
					// need to create an empty basic block to make sure we only have one exit edge
					genstruct::Table<otawa::Inst *> insts;
					otawa::BasicBlock* bb = build(insts);
					build(bb, cur->exit(), otawa::Edge::NOT_TAKEN); // add bb out edge
					build(first_exit_edge->source(), bb, otawa::Edge::NOT_TAKEN); // add bb in edge
					for(; exits; exits++)
						build((*exits)->source(), bb, otawa::Edge::NOT_TAKEN); // add bb in edge
				}
				return cur->exit();
			}
			else if(b->isUnknown())
				return cur->unknown();
			else {
				ASSERT(false);
				return 0;
			}
		}

		// synthetic block case
		else if(b->isSynth())
			return build(b->toSynth()->callee());

		// basic block
		else {
			otawa::BasicBlock *bb = b->toBasic();
			Vector<otawa::Inst *> insts(bb->count());
			for(otawa::BasicBlock::InstIter i = bb; i; i++)
				insts.add(*i);
			return build(insts.detach());
		}
	}
};

#endif