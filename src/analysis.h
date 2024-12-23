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
 * Warning: there are three versions
 * (i)   v1: cfg1, bb1, virtualized		-1
 * (ii)  v2: cfg1, bb2, virtualized		-2
 * (iii) v3: cfg2, bb2, non virtualized	-3
 */

#ifndef _ANALYSIS_H
#define _ANALYSIS_H

// #define V1 // v1 support (slows down v2 and v3)

// #include <elm/data/List.h>
#include <otawa/cfg/Edge.h>
#include <otawa/cfg/features.h>
#include <otawa/dfa/State.h>
#include <otawa/prop/Identifier.h>
#include "detailed_path.h"
#include "dom/GlobalDominance.h"
#include "features.h"
#include "pretty_printing.h"
#include "struct/DAG.h"
#include "struct/operand.h"
#include "struct/var_maker.h"
#include "working_list.h"

using namespace otawa;


class Analysis {
public:
	typedef elm::List<Edge*> OrderedPath;
	typedef elm::avl::Set<Edge*> Path;
	class State; // Abstract state corresponding to a set of paths at one point of the program
	class States; // Collection of State representing an abstract state at one point of the program
	class SPEquals;
	// static Identifier<int> ANALYSIS_FLAGS, NB_CORES, MERGE_THRESOLD;

	enum // flags
	{
		VERSION				 = 0b11, // must occupy the 2 first bits
		VIRTUALIZE_CFG		 = 1 <<  2,
		SLICE_CFG			 = 1 <<  3,
		REDUCE_LOOPS		 = 1 <<  4,
		USE_INITIAL_DATA 	 = 1 <<  5,
		MERGE			 	 = 1 <<  6,
		MERGE_AFTER_APPLY	 = 1 <<  7,
		DRY_RUN				 = 1 <<  8,
		SMT_CHECK_LINEAR	 = 1 <<  9,
		ALLOW_NONLINEAR_OPRS = 1 << 10,
		SHOW_PROGRESS		 = 1 << 11,
		POST_PROCESSING		 = 1 << 12,
		SP_CRITICAL			 = 1 << 15,
		CLEAN_TOPS			 = 1 << 16,
		ASSUME_IDENTICAL_SP	 = 1 << 17,
		NO_WIDENING			 = 1 << 18,
		UNMINIMIZED_PATHS	 = 1 << 19,
		CLAMP_PREDICATE_SIZE = 1 << 20,
	};
protected:
	typedef struct
	{
		const dfa::State* dfa_state;
		OperandVar sp; // Stack Pointer
		short max_tempvars;
		short max_registers;
	} context_t;
	typedef enum 
	{	
		ENTER=0, // not used: represented by no annotation, when we haven't entered the loop yet
		FIX,
		ACCEL,
		LEAVE,
	} loopheader_status_t; // Fixpoint status of the loop header, for annotation

	// just a reference on the INFEASIBLE_PATHS identifier
	class InfeasiblePaths {
	public:
		InfeasiblePaths() : ips(NULL) { }
		inline void init(CFG* cfg) { INFEASIBLE_PATHS(cfg) = Vector<DetailedPath>(); ips = &INFEASIBLE_PATHS.ref(cfg); }
		inline operator const Vector<DetailedPath>&() const { return *ips; }
		inline operator Vector<DetailedPath>&() { return *ips; }
		inline InfeasiblePaths& operator=(const Vector<DetailedPath>& x) { *ips = x; return *this; }

		// Vector methods
		inline int count(void) const { return ips->count(); }
		inline DetailedPath& get(const Vector<DetailedPath>::Iter& i) { return ips->get(i); }
		inline const DetailedPath& operator[](int i) const { return (*ips)[i]; }
	private:
		Vector<DetailedPath>* ips;
	};

	class IPStats {
	public:
		IPStats() : ip_count(0), unminimized_ip_count(0) { }
		IPStats(int ip_count, int unminimized_ip_count) : ip_count(ip_count), unminimized_ip_count(unminimized_ip_count) { }
		inline void onAnyInfeasiblePath() { ip_count++; }
		inline void onUnminimizedInfeasiblePath() { unminimized_ip_count++; }
		inline int getIPCount() const { return ip_count; }
		inline int getMinimizedIPCount() const { return ip_count - unminimized_ip_count; }
		inline int getUnminimizedIPCount() const { return unminimized_ip_count; }
		inline IPStats operator+(const IPStats& st) const { return IPStats(ip_count+st.ip_count, unminimized_ip_count+st.unminimized_ip_count); }
  		inline IPStats& operator+=(const IPStats& st) { ip_count += st.ip_count; unminimized_ip_count += st.unminimized_ip_count; return *this; }
		inline IPStats& operator=(const IPStats& st) { ip_count = st.ip_count; unminimized_ip_count = st.unminimized_ip_count; return *this; }
	private:
		int ip_count;
		int unminimized_ip_count;
	};

public:
	// Analysis(WorkSpace *ws, PropList &props, int flags, int merge_thresold, int nb_cores);
	Analysis();
	virtual ~Analysis();

protected:
	const Vector<DetailedPath>& run(const WorkSpace* ws);
	const Vector<DetailedPath>& run(CFG *cfg);
	// inline const Vector<DetailedPath>& infeasiblePaths() const { return infeasible_paths; }

	// --- otawa::Processor BEGIN ---
	virtual void configure(const PropList &props);
	virtual void processWorkSpace(WorkSpace *ws);
	// --- otawa::Processor END ---

	// shortcuts
	int version() const;
	inline bool multithreaded() const { return nb_cores > 1; }

	class Progress;
	class Progressv1;
	class Progressv2;
	class SolverProgress;
	context_t context;
	DAG* dag;
	LockPtr<VarMaker> vm;
	IPStats ip_stats;
	Analysis::Progress* progress;
	InfeasiblePaths infeasible_paths;
	int state_size_limit, nb_cores, flags; // read by inherited class

	static Identifier<LockPtr<Analysis::States> > EDGE_S; // Trace on an edge
	static Identifier<Analysis::State>			  LH_S; // Trace on a loop header
	static Identifier<Analysis::State>			  LH_S0; // Trace on a loop header
	static Identifier<OperandIter*>				  LH_I; // on lheaders
	static Identifier<loopheader_status_t>		  LH_STATUS; // Fixpt status of a loop header
	static Identifier<LockPtr<Analysis::States> > CFG_S; // Trace on a CFG
	static Identifier<LockPtr<VarMaker> > 		  CFG_VARS; // VarMaker on a CFG (useful?)

private:
	GlobalDominance* gdom;
	elm::sys::StopWatch sw;

	// virtual pure functions to implement
	virtual LockPtr<States> join(const Vector<Edge*>& edges) const = 0;
	virtual LockPtr<States> merge(LockPtr<States> v, Block* b) const = 0;

	virtual bool inD_ip(const otawa::Edge* e) const = 0;
	virtual IPStats ipcheck(States& s, Vector<DetailedPath>& infeasible_paths) const = 0;

protected:
	inline static loopheader_status_t loopStatus(const Block* h) { ASSERT(LOOP_HEADER(h)); return LH_STATUS.get(h,ENTER); }
	inline static void setLoopStatus(Block* h, loopheader_status_t ls) { ASSERT(LOOP_HEADER(h)); if(ls != ENTER) LH_STATUS.set(h,ls); else LH_STATUS.remove(h); }
	inline static bool isConditional(Block* b) { return b->countOuts() > 1; }
	const Operand* newLoopIterOpd(Block* h) { // WARNING: must only be called once per loop!
		LH_I(h) = new OperandIter(h); // never deleted
		return (flags & NO_WIDENING) ? Top : static_cast<const Operand*>(LH_I.get(h));
	}

	// oracle.cpp
	Block* insAlias		    (Block* b) const;
	Block* outsAlias	    (Block* b) const;
	Vector<Edge*> allIns    (Block* h) const;
	Vector<Edge*> backIns   (Block* h) const;
	Vector<Edge*> nonBackIns(Block* h) const;

	// analysis_state.cpp
	Analysis::State topState(Block* entry) const;
	// analysis.cpp
	void printResults(int exec_time_ms, int real_time_ms) const;
	void printInfeasiblePaths() const;
	void postProcessResults(CFG *cfg);
	void purgeBottomStates(States& sc) const;
	int countIPsOf(CFG* cfg) const;
	static Vector<Edge*> outsWithoutUnallowedExits(Block* b);
	static bool isAllowedExit(Edge* exit_edge);
	static elm::String printFixPointStatus(Block* b);
	static elm::String pathToString(const Path& path);
	static elm::String pathToString(const OrderedPath& path);

#ifdef V1 // TODO: make a clean version
	avl::Set<Block*> loops;
	int max_loop_depth;
	void addLoopStats(Block* b);
#endif
	
	// analysis_cfg.cpp
	void processProg(CFG* cfg);
	virtual void processCFG(CFG* cfg, bool use_initial_data) = 0;
	virtual void I(Block* b, LockPtr<Analysis::States> s) = 0; // modifies existing states
	LockPtr<States> I(const Vector<Edge*>::Iter& e, LockPtr<States> s); // creates new states
	static void onAnyInfeasiblePath();
	static bool checkInfeasiblePathValidity(const Vector<State>& sv, const Vector<Option<Path*> >& sv_paths, /*const Edge* e,*/ const Path& infeasible_path, elm::String& counterexample);
	static DetailedPath reorderInfeasiblePath(const Path& infeasible_path, const DetailedPath& full_path);
	static void addDetailedInfeasiblePath(const DetailedPath& infeasible_path, Vector<DetailedPath>& infeasible_paths);
	static bool isSubPath(const OrderedPath& included_path, const Path& path_set);

	bool anyEdgeHasTrace(const Vector<Edge*>& edges) const;
	bool anyEdgeHasTrace(const Block::EdgeIter& biter) const;
	bool allEdgesHaveTrace(const Vector<Edge*>& edges) const;
	bool allEdgesHaveTrace(const Block::EdgeIter& biter) const;

	class LoopExitIterator : public PreIterator<LoopExitIterator, Block*>
	{
	public:
		LoopExitIterator(Edge* e) : h(LOOP_EXIT_EDGE.get(e, NULL)), lhi(e->source()) { }
		inline bool ended(void) const
			{ return h == NULL; }
		inline Block* item(void) const
			{ return *lhi; }
		void next(void)
			{ if(*lhi != h) lhi++; else h = NULL; }
	private:
		Block* h;
		LoopHeaderIter lhi;
	};

private:
	// dominance stuff
	static Option<Edge*> f_dom(GlobalDominance* gdom, Edge* e1, Edge* e2);
	static Option<Edge*> f_postdom(GlobalDominance* gdom, Edge* e1, Edge* e2);
	int simplifyUsingDominance(Option<Edge*> (*f)(GlobalDominance* gdom, Edge* e1, Edge* e2));
	int removeDuplicateIPs(void);

	// bool invalidate_constant_info
	enum {
		KEEP_CONSTANT_INFO = false,
		INVALIDATE_CONSTANT_INFO = true,
	};

}; // Analysis class

#endif
