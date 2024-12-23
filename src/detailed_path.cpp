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
 
#include "detailed_path.h"
#include "debug.h"
#include "otawa/cfg/features.h"

extern bool cfg_follow_calls;

/**
 * @class DetailedPath
 * @author Jordy Ruiz 
 * @brief A path enriched with flowfact information such as loops, calls, etc.
 */
/**
 * @class DetailedPath::Iter
 * @author Jordy Ruiz 
 * @brief Iter on all the compounds of the DetailedPath
 */
/**
 * @class DetailedPath::FlowInfo
 * @author Jordy Ruiz 
 * @brief Flowfact information around the path, such as loops, calls, etc.
 */
/**
 * @class DetailedPath::EdgeIterator
 * @author Jordy Ruiz 
 * @brief Iter only on the actual edges of the DetailedPath
 */

DetailedPath::DetailedPath(CFG* f) : fun(f) { }
DetailedPath::DetailedPath(BasicBlock* bb) : fun(NULL) { fromContext(bb); }
// DetailedPath::DetailedPath(const elm::List<Edge*>& edge_list) : fun(NULL)
// {
// 	for(elm::List<Edge*>::Iter iter(edge_list); iter; iter++)
// 		this->addLast(*iter);
// }
DetailedPath::DetailedPath(const DetailedPath& dp) : _path(dp._path), fun(dp.fun) { }

/**
 * @fn inline void DetailedPath::clear(void);
 * @brief Reset the detailed path
 */

/**
 * @fn void DetailedPath::addLast(Edge* e);
 * @brief add Edge at the end of the DetailedPath
 * @param e Edge to add
 */
void DetailedPath::addLast(Edge* e)
	{ _path.addLast(FlowInfo(FlowInfo::KIND_EDGE, e)); }

void DetailedPath::addBefore(const Iter &pos, const FlowInfo &value)
	{ _path.addBefore(pos, value); }

/**
 * @fn void DetailedPath::removeLast(void);
 * Remove the last item from the list.
 * @warning It is an error to call this method if the list is empty.
 */

/**
 * @fn void DetailedPath::optimize();
 * Remove redundant information such as loop entries followed by loop exits with nothing in between
 */
void DetailedPath::optimize()
{
	// removeDuplicates(); // useless (why?)
	removeAntagonists();
	removeCallsAtEndOfPath();
}

void DetailedPath::removeDuplicates()
{
	Iter iter(*this), prev(*this);
	if(!iter) // make sure the detailed path is not empty
		return;
	for(iter++; !iter.ended(); prev++, iter++)
	{
		if(    ((iter->isLoopEntry() && prev->isLoopEntry()) || (iter->isLoopExit() && prev->isLoopExit()))
			&& (iter->getLoopHeader() == prev->getLoopHeader()))
		{
			remove(iter);
			return removeDuplicates(); // parse again
		}
	}
}

void DetailedPath::removeAntagonists()
{
	Iter iter(*this), prev(*this);
	if(!iter) // make sure the detailed path is not empty
		return;
	for(iter++; !iter.ended(); prev++, iter++)
	{
		if( (iter->isLoopExit() && prev->isLoopEntry() && iter->getLoopHeader() == prev->getLoopHeader())
		 || (iter->isReturn()	&& prev->isCall()	   && iter->getCaller()		== prev->getCaller()	))
		{
			// Be careful, do not call prev before iter, because remove(prev) does NOT update info in iter.
			remove(iter);
			remove(prev);
			return removeAntagonists(); // parse again
		}
	}
}

void DetailedPath::removeCallsAtEndOfPath()
{
	Iter last_edge_iter(*this);
	for(Iter iter(*this); !iter.ended(); iter++)
		if(iter->isEdge())
			last_edge_iter = iter;
	// last_edge_iter is now at the position of the last edge in the path
	if(!last_edge_iter)
		return; // nothing remains, exit
	for(last_edge_iter++; !last_edge_iter.ended(); )
		if(last_edge_iter->isCall())
			remove(last_edge_iter);
		else last_edge_iter++;
}

void DetailedPath::fromContext(Block* b)
{
	do
	{
		if(b->isCall()) // always executed except the first time
			this->_path.addFirst(FlowInfo(FlowInfo::KIND_CALL, b->toSynth()));
		if(otawa::LOOP_HEADER(b))
			this->_path.addFirst(FlowInfo(FlowInfo::KIND_LOOP_ENTRY, b->toBasic()));
		while(otawa::ENCLOSING_LOOP_HEADER(b))
		{
			b = otawa::ENCLOSING_LOOP_HEADER(b);
			this->_path.addFirst(FlowInfo(FlowInfo::KIND_LOOP_ENTRY, b->toBasic()));
		}
		fun = b->cfg();
	} while(cfg_follow_calls && (b = getCaller(b->cfg(), NULL)) != NULL);
}

/*
void DetailedPath::onLoopEntry(Block* loop_header)
{
	ASSERT(loop_header->isBasic());
	FlowInfo fi(FlowInfo::KIND_LOOP_ENTRY, loop_header->toBasic());
	_path.addLast(fi);
}*/

void DetailedPath::onLoopExit(Option<Block*> new_loop_header) // TODO: shouldn't this be old_loop_header?
{
	ASSERT(!new_loop_header || (*new_loop_header)->isBasic());
	if(new_loop_header)
		_path.addLast(FlowInfo(FlowInfo::KIND_LOOP_EXIT, (*new_loop_header)->toBasic())); // TODO! change this
	else
		_path.addLast(FlowInfo(FlowInfo::KIND_LOOP_EXIT, (BasicBlock*)NULL));
}

/**
 * @param sb Call block
 */
void DetailedPath::onCall(SynthBlock* sb)
{
	ASSERT(sb->isCall());
	_path.addLast(FlowInfo(FlowInfo::KIND_CALL, sb));
}

/**
 * @param sb Call block
 */
void DetailedPath::onReturn(SynthBlock* sb)
{
	ASSERT(sb->isCall());
	_path.addLast(FlowInfo(FlowInfo::KIND_RETURN, sb));
	/*
	BasicBlock::InIterator ins(bb);
	while(otawa::RECURSIVE_LOOP(ins))
	{
		ins++;
		ASSERTP(ins, "Only recursive loop calls have been found pointing to BasicBlock bb.")  // useless check in case of a well-formed CFG
	}
	_path.addLast(FlowInfo(FlowInfo::KIND_RETURN, ins->source()));
	while(ins++)
		ASSERTP(otawa::RECURSIVE_LOOP(ins), "First BB of function has several incoming non-recursive edges (e.g. it is called several times)");
	*/
}

/**
 * @fn bool DetailedPath::weakEqualsTo(const DetailedPath& dp) const;
 * @brief weak equality: test if the edge paths are the same - do not consider other flow info
 * 
 * @param dp DetailedPath to compare to
 */
bool DetailedPath::weakEqualsTo(const DetailedPath& dp) const
{
	EdgeIterator dp_iter(dp), this_iter(*this);
	while(!dp_iter.ended() && !this_iter.ended())
	{
		if(*this_iter != *dp_iter) // compare edge*
			return false; // not equal
		dp_iter++, this_iter++;
	}
	if(dp_iter || this_iter) // if we are not done parsing one of the two paths
		return false; // then not equal
	return true; // all checks passed, equal
}

/**
 * @fn void DetailedPath::merge(const Vector<DetailedPath>& paths);
 * Modifies the current path to be the result of the merge of a Vector of path (basically we're going to preserve info such as loop and call info)
 * Previous info in current DetailedPath is ignored
 */
void DetailedPath::merge(const Vector<DetailedPath>& paths)
{
	ASSERTP(paths, "merge called with empty paths vector")
	_path.clear(); // do not take in account current path
	/* explanation of the algorithm:
		Path#0: C#8, C#24, 
		Path#1: C#8, C#24, 
		Path#2: C#8
		Path#4: C#8, C#24, C#25,

		First, take this->_path := Path#0 (C#8, C#24)
		Then for each Path#1-4 "p", check that p contains this->_path.
		For example, when p=Path#2, p (C#8) does not contain this->_path (C#8, C#24) does not contain p, so we remove C#24 from this->_path
	*/ 
	bool first = true;
	for(Vector<DetailedPath>::Iter paths_iter(paths); !paths_iter.ended(); paths_iter++)
	{
		const DetailedPath &p = *paths_iter;
		if(first) // Path#0: initialize this->_path
		{
			first = false;
			// this->_path initialized with all calls contained in paths[0]
			for(DetailedPath::Iter p_flowinfo_iter(p); p_flowinfo_iter; p_flowinfo_iter++)
			{
				if(p_flowinfo_iter->isCall() || p_flowinfo_iter->isReturn())
					this->_path.addLast(*p_flowinfo_iter); // use addLast instead of addFirst to preserve order of calls
			}
			removeAntagonists();
		}
		else
		{
			for(DetailedPath::Iter this_iter(*this); this_iter; ) // for each element in this->_path
			{
				if(!p.contains(this_iter)) // if it is not in p
				{
					#ifdef DBGG // this is due to flowinfo...
						cout << color::IYel() << "!p.contains(this_iter);" << color::RCol() << io::endl;
						cout << "\t* p=" << p << io::endl;
						cout << "\t* this=" << *this << io::endl;
						cout << "\t* this_iter=" << *this_iter << io::endl;
						cout << "this->remove(*this_iter);" << io::endl;
					#endif
					this->_path.remove(this_iter); // remove it from this->_path
				}
				else this_iter++;
			}
		}
	}
		// DBGG(color::IYel() << "result: " << toString())
/*
	int merge_count = 0;
	for(Vector<DetailedPath>::Iter iter(paths); iter; iter++)
		cout << color::IRed() << "\tmerge[" << merge_count++ << "]=" << *iter << io::endl;
	cout << color::RCol();
	Edge* first_call = NULL;
	for(Iter this_iter(paths.first()); this_iter; this_iter++)
	{
		if(this_iter->isCall())
		{
			first_call = this_iter->getEdge();
			break;
		}
	}
	if(first_call == NULL)
		return; // nothing to do
	else cout << color::ICya() << "is call:" << first_call << color::RCol() << io::endl;
	int count = 0;
	for(Vector<DetailedPath>::Iter paths_iter(paths); paths_iter; paths_iter++)
	{
		cout << "DP#" << count++ << ": ";
		for(DetailedPath::Iter flowinfo_iter(*paths_iter); flowinfo_iter; flowinfo_iter++)
		{
			if(flowinfo_iter->isCall())
				cout << flowinfo_iter->toString() << ", ";
		}
		cout << io::endl;
	}
*/
}

void DetailedPath::apply(const DetailedPath& path)
{
	for(elm::List<FlowInfo>::Iter i(path._path); !i.ended(); i++)
		this->_path.addLast(*i);
}

bool DetailedPath::hasAnEdge() const
{
	return ! EdgeIterator(*this).ended();
}

Edge* DetailedPath::firstEdge() const
{
	return EdgeIterator(*this).item();
}

Edge* DetailedPath::lastEdge() const
{
	Edge* rtn = NULL;
	for(EdgeIterator iter(*this); iter; iter++)
		rtn = *iter;
	ASSERTP(rtn != NULL, "lastEdge() called on DetailedPath empty of edges: " << *this);
	return rtn;
}

/**
 * @brief      Find the (virtually) last block. May use loop info. This is handy to find an id to attach to memory.
 *
 * @return     The Block found
 */
Option<Block*> DetailedPath::lastBlock() const
{
	Option<Block*> rtn = none;
	for(elm::List<FlowInfo>::Iter iter(_path); !iter.ended(); iter++)
	{
		if(iter->isEdgeKind())
			rtn = some(iter->getEdge()->target());
		if(iter->isBasicBlockKind())
			rtn = some(iter->getBasicBlock());
		if(iter->isSynthBlockKind())
			rtn = some(iter->getSynthBlock());
	}
	return rtn;
}

int DetailedPath::countEdges() const
{
	int count = 0;
	for(elm::List<FlowInfo>::Iter iter(_path); !iter.ended(); iter++)
		if((*iter).isEdge())
			count++;
	return count;
}

elm::List<Edge*> DetailedPath::toOrderedPath() const
{
	elm::List<Edge*> rtn;
	for(EdgeIterator iter(*this); iter; iter++)
		rtn.addLast(*iter);
	return rtn;
}

elm::String DetailedPath::toString(bool colored) const
{
	elm::String str;
	if(! (dbg_flags&DBG_FORMAT_FLOWINFO))
	{
		// show the list of flowinfo and edges
		bool first = true;
		for(Iter iter(*this); iter; iter++)
		{
			if(first) first = false; else
				str = str.concat(_ << ", ");
			str = str.concat(iter->toString(colored));
		}
	}
	else
	{
		// prettier print
		bool comma = false;
		// if(countEdges() > 0)
		// 	str = _ << color::Dim() << firstEdge()->source()->cfg() << "(" << color::NoDim();
		for(Iter iter(*this); iter; iter++)
		{
			switch(iter->kind())
			{
				case FlowInfo::KIND_EDGE:
					if(comma) str = str.concat(_ << ", ");
					str = str.concat(iter->toString(colored));
					comma = true;
					break;
				case FlowInfo::KIND_LOOP_ENTRY:
					if(comma) str = str.concat(_ << ", ");
					str = str.concat(_ << color::Dim() << "L#" << iter->getLoopHeader()->index() << "{" << color::NoDim());
					comma = false;
					break;
				case FlowInfo::KIND_LOOP_EXIT:
					str = str.concat( _ << color::Dim() << "}" << color::NoDim());
					comma = true;
					break;
				case FlowInfo::KIND_CALL:
					if(comma) str = str.concat(_ << ", ");
					str = str.concat( _ << color::Dim() << iter->getCaller()->callee() << "(" << color::NoDim());
					comma = false;
					break;
				case FlowInfo::KIND_RETURN:
					str = str.concat(_ << color::Dim() << ")" << color::NoDim());
					comma = true;
					break;
				default:
					str = str.concat( _ << "[UKNOWN KIND " << (int)iter->kind() << "]");
					break;
			}
		}
		// if(_path)
		// 	str = str.concat(_ << color::Dim() << ")" << color::NoDim());
	}
	return str;
}

io::Output& DetailedPath::print(io::Output& out) const
{
	return out << toString();
}

elm::String DetailedPath::FlowInfo::toString(bool colored) const
{
	switch(_kind)
	{
		case KIND_EDGE:
			{
				ASSERT(getEdge()->source()->cfg() == getEdge()->target()->cfg());
				elm::String prefix, suffix;
				if(! (dbg_flags&DBG_FORMAT_FLOWINFO)) {
					prefix = _ << getEdge()->source()->cfg() << "(";
					suffix = ")";
				}
				if(getEdge()->source()->isBasic() && getEdge()->target()->isBasic())
					return _ << prefix << getEdge()->source()->index() << "->" << getEdge()->target()->index() << suffix;
				else if(getEdge()->source()->isBasic() && getEdge()->target()->isExit())
					return _ << prefix << getEdge()->source()->index() << "->" << getEdge()->target()->index() << "$" << suffix;
				else if(getEdge()->source()->isCall() && getEdge()->target()->isBasic())
					return _ << prefix << "@" << getEdge()->source()->toSynth()->callee() << "->" << getEdge()->target()->index() << suffix;
				else if(getEdge()->source()->isBasic() && getEdge()->target()->isCall())
					return _ << prefix << getEdge()->source()->index() << "->@" << getEdge()->target()->toSynth()->callee() << suffix;
				else if(getEdge()->source()->isEntry() && getEdge()->target()->isBasic())
					return _ << prefix << "^" << getEdge()->source()->cfg() << "->" << getEdge()->target()->index() << suffix;
				else
					return _ << getEdge();
			}
		case KIND_LOOP_ENTRY:
			return _ << color::Dim() << "LEn#" << getLoopHeader()->index() << color::NoDim();
		case KIND_LOOP_EXIT:
			return _ << color::Dim() << "LEx#" << getLoopHeader()->index() << color::NoDim();
		case KIND_CALL:
			return _ << color::Dim() << "C#" << getCaller()->callee() << color::NoDim();
			// return _ << color::Dim() << "C#" << getEdge()->source()->index() << "-" << getEdge()->target()->index() << color::NoDim();
		case KIND_RETURN:
			return _ << color::Dim() << "R#" << getCaller()->callee() << color::NoDim();
		default:
			return _ << "[UKNOWN KIND " << (int)_kind << "]";
	}
}

io::Output& DetailedPath::FlowInfo::print(io::Output& out) const
{
	return (out << toString());
}


/*void InfeasiblePaths::addDisorderedPath(const Set<Edge*>& infeasible_path, const elm::List<Edge*>& full_path, Edge* last_edge)
{
	elm::List<Edge*> ordered_ip;
	for(elm::List<Edge*>::Iter full_path_iter(full_path); full_path_iter; full_path_iter++)
	{
		if(infeasible_path.contains(*full_path_iter))
			ordered_ip.addLast(*full_path_iter);
	}
	if(infeasible_path.contains(last_edge))
		ordered_ip.addLast(last_edge);
	ips.add(ordered_ip);
}*/
