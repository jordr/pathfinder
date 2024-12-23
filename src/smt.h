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
 * Interfacing with the SMT solver
 */
#ifndef _SMT_H
#define _SMT_H

// check for consistent SMT options
#ifdef SMT_SOLVER_CVC4
	#ifdef SMT_SOLVER_Z3
		#error "Conflict: several SMT solvers specified!"
	#endif
#else
	#ifndef SMT_SOLVER_Z3
		#error "No SMT solver specified!"
	#endif
#endif

#include "analysis_state.h"
#include "struct/DAG.h"

class SMT
{
public:
	SMT(int flags);
	Option<Analysis::Path*> seekInfeasiblePaths(const Analysis::State& s);
	Option<Analysis::Path*> seekInfeasiblePathsv2(const Analysis::State& s);
	static const elm::String printChosenSolverInfo();
	
private:
	virtual void initialize(const elm::List<LabelledPredicate>& labelled_preds) = 0;
	virtual void initialize(const LocalVariables& lv, const elm::HashTable<Constant, const Operand*, ConstantHash>& mem, DAG& dag) = 0;
	virtual bool checkPredSat() = 0;
	virtual bool retrieveUnsatCore(Analysis::Path& path, const elm::List<LabelledPredicate>& labelled_preds, std::basic_string<char>& unsat_core_output) = 0;

protected:
	int flags;
};

#endif
