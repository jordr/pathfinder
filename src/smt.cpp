/*
 *	Interfacing with the SMT solver
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

#include <elm/data/List.h>
#include "smt.h"
#include "debug.h"

/**
 * @class SMT
 * @author Jordy Ruiz
 * @brief Interface with the SMT solver
 */
SMT::SMT(int flags) : flags(flags) { }

/**
 * @fn Option<Analysis::Path> SMT::seekInfeasiblePaths(const Analysis::State& s);
 * @brief Check the satisfiability of a state
 * @param s State to check
 * @return If unsatisfiable, returns a path, otherwise elm::none
 */
Option<Analysis::Path*> SMT::seekInfeasiblePaths(const Analysis::State& s)
{
#ifdef V1
	// add the constant info to the the list of predicates
	elm::List<LabelledPredicate> labelled_preds = s.getLabelledPreds(); // implicit copy
	labelled_preds += s.getConstants().toPredicates(s.getDag());
	
	initialize(labelled_preds);
	ELM_DBGV(1, "Checking path " << s.dumpPath() << ": ")
	if(checkPredSat())
	{
		if(dbg_verbose == DBG_VERBOSE_ALL) cout << color::BGre() << "SAT\n";
		return elm::none;
	}
	if(dbg_verbose == DBG_VERBOSE_ALL) cout << color::BIRed() << "UNSAT\n";

	Analysis::Path *path = new Analysis::Path(); // will be deleted in oracle
	std::basic_string<char> unsat_core_output;
	bool unsat_core_success = retrieveUnsatCore(*path, labelled_preds, unsat_core_output);

	if(!unsat_core_success)
	{
		DBG(color::BIWhi() << "A)" << color::RCol() << " Extracting UNSAT core... " << color::IRed() << "FAILED!");
		DBG("   Using original predicates:")
		for(elm::List<LabelledPredicate>::Iter parse_iter(labelled_preds); parse_iter; parse_iter++)
		{
			if((*parse_iter).pred().isComplete())
			{
				DBG("   * " << (*parse_iter))
				path->addAll((*parse_iter).labels());
			}
		}
	}
	else
	{
		DBG(color::BIWhi() << "A)" << color::RCol() << " Extracting UNSAT core... " << color::IGre() << "SUCCESS!");
		DBG("   UNSAT core: " << unsat_core_output.c_str())
	}

	// printInfeasiblePath(path);
	if(path->isEmpty())
		return elm::none;
	return elm::some(path);
#else
	ASSERTP(false, "v1 SMT solving triggered, but Pathfinder was not built with -D V1");
#endif
}

Option<Analysis::Path*> SMT::seekInfeasiblePathsv2(const Analysis::State& s)
{
	initialize(s.getLabelledPreds());
	initialize(s.getLocalVariables(), s.getMemoryTable(), s.getDag());
	ELM_DBGV(1, "Checking path " << s.dumpPath() << ": ")
	if(checkPredSat())
	{
		if(dbg_verbose == DBG_VERBOSE_ALL) cout << color::BGre() << "SAT\n";
		return elm::none;
	}
	else
	{
		if(dbg_verbose == DBG_VERBOSE_ALL) cout << color::BIRed() << "UNSAT\n";

		DBG("   Predicates:")
		for(elm::List<LabelledPredicate>::Iter parse_iter(s.getLabelledPreds()); parse_iter; parse_iter++)
			if((*parse_iter).pred().isComplete())
				DBG("   * " << (*parse_iter))
		// TODO!! we need to implement labels on memory, and use those to minimize IPs.
		// Also failure case should be the union of all preds, not the full path.
		Analysis::Path *path = new Analysis::Path();
		for(DetailedPath::EdgeIterator iter(s.getDetailedPath()); iter; iter++)
			path->add(*iter);
		return elm::some(path);
	}
}


/**
 * @fn const elm::String SMT::printChosenSolverInfo();
 * @brief Print the name of the solver being used
 * @return Short string containing the name of the solver
 */
const elm::String SMT::printChosenSolverInfo()
{
#ifdef SMT_SOLVER_Z3
	return "z3";
#endif
#ifdef SMT_SOLVER_CVC4
	return "cvc4";
#endif
}

/*
bool SMT::checkPredSat(const elm::List<LabelledPredicate>& labelled_preds)
{	
	try
	{
		for(elm::List<LabelledPredicate>::Iter iter(labelled_preds); iter; iter++)
			if(Option<Expr> expr = getExpr(iter->pred()))
				smt.assertFormula(*expr); // second parameter to true for unsat cores

		bool isSat = smt.checkSat(em.mkConst(true), true).isSat(); // check satisfability, the second parameter enables unsat cores
		return isSat;
	}
	catch(CVC4::LogicException e)
	{
		#ifdef DBG_WARNINGS
			cout << "WARNING: non-linear call to CVC4, defaulted to SAT." << endl;
		#endif
		return true;
	}
}

void SMT::removeIncompletePredicates(elm::List<LabelledPredicate>& labelled_preds)
{
	elm::List<LabelledPredicate>::Iter iter(labelled_preds);
	while(iter)
	{
		if(!iter->pred().isComplete())
		{
			elm::List<LabelledPredicate>::Iter prev_iter(iter);
			iter++;
			labelled_preds.remove(prev_iter);
		}
		else
			iter++;
	}
}

elm::List<Analysis::Path> SMT::getAllInfeasiblePaths(const elm::List<LabelledPredicate>& labelled_preds, int index)
{	// Algorithm 3 (n~2^n)
	bool index_in_range = false;
	elm::List<LabelledPredicate> labelled_preds_truncated = labelled_preds;
	elm::List<LabelledPredicate>::Iter iter(labelled_preds_truncated);
	for(int i = 0; iter; iter++)
	{
		if(i++ == index)
		{
			index_in_range = true;
			break;
		}
	}
	if(!index_in_range) // we're done parsing predicates
	{
		Analysis::Path path; // typedef elm::List<const Edge*> Path;
		elm::List<Analysis::Path> path_list;
		for(elm::List<LabelledPredicate>::Iter parse_iter(labelled_preds); parse_iter; parse_iter++)
			path += (*parse_iter).labels();
		path_list += path;
		return path_list;
	}
	labelled_preds_truncated.remove(iter);
	// DBG(color::Whi() << "LPs: " << labelled_preds)
	// DBG(color::Whi() << "LPs_truncated: " << labelled_preds_truncated)

	if(checkPredSat(labelled_preds_truncated))
	{	// SAT: keep predicate in list
		return getAllInfeasiblePaths(labelled_preds, index+1);
	}
	else
	{	// UNSAT: fork
		elm::List<Analysis::Path> paths = getAllInfeasiblePaths(labelled_preds_truncated, index); // without the unnecessary predicate
							   paths+= getAllInfeasiblePaths(labelled_preds,         index+1); // with the unnecessary predicate
		return paths;
	}
}

// generate maps
void SMT::genPathPointBitMaps(const elm::List<Analysis::Path>& paths, AVLMap<const Edge*, unsigned int>& map_pathpoint_to_bit, Vector<const Edge*>& map_bit_to_pathpoint)
{
	unsigned int bit_count = 0;
	for(elm::List<Analysis::Path>::Iter iter(paths); iter; iter++)
	{
		Analysis::Path p = *iter;
		for(Analysis::Path::Iter subiter(p); subiter; subiter++)
		{
			if(!map_pathpoint_to_bit.hasKey(*subiter))
			{
				// DBG(bit_count << " <=> " << *subiter)
				map_pathpoint_to_bit.put(*subiter, bit_count++);
				map_bit_to_pathpoint.push(*subiter);
			}
		}
	}
}

// map paths to bitcodes
elm::List<BitVector> SMT::genBitCodes(const elm::List<Analysis::Path>& paths, const AVLMap<const Edge*, unsigned int>& map_pathpoint_to_bit, unsigned int used_bits)
{
	BitVector bitcode(used_bits);
	elm::List<BitVector> bitcode_list;
	for(elm::List<Analysis::Path>::Iter iter(paths); iter; iter++)
	{
		bitcode.clear();
		Analysis::Path p = *iter;
		for(Analysis::Path::Iter subiter(p); subiter; subiter++)
			bitcode.set(*map_pathpoint_to_bit.get(*subiter));
			// bitcode |= 1u << *map_pathpoint_to_bit.get(*subiter);
		bitcode_list += bitcode;
	}
	return bitcode_list;
}

BitVector SMT::getListOfPathsToKeep(const elm::List<BitVector>& bitcode_list)
{
	const int number_of_paths = bitcode_list.count();
	BitVector paths_to_keep(number_of_paths, true); // a priori, keep all paths
	int i = 0;
	for(elm::List<BitVector>::Iter iter(bitcode_list); iter; iter++, i++)
	{
		if(!paths_to_keep[i])
			continue;

		elm::List<BitVector>::Iter subiter(iter);
		subiter++;
		for(int j = i+1; subiter; subiter++, j++)
		{
			if(!paths_to_keep[j])
				continue;
			const BitVector &opd1 = *iter, &opd2 = *subiter;
			if(opd1 <= opd2)
			{	// opd2 is superfluous
				paths_to_keep.set(j, false);
				continue;
			}
			if(opd1 >= opd2)
			{	// opd1 is superfluous
				paths_to_keep.set(i, false);
				break;
			}
		}
	}
	return paths_to_keep;
}

elm::List<Analysis::Path> SMT::filterPaths(const elm::List<BitVector>& bitcode_list, const Vector<const Edge*>& map_bit_to_pathpoint, const BitVector& paths_to_keep, bool print_results)
{
	elm::List<Analysis::Path> filtered_paths;
	int index = 0;
	for(elm::List<BitVector>::Iter iter(bitcode_list); iter; iter++, index++)
	{
		if(!paths_to_keep[index])
			continue;
		Analysis::Path path; // this is a SLList
		elm::String str = "[";
		bool first = true;
		for(BitVector::OneIterator oneiter(*iter); oneiter; oneiter++)
		{
			const Edge* e = map_bit_to_pathpoint[*oneiter];
			if(first)
				first = false;
			else
				str = str.concat(_ << ", ");
			str = str.concat(_ << e->source()->number() << "->" << e->target()->number());
			// path.addLast(e); // preserving order for aesthetic purposes
			path += e;
		}
		str = str.concat(_ << "]");
		if(print_results)
			DBG(color::On_IRed() << "Inf. path found: " << str << " (bitcode=" << *iter << ")")
		filtered_paths += path;
	}
	return filtered_paths;
}
*/
