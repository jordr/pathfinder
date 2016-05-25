/**
 * Standard implementation of the Analysis
 */

// #include <elm/sys/Thread.h> // multithreading
#include "analysis_state.h"
#include "cfg_features.h"
#include "debug.h"
#include "oracle.h"
#include "progress.h"
#ifdef SMT_SOLVER_CVC4
	#include "cvc4/cvc4_smt.h"
 	typedef CVC4SMT chosen_smt_t;
#elif SMT_SOLVER_Z3
	#include "z3/z3_smt.h"
 	typedef Z3SMT chosen_smt_t;
#endif

DefaultAnalysis::DefaultAnalysis(const context_t& context, int state_size_limit, int flags)
	: Analysis(context, state_size_limit, flags) { }

/**
 * actually... this is widening
 */
LockPtr<Analysis::States> DefaultAnalysis::narrowing(const Vector<Edge*>& ins) const
{
	ASSERTP(ins, "narrowing given empty ingoing edges vector")
	LockPtr<States> v = vectorOfS(ins);
	Block* b = ins[0]->target();
	// loop header or too big state
	if(LOOP_HEADER(b) || ((flags&MERGE) && v->count() > state_size_limit))
	{
		if(LOOP_HEADER(b) && LH_S.exists(b)) // also merge with the state on the loop header if it exists
			v->push(LH_S(b));
		purgeBottomStates(v->states());
		// ASSERTP(v, "Loop Header received only bottom state, case not handled yet. The main algorithm will use s[0] so...")
		if(v->isEmpty())
		{
			DBGG("narrowing returns null vector")
			return v;
		}
		State s((Edge*)NULL, context, false); // entry is cleared anyway
		s.merge(v->states(), b); // s <- widening(s0, s1, ..., sn)
		LockPtr<States> rtnv(new States(1));
		rtnv->push(s);
		if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY && v->count() > 50)
			cout << " " << v->count() << " states merged into 1 (from " << ins.count() << " ins)." << endl;
		return rtnv;
	}
	else
		return v; // no widening
}

/**
 * @brief Checks if a path ending with a certain edge is within the domain D of path we test the (in)feasibility of
 * @param e Edge the path ends with
 * @return true if the path is in D
 */
bool DefaultAnalysis::inD_ip(const otawa::Edge* e) const
{
	bool all_leave = true; // enable SMT when on sequential level
	for(LoopHeaderIter lh(e->source()); lh; lh++)
	{
		if(loopStatus(lh) != LEAVE)
		{
			all_leave = false;
			break;
		}
	}
	return all_leave && isConditional(e->source()); //TODOv2: add condition: && isConditional(e->source())
}

// look for infeasible paths, add them to infeasible_paths, and removes the states from ss
Analysis::IPStats DefaultAnalysis::ipcheck(States& ss, elm::genstruct::Vector<DetailedPath>& infeasible_paths) const
// void Analysis::stateListToInfeasiblePathList(SLList<Option<Path> >& sl_paths, const SLList<Analysis::State>& sl, Edge* e, bool is_conditional)
{
	IPStats stats;
	if(flags&DRY_RUN) // no SMT call
		return stats;
	// find the conflicts
	const int state_count = ss.count();
	Vector<Option<Path> > vl_paths;
	Vector<Analysis::State> new_sv(state_count); // safer to do it this way than remove on the fly (i think more convenient later too)
	SolverProgress* sprogress;
	if(flags&SHOW_PROGRESS)
		sprogress = new SolverProgress(state_count);
	for(States::Iterator si(ss.states()); si; si++)
	{
		// SMT call
		chosen_smt_t smt(flags);
		const Option<Path>& infeasible_path = smt.seekInfeasiblePaths(*si);
		vl_paths.addLast(infeasible_path);
		if(infeasible_path)
		{
#			ifdef DBG_WARNINGS
				if(! (*infeasible_path).contains(si->lastEdge())) // make sure the last edge was relevant in this path
					cerr << "WARNING: !infeasible_path->contains(s.lastEdge())" << endl;
#			endif
		}
		else
			new_sv.addLast(*si); // only add feasible states to new_sv
		if(flags&SHOW_PROGRESS)
			sprogress->onSolving(infeasible_path);
	}
	if(flags&SHOW_PROGRESS)
		delete sprogress;
	// analyse the conflicts found
	ASSERTP(ss.count() == vl_paths.count(), "different size of ss and vl_paths")
	Vector<Option<Path> >::Iterator pi(vl_paths);
	for(States::Iterator si(ss.states()); si; si++, pi++) // iterate on paths and states simultaneously
	{
		const State& s = *si;
		if(*pi) // is infeasible?
		{
			const Path& ip = **pi;
			elm::String counterexample;
			DBG("Path " << s.getPathString() << color::Bold() << " minimized to " << color::NoBold() << pathToString(ip))
			bool valid = checkInfeasiblePathValidity(ss.states(), vl_paths, ip, counterexample);
			DBG(color::BIWhi() << "B)" << color::RCol() << " Verifying minimized path validity... " << (valid?color::IGre():color::IRed()) << (valid?"SUCCESS!":"FAILED!"))
			stats.onAnyInfeasiblePath();
			if(valid)
			{
				DetailedPath reordered_path = reorderInfeasiblePath(ip, s.getDetailedPath());
				reordered_path.optimize();
				addDetailedInfeasiblePath(reordered_path, infeasible_paths); // infeasible_paths += order(ip); to output proprer ffx!
				DBG(color::On_IRed() << "Inf. path found: " << reordered_path << color::RCol())
			}
			else // we found a counterexample, e.g. a feasible path that is included in the set of paths we marked as infeasible
			{
				DetailedPath full_path = s.getDetailedPath();
				full_path.optimize();
				DBG("   counterexample: " << counterexample)
				stats.onUnminimizedInfeasiblePath();
				if(flags&UNMINIMIZED_PATHS)
				{	// falling back on full path (not as useful as a result, but still something)
					addDetailedInfeasiblePath(full_path, infeasible_paths);
					if(dbg_verbose == DBG_VERBOSE_ALL) {
						Path fp;
						for(DetailedPath::EdgeIterator fpi(full_path); fpi; fpi++)
							fp += *fpi;
						DBG(color::On_IRed() << "Inf. path found: " << pathToString(fp) << color::RCol() << " (unrefined)")
					}
				}
				else
					DBG(color::IRed() << "Ignored infeasible path that could not be minimized")
			}
			onAnyInfeasiblePath();
		}
	}
	ss = new_sv;
	return stats;
}

/*SLList<Analysis::State> DefaultAnalysis::listOfS(const Vector<Edge*>& ins) const
{
	SLList<State> sl;
	for(Vector<Edge*>::Iterator i(ins); i; i++)
		sl.addAll(EDGE_S.use(*i).states());
	return sl;
}*/

LockPtr<Analysis::States> DefaultAnalysis::vectorOfS(const Vector<Edge*>& ins) const
{
	if(ins.count() == 1) // opti TODO! should work now with the LockPtr
		return EDGE_S.use(ins[0]);
	LockPtr<States> s(new States());
	for(Vector<Edge*>::Iterator i(ins); i; i++)
		s->states().addAll(EDGE_S.use(*i)->states());
	return s;
}
