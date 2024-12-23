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
 
/**
 General functions to browse a virtualized CFG (inline, unique-Block calls)
*/

#ifndef _CFG_FEATURES_H
#define _CFG_FEATURES_H
#include <elm/PreIterator.h>
#include <elm/data/List.h>
#include <elm/option/Option.h>
#include <otawa/cfg/CFG.h>
#include <otawa/cfg/features.h>

using otawa::Block;
using otawa::CFG;
using otawa::LOOP_HEADER;
using otawa::ENCLOSING_LOOP_HEADER;
using elm::PreIterator;
using elm::Option;
using namespace elm;


extern bool cfg_follow_calls; // TODO!! dirty

template <class I, class T> inline T theOnly(PreIterator<I, T>& i) {
	ASSERT(i);
	T rtn = *i;
	i++;
	ASSERTP(!i, "not alone, next is " << *i);
	return rtn;
}
template <class I, class T> inline T theOnly(const PreIterator<I, T>& i) 
	{ I j(static_cast<const I&>(i)); return theOnly(j); }
	// { ASSERT(i); T rtn = *i; I j(static_cast<const I&>(i)); ASSERT(!++j); return rtn; }
template <class T> inline elm::List<T> singleton(const T& x) { elm::List<T> l; l.add(x); return l; }

Option<Block*> getCaller(CFG* cfg);
Block* getCaller(CFG* cfg, Block* def);
inline Block* getCaller(Block* b, Block* def) { return getCaller(b->cfg(), def); }

// Loop Header Iter for a virtualized CFG
class LoopHeaderIter: public PreIterator<LoopHeaderIter, Block*> {
public:
	inline LoopHeaderIter(Block* b, bool inlined = true): lh(b)
		{ if(!LOOP_HEADER(b)) next(); }
	inline bool ended(void) const
		{ return lh == NULL; }
	Block* item(void) const
		{ return lh; } // works even when ended()
	inline void next(void) {
		if(cfg_follow_calls) {
			while(lh && !(tmp = ENCLOSING_LOOP_HEADER.get(lh, NULL)))
				lh = getCaller(lh->cfg(), NULL);
			lh = lh ? tmp : NULL;
		}
		else
			lh = ENCLOSING_LOOP_HEADER.get(lh, NULL);
	}

private:
	Block* lh; // can be NULL
	Block* tmp;
};

#endif
