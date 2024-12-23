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
 * General analysis methods
 */

#include <cmath> // sqrt
#include <ctime> // clock
#include <iomanip> // std::setprecision
#include <iostream> // std::cout
#include <elm/util/BitVector.h>
#include <otawa/cfg/Edge.h>
#include <otawa/cfg/features.h> // COLLECTED_CFG_FEATURE
#include <otawa/dfa/State.h> // INITIAL_STATE_FEATURE
#include <otawa/hard/Platform.h>
#include <otawa/oslice/features.h>
#include <otawa/prog/WorkSpace.h>
#include <sys/time.h>
#include "analysis_states.h"
#include "cfg_features.h"
#include "progress.h"
#include "smt.h"
#include "dom/GlobalDominance.h"

bool cfg_follow_calls = false; // for cfg_features.h
Identifier<int> otawa::ANALYSIS_FLAGS("otawa::pathfinder::ANALYSIS_FLAGS", -1);
Identifier<int> otawa::MERGE_THRESOLD("otawa::pathfinder::MERGE_THRESOLD", 0);
Identifier<int> otawa::NB_CORES("otawa::pathfinder::NB_CORES", 0);

Identifier<Vector<DetailedPath> > otawa::INFEASIBLE_PATHS("otawa::pathfinder::INFEASIBLE_PATHS", Vector<DetailedPath>()); // on a CFG

/**
 * @class Analysis
 * @author Jordy Ruiz 
 * @brief Perform an infeasible path analysis on a CFG 
 * @attention There are three versions (-1, -2, -3). Only -3 is modular, -1 and -2 inline the CFG, and only -2 and -3 use SSA-like abstract interpretation. -1 is basically only predicates.
 */
Analysis::Analysis()
#ifdef V1
	: max_loop_depth(0)
#endif
	{ }

Analysis::~Analysis()
{
	delete gdom;
	delete dag;
}

void Analysis::configure(const PropList &props)
{
	flags = ANALYSIS_FLAGS(props);
	state_size_limit = MERGE_THRESOLD(props);
	nb_cores = NB_CORES(props);

	ASSERTP(flags != -1, "flags must be set!")
	ASSERT(version() > 0)
#ifndef V1
	ASSERTP(version() > 1, "program was not built with v1 support")
#endif
}


void Analysis::processWorkSpace(WorkSpace *ws)
{
	/*ws->require(dfa::INITIAL_STATE_FEATURE); // dfa::INITIAL_STATE
	if(flags & REDUCE_LOOPS)
		ws->require(REDUCED_LOOPS_FEATURE); // for irregular loops
	ws->require(COLLECTED_CFG_FEATURE); // INVOLVED_CFGS
	if(flags & VIRTUALIZE_CFG) {
		cfg_follow_calls = true;
		ws->require(VIRTUALIZED_CFG_FEATURE); // inline calls
	}
#ifdef OSLICE
	if(flags & SLICE_CFG) {
		// oslice::SLICING_CFG_OUTPUT_PATH(props) = "slicing.dot";
		// oslice::SLICED_CFG_OUTPUT_PATH(props) = "sliced.dot";
		ws->require(oslice::COND_BRANCH_COLLECTOR_FEATURE);
		ws->require(oslice::SLICER_FEATURE);
	}
#endif
	*/
	ASSERTP(INVOLVED_CFGS(ws) && INVOLVED_CFGS(ws)->count() > 0, "no CFGs found");
	gdom = new GlobalDominance(INVOLVED_CFGS(ws), GlobalDominance::EDGE_DOM | GlobalDominance::EDGE_POSTDOM); // no block dominance
	context.dfa_state = dfa::INITIAL_STATE(ws); // initial state
	context.sp = ws->platform()->getSP()->number(); // id of the stack pointer
	context.max_tempvars = (short)ws->process()->maxTemp(); // maximum number of tempvars used
	context.max_registers = (short)ws->platform()->regCount(); // count of registers
	dag = new DAG(context.max_tempvars, context.max_registers);
	// vm will be initialized on CFG init

	run(ws);
}


int Analysis::version(void) const
{
	return flags & VERSION;
	// if(flags & IS_V1)
	// 	return 1;
	// if(flags & IS_V2)
	// 	return 2;
	// if(flags & IS_V3)
	// 	return 3;
	// crash();
}

/**
  * @fn const Vector<DetailedPath>& Analysis::run(const WorkSpace* ws);
  * @brief Run the analysis on the main CFG
  */
const Vector<DetailedPath>& Analysis::run(const WorkSpace* ws)
{
	return run(INVOLVED_CFGS(ws)->get(0));
}

/**
  * @fn const Vector<DetailedPath>& Analysis::run(CFG *cfg);
  * @brief Run the analysis on a specific CFG
  */
const Vector<DetailedPath>& Analysis::run(CFG *cfg)
{
	if(flags&SHOW_PROGRESS) progress = (version() == 3) ? (Progress*)new Progressv2() : (Progress*)new Progressv1(cfg);
	DBG("Analysis V" << version())
	DBG("Using SMT solver: " << (flags&DRY_RUN ? "(none)" : SMT::printChosenSolverInfo()))
	DBG("Stack pointer identified to " << context.sp)

    struct timeval tim;
	std::time_t start = clock();
	sw.start();
    gettimeofday(&tim, NULL);
    t::int64 t1 = tim.tv_sec*1000000+tim.tv_usec;
	
	processProg(cfg);

    gettimeofday(&tim, NULL);
    t::int64 t2 = tim.tv_sec*1000000+tim.tv_usec;
	sw.stop();
	std::time_t end = clock();
	
	postProcessResults(cfg);
	printResults((end-start)*1000/CLOCKS_PER_SEC, (t2-t1)/1000);
	if(flags&SHOW_PROGRESS) delete progress;
	return infeasible_paths;
}

/*
 * @fn void Analysis::debugProgress(int block_id, bool enable_smt) const;
 * Print progress of analysis
 */
/*void Analysis::debugProgress(int block_id, bool enable_smt) const
{
	if(dbg_verbose >= DBG_VERBOSE_RESULTS_ONLY && (dbg_flags&DBG_PROGRESS))
	{
		static int processed_bbs = 0;
		if(enable_smt)
			++processed_bbs; // only increase processed_bbs when we are in a state where we are no longer looking for a fixpoint
		cout << "[" << processed_bbs*100/bb_count << "%] Processed Block #" << block_id << " of " << bb_count << "        " << endl << "\e[1A";
	}
}*/

Block* Analysis::outsAlias(Block* b) const
{
	ASSERTP(!b->isUnknown(), "Block " << b << " is unknown, not supported by analysis.");
	if(flags&VIRTUALIZE_CFG)
	{
		if(b->isCall()) 
			return b->toSynth()->callee()->entry(); // call becomes callee entry
		else if(b->isExit())
			return getCaller(b, b); // exit becomes caller (remains exit if no caller)
		else
			return b;
	}
	else
		return b;
}

/**
  * @fn inline static loopheader_status_t Analysis::loopStatus(Block* h);
  * @brief Give the loop status of a Block
  * @param h Block to examinate
  * @return The loop status
*/

/**
  * @fn Block* Analysis::insAlias(Block* b);
  * @brief Substitue a block with the appropriate block to get ingoing edges from, in order to properly handle calls
  * @return The Block to substitute b with (by default, b)
*/
Block* Analysis::insAlias(Block* b) const
{
	if(flags & VIRTUALIZE_CFG)
	{
		if(b->isEntry()) // entry becomes caller
		{
			Option<Block*> rtn = getCaller(b->cfg());
			ASSERTP(rtn, "insAlias called on main entry - no alias with ins exists")
			return rtn;
		}
		else if(b->isCall()) // call becomes exit
			return b->toSynth()->callee()->exit();
		else
			return b;
	}
	else
		return b; // no aliasing in case of non-virtualized CFG
}
/**
 * @fn static Vector<Edge*> Analysis::allIns (Block* h) const;
 * @brief Collect all edges pointing to a block
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::allIns(Block* h) const
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		rtn.push(*i);
	
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting allIns...")
	return rtn;
}
/**
 * @fn static Vector<Edge*> Analysis::backIns(Block* h) const;
 * @brief Collect all back-edges pointing to a block
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::backIns(Block* h) const
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		if(BACK_EDGE(*i))
			rtn.push(*i);
	
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting backIns...")
	return rtn;
}
/**
 * @fn static Vector<Edge*> Analysis::nonBackIns(Block* h) const;
 * @brief Collect all edges pointing to a block that are not back edges of a loop
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::nonBackIns(Block* h) const
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		if(!BACK_EDGE(*i))
			rtn.push(*i);
	
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting nonBackIns...")
	return rtn;
}

/**
  * @brief check that all the loops this exits from are "LEAVE" status
  * aka e ∈ exits\{EX_h | src(e) ∈ L_h ∧ status_h ≠ LEAVE}
*/
bool Analysis::isAllowedExit(Edge* exit_edge)
{
	Block* outer_lh = LOOP_EXIT_EDGE(exit_edge);
	for(LoopHeaderIter lh(exit_edge->source()); lh; lh++)
	{
		if(loopStatus(lh) != LEAVE)
			return false;
		if(lh == outer_lh) // stop here
			break;	
	}
	return true;
}

/**
 * @fn Vector<Edge*> Analysis::outsWithoutUnallowedExits(Block* b);
 * @brief      Returns the list of outgoing edges from a block that pass the "isAllowedExit" check
 *
 * @param      b     The basic block we are at
 * @return     The list of allowed outgoing edges
 */
/* for e ∈ outs \ {EX_h | b ∈ L_h ∧ status_h ≠ LEAVE} */
Vector<Edge*> Analysis::outsWithoutUnallowedExits(Block* b)
{
	if(b->isExit())
		return nullVector<Edge*>();
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(b->outs()); i; i++)
		if(! LOOP_EXIT_EDGE.exists(*i) || isAllowedExit(*i))
			rtn.push(*i);
	ASSERTP(rtn, "outsWithoutUnallowedExits found no outs!")
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY)
	{
		for(Vector<Edge*>::Iter i(rtn); i; i++)
#ifndef NO_UTF8
			DBGG(color::Bold() << "\t\t└▶" << color::RCol() << i->target())
#else
			DBGG(color::Bold() << "\t\t->" << color::RCol() << i->target())
#endif
	}
	return rtn;
}

/**
 * @fn String Analysis::printFixPointStatus(Block* b);
  * @brief Short display of the fixpoint status of the current and enclosing loops (including caller CFGs)
  * @param b The block to process
  * @return The String contain the output
  */
String Analysis::printFixPointStatus(Block* b)
{
	String rtn = "[";
	for(LoopHeaderIter i(b); i; i++)
	{
		switch(loopStatus(*i))
		{
			case ENTER:
				rtn = rtn.concat(color::IRed() + "E");
				break;
			case FIX:
				rtn = rtn.concat(color::Yel() + "F");
				break;
			case ACCEL:
				rtn = rtn.concat(color::IPur() + "A");
				break;
			case LEAVE:
				rtn = rtn.concat(color::IGre() + "L");
				break;
		}
	}
	return rtn + color::RCol() + "]";
}


/**
 * @fn static bool Analysis::isSubPath(const OrderedPath& included_path, const Path& path_set);
 * @brief Checks if 'included_path' is a part of the set of paths "path_set", that is if 'included_path' includes all the edges in the Edge set of path_set
 * 
 * @return true if it is a subpath
*/
bool Analysis::isSubPath(const OrderedPath& included_path, const Path& path_set) 
{
	for(Path::Iter iter(path_set); iter; iter++)
		if(!included_path.contains(*iter))
			return false;
	return true;
}

/**
 * @fn elm::String Analysis::pathToString(const Path& path);
 * @brief Get pretty printing for any unordered Path (Set of Edge*)
 * 
 * @param path Path to parse
 * @return String representing the path
 */
elm::String Analysis::pathToString(const Path& path)
{
	if(dbg_flags&DBG_DETERMINISTIC)
		return _ << path.count() << " labels";
	else
	{
		elm::String str;
		bool first = true;
		for(Analysis::Path::Iter iter(path); iter; iter++)
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

/**
 * @fn elm::String Analysis::pathToString(const OrderedPath& path);
 * @brief Get pretty printing for any OrderedPath (SLList of Edge*)
 * 
 * @param path OrderedPath to parse
 * @return String representing the path
 */
elm::String Analysis::pathToString(const OrderedPath& path)
{
	elm::String str;
	bool first = true;
	int lastid = 0; // -Wmaybe-uninitialized
	for(OrderedPath::Iter iter(path); iter; iter++)
	{
		ASSERTP(first || (*iter)->source()->index() == lastid, "OrderedPath previous target and current source do not match! ex: 1->2, 2->4, 3->5");
		if(first)
		{
#ifndef NO_UTF8
			if((*iter)->source()->index() == 0)
				str = _ << "ε";
			else
#endif
				str = _ << (*iter)->source()->index();
			first = false;
		}
		str = _ << str << "->" << (*iter)->target()->index();
		lastid = (*iter)->target()->index();
	}
	if(str.isEmpty())
		return "(empty)";
	return str;
}

/**
 * @fn void Analysis::printResults(int exec_time_ms, int real_time_ms) const;
 * @brief Print results after a CFG analysis completes
 * 
 * @param exec_time_ms Measured execution time of the analysis (in ms)
 * @param real_time_ms Measured execution time of the analysis, using timeval (in ms)
 */
void Analysis::printResults(int exec_time_ms, int real_time_ms) const
{
	if(dbg_verbose == DBG_VERBOSE_NONE)
		return;
	const int ipcount = infeasible_paths.count();

	printInfeasiblePaths();
	cout << color::BIGre() << ipcount << color::RCol() << " infeasible path" << (ipcount == 1 ? " " : "s") << " ("
		 << color::IGre() << ipcount-ip_stats.getUnminimizedIPCount() << color::RCol() << " min + "
		 << color::Yel() << ip_stats.getUnminimizedIPCount() << color::RCol() << " unmin, implicitly "
		 << color::IRed() << ip_stats.getIPCount() << color::RCol() << ").";

	if(! (dbg_flags & DBG_DETERMINISTIC))
	{	// print execution time
		if(dbg_verbose == DBG_VERBOSE_ALL)
			cout << " (" << (real_time_ms>=1000 ? (float(real_time_ms)/float(1000)) : real_time_ms) << (real_time_ms>=1000 ? "s" : "ms") << ")" << color::RCol() << endl;
		else // not all verbose
		{
		    std::ios_base::fmtflags oldflags = std::cout.flags();
		    std::streamsize oldprecision = std::cout.precision();
			std::cout << std::fixed << std::setprecision(3) << color::IYel().chars() << " (" << real_time_ms/1000.f << "s)" << color::RCol().chars();
		    if(dbg_flags&DBG_DETAILED_STATS)
				std::cout << color::Yel().chars() << " [" << sw.delay()/1000000.f << " of " << exec_time_ms/1000.f << "s]" << color::RCol().chars();
		    std::cout.flags(oldflags);
		    std::cout.precision(oldprecision);
			std::cout << endl;
		}
	}
	else
		elm::cout << endl;
	// cout << "Minimized+Unminimized => Total w/o min. : " << color::On_Bla() << color::IGre() << ipcount-ip_stats.getUnminimizedIPCount() << color::RCol() <<
	// 		"+" << color::Yel() << ip_stats.getUnminimizedIPCount() << color::RCol() << " => " << color::IRed() << ip_stats.getIPCount() << color::RCol() << endl;
	if(dbg_flags & DBG_DETAILED_STATS)
	{
		if(ipcount > 0)
		{
			int sum_path_lengths = 0, squaredsum_path_lengths = 0, one_edges = 0;
			for(Vector<DetailedPath>::Iter iter(infeasible_paths); iter; iter++)
			{
				one_edges += iter->countEdges() == 1;
				sum_path_lengths += iter->countEdges();
				squaredsum_path_lengths += iter->countEdges() * iter->countEdges();
			}
			float average_length = (float)sum_path_lengths / (float)ipcount;
			float norm2 = sqrt((float)squaredsum_path_lengths / (float)ipcount);
		    std::ios_base::fmtflags oldflags = std::cout.flags();
		    std::streamsize oldprecision = std::cout.precision();
			std::cout << std::fixed << std::setprecision(2) << " (Average: " << average_length << ", Norm2: " << norm2
				<< ", #1edge: " << one_edges << "/" << ipcount << ")" << endl;
			std::cout.flags(oldflags);
			std::cout.precision(oldprecision);
		}
#ifdef V1
		std::cout << "Loops count: " << loops.count() << ", max depth: " << max_loop_depth << endl;
#else
		std::cout << "[use V1 to get loop stats]" << endl;
#endif
	}
}

/**
 * @brief      Print all infeasible paths
 */
void Analysis::printInfeasiblePaths() const
{
	if(dbg_flags&DBG_RESULT_IPS)
		for(Vector<DetailedPath>::Iter iter(infeasible_paths); iter; iter++)
		{
			CFG* f = (*iter).function();
			cout << "    * " << (f ? _ << f << ":" : elm::String()) << "[" << *iter << "]" << endl;
		}
}

/**
 * @fn int Analysis::countIPsOf(CFG* cfg) const;
 * @brief      Counts the number of infeasible paths in the scope of the CFG cfg.
 * @param      cfg   The control flow graphs
 * @return     Number of ips found
 */
int Analysis::countIPsOf(CFG* cfg) const
{
	int rtn = 0;
	for(Vector<DetailedPath>::Iter iter(infeasible_paths); iter; iter++)
		if((*iter).function() == cfg)
			rtn++;
	return rtn;
}

// returns edge to remove
Option<Edge*> Analysis::f_dom(GlobalDominance* gdom, Edge* e1, Edge* e2)
{
	DBG("\tdom(" << e1 << ", " << e2 << "): " << DBG_TEST(gdom->dom(e1, e2), false))
	if(gdom->dom(e1, e2))
		return elm::some(e1);
	return elm::none;
}
// returns edge to remove
Option<Edge*> Analysis::f_postdom(GlobalDominance* gdom, Edge* e1, Edge* e2)
{
	DBG("\tpostdom(" << e2 << ", " << e1 << "): " << DBG_TEST(gdom->postdom(e2, e1), false))
	if(gdom->postdom(e2, e1))
		return elm::some(e2);
	return elm::none;
}

int Analysis::simplifyUsingDominance(Option<Edge*> (*f)(GlobalDominance* gdom, Edge* e1, Edge* e2))
{
	int changed_count = 0;
	for(Vector<DetailedPath>::Iter dpiter(infeasible_paths); dpiter; dpiter++)
	{
		DetailedPath& dp = infeasible_paths.get(dpiter);
		DBG(dp << "...")
		bool hasChanged = false, changed = false;
		do
		{
			changed = false;
			elm::Option<DetailedPath::FlowInfo> prev = elm::none;
			for(DetailedPath::Iter i(dp); i; i++)
			{
				if(i->isEdge())
				{
					if(prev)
						if(Option<Edge*> edge_to_remove = f(gdom, (*prev).getEdge(), i->getEdge()))
						{
							dp.remove(edge_to_remove); // search and destroy
							changed = true;
							hasChanged = true;
							break;
						}
					prev = elm::some(*i);
				}
			}
		} while(changed);
		if(hasChanged) {
			dp.removeCallsAtEndOfPath();
			DBG("\t...to " << dp)
			changed_count++;
		}
	}
	return changed_count;
}

/**
 * @fn int Analysis::removeDuplicateIPs(void);
 * @brief Look for infeasible paths that share the same ordered list of edges and remove duplicates 
 * @return the amount of infeasible paths that were removed
 */
int Analysis::removeDuplicateIPs()
{
	const int n = infeasible_paths.count();
	if(!n) // no infeasible paths
		return 0;
	BitVector bv(n, true);
	for(int i = 0; i < n; i++)
		for(int j = i+1; j < n; j++)
			if(infeasible_paths[j] == infeasible_paths[i]) // found a duplicate
			{
				bv.set(i, false); // do not include i
				// ASSERTP(false, "it seems like we never hit this point?? rm assert if we do, this should be the case")
				break; // do not look any further
			}
	const int k = bv.countBits();
	Vector<DetailedPath> v(bv.countBits()); // the new infeasible paths vector will have the perfect size
	for(int i = 0; i < n; i++)
		if(bv[i])
			v.push(infeasible_paths[i]);
	if(k != n) // some elements have been removed
		infeasible_paths = v;
	return k - n;
}

/**
 * @fn void Analysis::postProcessResults(CFG *cfg);
 * @brief      Posts processes results by removing useless infeasible paths or edges within infeasible paths.
 * @param      cfg   The CFG to process
 */
void Analysis::postProcessResults(CFG *cfg)
{
	if(! (flags&POST_PROCESSING))
		return;
	DBG(color::On_IGre() << "post-processing..." << color::RCol())
	// elm::log::Debug::setDebugFlag(true);
	// elm::log::Debug::setVerboseLevel(1);
	/*otawa::Edge* program_entry_edge = theOnly(cfg->entry()->outs());
	for(Vector<DetailedPath>::MutableIterator dpiter(infeasible_paths); dpiter; dpiter++)
	{
		DBG("\tcontains entry: " << DBG_TEST(dp.contains(program_entry_edge), false))
		if(dp.contains(program_entry_edge))
			dp.remove(program_entry_edge);
	}*/ // should be removed by dominance anyway
	int count = simplifyUsingDominance(&f_dom);
	DBGG("Dominance: minimized " << count << " infeasible paths.")
	count = simplifyUsingDominance(&f_postdom);
	DBGG("Post-dominance: minimized " << count << " infeasible paths.")
	count = removeDuplicateIPs();
	DBGG("Removed " << count << " duplicate infeasible paths.")
}

/**
 * @fn void Analysis::purgeBottomStates(States& sc) const;
 * @brief Remove all bottom states from a Collection of States
 */
// template<template< class _ > class C>
void Analysis::purgeBottomStates(States& sc) const
{
	for(States::Iter i(sc); i; )
	{
		if(i.item().isBottom())
			sc.remove(i);
		else i++;
	}
}

#ifdef V1
/**
 * @fn void Analysis::addLoopStats(Block* b);
 * @brief      This function adds loops from a block b to the overall loop statistics
 * @param      b     The Block
 */
void Analysis::addLoopStats(Block* b)
{
	loops.add(b);
	int x = 0;
	for(LoopHeaderIter i(b); i; i++)
		x++;
	if(x > max_loop_depth)
		max_loop_depth = x;
}
#endif
