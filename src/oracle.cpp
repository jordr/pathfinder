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
 * Standard implementation of the Analysis
 */

#include "analysis_states.h"
#include "cfg_features.h"
#include "debug.h"
#include "oracle.h"
#include "progress.h"
#include "smt_job.h"
#ifdef SMT_SOLVER_CVC4
	#include "cvc4/cvc4_smt.h"
 	typedef CVC4SMT chosen_smt_t;
#elif SMT_SOLVER_Z3
	#include "z3/z3_smt.h"
 	typedef Z3SMT chosen_smt_t;
#else
 	#error "no SMT solver"
#endif

// note: we do this one time too much because the join when we leave is useless... maybe optimize that in the algorithm some day, it's a bit hard to do it cleanly
LockPtr<Analysis::States> DefaultAnalysis::join(const Vector<Edge*>& ins) const
{
	ASSERTP(ins, "join given empty ingoing edges vector")
	LockPtr<States> v = vectorOfS(ins);

	if((flags&MERGE) && v->count() > state_size_limit) // check for too large states
		v = merge(v, ins[0]->target());
	return v;
}

LockPtr<Analysis::States> DefaultAnalysis::merge(LockPtr<States> v, Block* b) const
{
	purgeBottomStates(*v);
	if(v->count() <= 1)
	{
		if(v->isEmpty())
			DBGG("merge returns null vector")
		return v;
	}
	else
	{
		State s((Edge*)NULL, context, dag, false); // entry is cleared anyway
		s.merge(*v, b, *vm); // s <- merging(s0, s1, ..., sn)
		LockPtr<States> rtnv(new States(1));
		rtnv->push(s);
		if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY && v->count() > 50)
			cout << " " << v->count() << " states merged into 1." << endl;
		return rtnv;
	}
}

/**
 * @brief Checks if a path ending with a certain edge is within the domain D of path we test the (in)feasibility of
 * @param e Edge the path ends with
 * @return true if the path is in D
 */
bool DefaultAnalysis::inD_ip(const otawa::Edge* e) const
{
	if(e->source()->isCall())
		return true; // when we come back from a call, we want to apply because we need to check all applied paths are SAT
	bool all_leave = true; // enable SMT when on sequential level
	for(LoopHeaderIter lh(e->source()); lh; lh++)
	{
		if(loopStatus(lh) != LEAVE/* && loopStatus(lh) != ACCEL*/) // TODO, add ACCEL?
		{
			all_leave = false;
			break;
		}
	}
	return all_leave && isConditional(e->source());
}

// look for infeasible paths, add them to infeasible_paths, and removes the states from ss
Analysis::IPStats DefaultAnalysis::ipcheck(States& ss, Vector<DetailedPath>& infeasible_paths) const
// void Analysis::stateListToInfeasiblePathList(elm::List<Option<Path> >& sl_paths, const elm::List<Analysis::State>& sl, Edge* e, bool is_conditional)
{
	IPStats stats;
	if(flags&DRY_RUN) // no SMT call
		return stats;

	const int state_count = ss.count();
	SolverProgress* sprogress;
	if(flags&SHOW_PROGRESS)
		sprogress = new SolverProgress(state_count);

	// find the conflicts
	Vector<Option<Path*> > sv_paths;
	Vector<Analysis::State> new_sv(state_count); // safer to do it this way than remove on the fly (i think more convenient later too)

	if(multithreaded() && state_count >= nb_cores)
	{	// with multithreading
		const int nb_threads = nb_cores;
		DBGG("1) Initializing " << nb_threads << " threads")
		Vector<elm::sys::Thread*> threads(nb_threads);
		Vector<SMTJob<chosen_smt_t>*> jobs(nb_threads);
		States::Iter si(ss.states());
		for(int tid = 0, i = 0; tid < nb_threads; tid++)
		{
			SMTJob<chosen_smt_t>* job = new SMTJob<chosen_smt_t>(flags);
			const int thresold = state_count * (tid+1)/nb_threads; // add states until this thresold
			DBGG("\tthread #" << tid << ", doing jobs [" << i << "," << thresold << "[")
			for(; i < thresold; i++, si++)
				job->addState(&*(si));
			elm::sys::Thread* t = elm::sys::Thread::make(*job);
			jobs.push(job);
			threads.push(t);
		}
		DBGG("2) Starting threads")
		for(int i = 0; i < nb_threads; i++)
			threads[i]->start();
		DBGG("3) Joining threads")
		// join and get result
		for(int i = 0; i < nb_threads; i++)
		{
			threads[i]->join();
			DBGG("\t(joined #" << i+1 << ")")
			for(SMTJob<chosen_smt_t>::Iter ji(jobs[i]->getResults()); ji; ji++)
			{
				const State* s = (*ji).fst;
				Option<Path*> infeasible_path = (*ji).snd;
				if(flags&SHOW_PROGRESS)
					sprogress->onSolving(infeasible_path);
				sv_paths.addLast(infeasible_path);
				if(!infeasible_path)
					new_sv.addLast(*s);
			}
			delete jobs[i];
			delete threads[i];
		}
		DBGG("4) done")
	}
	else
	{	// without multithreading
	 	DBGG("\t" << chosen_smt_t::name() << "(" << ss.count() << " states)")
		for(States::Iter si(ss.states()); si; si++)
		{	// SMT call
			chosen_smt_t smt(flags);
			const Option<Path*> infeasible_path = (version() == 1) ? smt.seekInfeasiblePaths(*si) : smt.seekInfeasiblePathsv2(*si);
			sv_paths.addLast(infeasible_path);
			if(!infeasible_path)
				new_sv.addLast(*si); // only add feasible states to new_sv
			// else if(! (*infeasible_path)->contains(si->lastEdge())) // make sure the last edge was relevant in this path
			// 	cerr << "WARNING: !infeasible_path->contains(s.lastEdge())" << endl;
			if(flags&SHOW_PROGRESS)
				sprogress->onSolving(infeasible_path);
		}
	}

	// cout << "["; for(Vector<Option<Path*> >::Iter i(sv_paths); i; i++)
	// 	if(*i) cout << pathToString(***i) << ", "; cout << "\n";
	if(flags&SHOW_PROGRESS)
		delete sprogress;
	// analyse the conflicts found
	ASSERTP(ss.count() == sv_paths.count(), "different size of ss and sv_paths")
	Vector<Option<Path*> >::Iter pi(sv_paths);
	for(States::Iter si(ss.states()); si; si++, pi++) // iterate on paths and states simultaneously
	{
		const State& s = *si;
		if(*pi) // is infeasible?
		{
			const Path& ip = ***pi;
			elm::String counterexample;
			DBG("Path " << s.dumpPath() << color::Bold() << " minimized to " << color::NoBold() << pathToString(ip))
			bool valid = checkInfeasiblePathValidity(ss.states(), sv_paths, ip, counterexample);
			DBG(color::BIWhi() << "B)" << color::RCol() << " Verifying minimized path validity... " << (valid?color::IGre():color::IRed()) << (valid?"SUCCESS!":"FAILED!"))
			stats.onAnyInfeasiblePath();
			if(valid)
			{
				DetailedPath reordered_path(reorderInfeasiblePath(ip, s.getDetailedPath()));
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
					if(dbg_verbose == DBG_VERBOSE_ALL)
					{
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
			delete *pi;
		}
	}
	for(Vector<Analysis::State>::Iter svi(new_sv); svi; svi++)
		new_sv[svi].removeConstantPredicates(); // remaining constant predicates are tautologies, there is no need to keep them
	ss = new_sv; // TODO! this is copying states, horribly unoptimized, we only need to remove a few states!
	return stats;
}

/*elm::List<Analysis::State> DefaultAnalysis::listOfS(const Vector<Edge*>& ins) const
{
	elm::List<State> sl;
	for(Vector<Edge*>::Iter i(ins); i; i++)
		sl.addAll(EDGE_S.use(*i).states());
	return sl;
}*/

LockPtr<Analysis::States> DefaultAnalysis::vectorOfS(const Vector<Edge*>& ins) const
{
	if(ins.count() == 1) // opti 
		return EDGE_S.use(ins[0]);
	LockPtr<States> s(new States());
	for(Vector<Edge*>::Iter i(ins); i; i++)
		s->states().addAll(EDGE_S.use(*i)->states());
	return s;
}
