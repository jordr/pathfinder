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
 
#include <elm/io/Output.h>
#include <otawa/cfg/CFG.h>
#include "pretty_printing.h"

/**
 * Print n indents to the given output
 */
void addIndents(elm::io::Output& out, int n)
{
	for(int i=0; i<n; i++)
		out << "\t";
}

#ifndef NO_UTF8
	#define UTF8_Block_end_delim "⟩"
	#define UTF8_Block_start_delim "⟨"
#else
	#define UTF8_Block_start_delim "<"
	#define UTF8_Block_end_delim ">"
#endif

/**
 * @fn      Output& otawa::operator<<(Output& out, otawa::Block* b);
 * @brief      Prints a block
 *
 * @param      out   The out
 * @param      b     The block
 *
 * @return     the out
 */
Output& otawa::operator<<(Output& out, otawa::Block* b)
{
	if(b->isBasic())
		return (out << UTF8_Block_start_delim << b->cfg() << ":" << b->index() << UTF8_Block_end_delim);
	if(b->isEntry())
		return (out << UTF8_Block_start_delim "^" << b->cfg() << UTF8_Block_end_delim);
	if(b->isExit())
		return (out << UTF8_Block_start_delim << b->cfg() << ":" << b->index() << "$" UTF8_Block_end_delim);
	if(b->isUnknown())
		return (out << UTF8_Block_start_delim "?" << b->cfg() << ":" << b->index() << "?" UTF8_Block_end_delim);
	if(b->isVirtual())
		return (out << UTF8_Block_start_delim "~" << b->cfg() << ":" << b->index() << UTF8_Block_end_delim);
	if(b->isCall())
#		ifndef NO_UTF8
			return (out << UTF8_Block_start_delim "@" << b->cfg() << ":" << b->index() << "→" << b->toSynth()->callee() << UTF8_Block_end_delim);
#		else
			return (out << UTF8_Block_start_delim "@" << b->cfg() << ":" << b->index() << "->" << b->toSynth()->callee() << UTF8_Block_end_delim);
#		endif
	ASSERT(false);
}

elm::String pathToString(const Set<otawa::Edge*>& path)
{
	if(dbg_flags&DBG_DETERMINISTIC)
		return _ << path.count() << " labels";
	else
	{
		elm::String str;
		bool first = true;
		for(Set<otawa::Edge*>::Iter iter(path); !iter.ended(); iter++)
		{
			if(first)
				first = false;
			else
				str = str.concat(_ << ", ");
			str = str.concat(_ << (*iter)->source()->cfg() << ":" << (*iter)->source()->index() << "->" << (*iter)->target()->cfg() << ":" << (*iter)->target()->index());
		}
		return str;
	}
}
