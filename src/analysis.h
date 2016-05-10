#ifndef _ANALYSIS_H
#define _ANALYSIS_H

#include <elm/genstruct/SLList.h>
#include <otawa/cfg/Edge.h>
#include <otawa/cfg/features.h>
#include <otawa/dfa/State.h>
#include <otawa/prop/Identifier.h>
#include "detailed_path.h"
#include "operand.h"
#include "pretty_printing.h"

using namespace otawa;
using elm::genstruct::SLList;

class Analysis {
public:
	typedef SLList<Edge*> OrderedPath;
	typedef elm::avl::Set<Edge*> Path;
	class State; // Abstract state corresponding to a set of paths at one point of the program
	class States; // Collection of State representing an abstract state at one point of the program

	enum // flags 
	{
		// FOLLOW_CALLS		  = 0b1 << 1,
		//SUPERSILENT		  = 0b1 << 2,
		MERGE				  = 0b1 << 3,
		UNMINIMIZED_PATHS	  = 0b1 << 4,
		DRY_RUN				  = 0b1 << 5,
		SMT_CHECK_LINEAR	  = 0b1 << 6,
		SHOW_PROGRESS		  = 0b1 << 7,
	};
protected:
	typedef struct
	{
		const dfa::State* dfa_state;
		OperandVar sp; // Stack Pointer
		unsigned int max_tempvars;
		unsigned int max_registers;
	} context_t;

	class IPStats
	{
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
	Analysis(const context_t& context, int state_size_limit, int flags);
	~Analysis();
	const Vector<DetailedPath>& run(CFG *cfg);
	inline const Vector<DetailedPath>& infeasiblePaths() const { return infeasible_paths; }
	// static bool listOfFixpoints(const SLList<Analysis::State>& sl);
	static elm::String pathToString(const Path& path);
	static elm::String orderedPathToString(const OrderedPath& path);

protected:
	class Progress;
	class SolverProgress;
	context_t context;
	IPStats ip_stats;
	Analysis::Progress* progress;
	int state_size_limit, flags; // read by inherited class

	static bool checkInfeasiblePathValidity(const Vector<State>& sv, const Vector<Option<Path> >& sv_paths, /*const Edge* e,*/ const Path& infeasible_path, elm::String& counterexample);
	static DetailedPath reorderInfeasiblePath(const Path& infeasible_path, const DetailedPath& full_path);
	static void addDetailedInfeasiblePath(const DetailedPath& infeasible_path, Vector<DetailedPath>& infeasible_paths);
	static bool isSubPath(const OrderedPath& included_path, const Path& path_set);
	static void onAnyInfeasiblePath();
	template<template< class _ > class C>
		void purgeBottomStates(C<Analysis::State>& sc) const;

	typedef enum 
	{	
		ENTER, // not used: represented by no annotation, when we haven't entered the loop yet
		FIX,
		LEAVE,
	} loopheader_status_t; // Fixpoint status of the loop header, for annotation
	inline static loopheader_status_t loopStatus(Block* h) { ASSERT(LOOP_HEADER(h)); return LH_STATUS.get(h,ENTER); }
	inline static bool isConditional(Block* b) { return b->countOuts() > 1; }
	static Block* insAlias		   (Block* b);
	static Vector<Edge*> allIns    (Block* h);
	static Vector<Edge*> backIns   (Block* h);
	static Vector<Edge*> nonBackIns(Block* h);
	static Vector<Edge*> outsWithoutUnallowedExits(Block* b);
	static bool isAllowedExit(Edge* exit_edge);
	static elm::String printFixPointStatus(Block* b);

	// static Identifier<Analysis::States> EDGE_S; // Trace on an edge
	static Identifier<LockPtr<Analysis::States> > EDGE_S; // Trace on an edge
	static Identifier<Analysis::State>			  LH_S; // Trace on a loop header
private:
	static Identifier<loopheader_status_t>		  LH_STATUS; // Fixpt status of a loop header

	Vector<Block*> wl; // working list
	Vector<DetailedPath> infeasible_paths; // TODO: Set<Path, PathComparator<Path> > path; to make Set useful
	int total_paths, loop_header_count, bb_count;

	// virtual pure functions to implement
	virtual LockPtr<States> narrowing(const Vector<Edge*>& edges) const = 0;
	virtual bool inD_ip(const otawa::Edge* e) const = 0;
	virtual IPStats ipcheck(States& s, elm::genstruct::Vector<DetailedPath>& infeasible_paths) const = 0;

	// analysis.cpp
	// void debugProgress(int block_id, bool enable_smt) const;
	Analysis::State topState(Block* entry) const;
	void wl_push(Block* b);
	void printResults(int exec_time_ms) const;
	
	// analysis_cfg.cpp
	void processCFG(CFG *cfg);
	States& I(Block* b, States& s); // modifies existing states
	LockPtr<States> I(Edge* e, const States& s); // creates new states
	void removeDuplicateInfeasiblePaths();
	Option<Constant> getCurrentStackPointer(const SLList<Analysis::State>& sl) const;
	elm::String wlToString() const;

	bool anyEdgeHasTrace(const Vector<Edge*>& edges) const;
	bool anyEdgeHasTrace(const Block::EdgeIter& biter) const;
	bool allEdgesHaveTrace(const Vector<Edge*>& edges) const;
	bool allEdgesHaveTrace(const Block::EdgeIter& biter) const;

	// bool invalidate_constant_info
	enum
	{
		KEEP_CONSTANT_INFO = false,
		INVALIDATE_CONSTANT_INFO = true,
	};
}; // Analysis class

#endif
