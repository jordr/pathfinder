/*
 *	Macro analysis: skeleton of the analysis algorithm, defines the way we parse the CFG 
 *  	WARNING: there are three versions
 *  	(i)   v1: cfg1, bb1, virtualized		-1z 1
 *  	(ii)  v2: cfg1, bb2, virtualized		-2z 1
 *  	(iii) v3: cfg2, bb2, non virtualized	-2z 0
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
 * 
 */
/**
 * Warning: there are three versions
 * (i)   v1: cfg1, bb1, virtualized		-1z 1
 * (ii)  v2: cfg1, bb2, virtualized		-2z 1
 * (iii) v3: cfg2, bb2, non virtualized	-2z 0
 */

#include <otawa/cfg/Edge.h>
#include "analysis_states.h"
#include "cfg_features.h"

using namespace elm::io;

Identifier<LockPtr<Analysis::States> >		Analysis::EDGE_S("Trace on an edge"); // old PROCESSED_EDGES
Identifier<Analysis::State>					Analysis::LH_S("Trace on a loop header"); // maybe change to vector
Identifier<Analysis::loopheader_status_t>	Analysis::LH_STATUS("Fixpt status of a loop (on a loop header)");
// for non-virtualized CFGs only
Identifier<Analysis::State>					Analysis::LH_S0("Initial trace on a loop header"); // maybe change to vector
Identifier<OperandIter*>					Analysis::LH_I("Iter operand for the loop"); // on lheaders
Identifier<LockPtr<Analysis::States> >		Analysis::CFG_S("Trace on a CFG");
Identifier<LockPtr<VarMaker> >				Analysis::CFG_VARS("VarMaker of a CFG"); // VarMaker to be copied, updated, and appended on f° return


/**
 * @fn void Analysis::processProg(CFG* cfg)
 * @brief       Runs the analysis on a whole program
 * @param cfg   Entry CFG
 */
void Analysis::processProg(CFG* cfg)
{
	/* ips ← {} */
	infeasible_paths.init(cfg);
	/* ... */
	processCFG(cfg, flags&USE_INITIAL_DATA);
	DBGG(IGre() << "Reached end of program.")
}

/**
 * @brief      Interpretation function of an Edge. Returns a processed copy of the provided states.
 */
LockPtr<Analysis::States> Analysis::I(const Vector<Edge*>::Iter& e, LockPtr<States> s)
{
	if(! e.ended()) // more edges to come
		s = LockPtr<States>(new States(*s));
	if(s->isEmpty())
		DBGG("-\tpropagating bottom state")
	// DBGG(Bold() << "-\tI(e= " << NoBold() << e << Bold() << " )" << NoBold() << (e->source()->isEntry() ? " (entry)" : ""))
	if(! e->source()->isEntry()) // do not process CFG entry: no generated preds and uninteresting edge to add (everything comes from the entry)
		for(States::Iter si(s->states()); si; si++)
			(*s)[si].appendEdge(e);
	if(LOOP_EXIT_EDGE(e))
		s->onLoopExit(e);
	return s;
}

/**
 * @fn bool Analysis::allEdgesHaveTrace(const Vector<Edge*>& edges) const;
 * @brief Checks for all edges to have a trace annotation
 * @param edges Vector of Block edges to inspect
 * @return true iff all edges have a trace annotation
 */
bool Analysis::allEdgesHaveTrace(const Vector<Edge*>& edges) const
{
	for(Vector<Edge*>::Iter iter(edges); iter; iter++)
		if(!EDGE_S.exists(*iter))
			return false;
	DBGG("-..." << edges)
	return true;
}

/**
 * @fn bool Analysis::allEdgesHaveTrace(const Block::EdgeIter& biter) const;
 * @brief Checks for all edges to have a trace annotation
 * @param biter Iter over the list of Block edges to inspect
 * @return true iff all edges have a trace annotation
 */
bool Analysis::allEdgesHaveTrace(const Block::EdgeIter& biter) const
{
	for(Block::EdgeIter i(biter); i; i++)
		if(!EDGE_S.exists(*i))
			return false;
	return true;
}

/**
 * @fn bool Analysis::anyEdgeHasTrace(const Vector<Edge*>& edges) const;
 * @brief Checks for at least one edge to have a trace annotation
 * @param edges Vector of Block edges to inspect
 * @return true iff at least one edge has a trace annotation
 */
bool Analysis::anyEdgeHasTrace(const Vector<Edge*>& edges) const
{
	for(Vector<Edge*>::Iter iter(edges); iter; iter++)
		if(EDGE_S.exists(*iter))
			return true;
	return false;
}

/**
 * @fn bool Analysis::anyEdgeHasTrace(const Block::EdgeIter& biter) const;
 * @brief Checks for at least one edge to have a trace annotation
 * @param biter Iter over the list of Block edges to inspect
 * @return true iff at least one edge has a trace annotation
 */
bool Analysis::anyEdgeHasTrace(const Block::EdgeIter& biter) const
{
	for(Block::EdgeIter i(biter); i; i++)
		if(EDGE_S.exists(*i))
			return true;
	return false;
}

/**
 * @fn bool Analysis::checkInfeasiblePathValidity(const Vector<Analysis::State>& sl, const Vector<Option<Path> >& sl_paths, const Edge* e, const Path& infeasible_path, elm::String& counterexample);
 * Checks if the minimized list of edges we found 'infeasible_path' is valid,
 * that is if all the paths it represents (all the 'sl[i]->e') are infeasible ('sl_paths[i]' is not 'elm::option::none')
 * If invalid, returns a counter-example in counterexample.
 * @return true if valid
*/
bool Analysis::checkInfeasiblePathValidity(const Vector<Analysis::State>& sv, const Vector<Option<Path*> >& sv_paths, const Path& infeasible_path, elm::String& counterexample)
{
	// check that all the paths going to the current BB are sound with the minimized inf. path we just found
	Vector<Option<Path*> >::Iter pi(sv_paths);
	for(Vector<Analysis::State>::Iter si(sv); si; si++, pi++) // iterate through paths at the same time
	{
		// if feasible path && contained in the minimized inf. path
		if((*pi).isNone() && isSubPath(si->getDetailedPath().toOrderedPath(), infeasible_path))
		{
			counterexample = si->dumpPath();
			return false;
		}
	}
	return true;
}

/**
 * @brief Reorder the infeasible path ip using the order path full_path
 * @param ip Infeasible path to add
 * @param full_path Full path it originates from (must include ip)
 */
DetailedPath Analysis::reorderInfeasiblePath(const Path& ip, const DetailedPath& full_path)
{
	DetailedPath ordered_ip(full_path.function());
	// parse the detailed path and add all the edges that match the Path ip and the loop entries
	for(DetailedPath::Iter full_path_iter(full_path); full_path_iter; full_path_iter++)
	{
		if(full_path_iter->isEdge())
		{
			if(ip.contains(full_path_iter->getEdge()))
				ordered_ip.addLast(*full_path_iter);
		}
		else
			ordered_ip.addLast(*full_path_iter);
	}
	DBG("reordered");
	DBG("\t" << pathToString(ip) << " to");
	DBG("\t" << ordered_ip << ", full_path was");
	DBG("\t" << full_path)
	// DBG("reorderInfeasiblePath(...), ip=" << pathToString(ip) << ", " << ICya() << "full_path=[" << full_path << "]" 
		// << RCol() << ", result=" << ordered_ip);
	return ordered_ip;
}

/**
 * @brief      Register a new infeasible path
 *
 * @param[in]  ip                The infeasible path to add
 * @param      infeasible_paths  The collection of paths to add to
 */
void Analysis::addDetailedInfeasiblePath(const DetailedPath& ip, Vector<DetailedPath>& infeasible_paths)
{
	DetailedPath new_ip(ip);
	ASSERT(ip.hasAnEdge());
	// first off, add loop entries for all missing loops
	// Block *b = ip.firstEdge()->source();
	// for(LoopHeaderIter i(b); i; i++)
	// 	new_ip.addEnclosingLoop(*i);
	if(! infeasible_paths.contains(new_ip))
		infeasible_paths.add(new_ip);
	else
		DBG("not adding redundant IP: " << new_ip)
}

/**
 * @fn static void Analysis::onAnyInfeasiblePath();
 * @brief Debugs to do when detecting any infeasible path
 */
void Analysis::onAnyInfeasiblePath()
{
	DBG(BIYel() << "Stopping current path analysis")
}

/**
 * @fn inline static bool Analysis::isConditional(Block* b);
 * @brief Test if a BasicBlock is conditional, i.e. has more than one edge
 * @param b Block to test
 */
