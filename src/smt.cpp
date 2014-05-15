// Interfacing with the SMT solver

#include <elm/genstruct/SLList.h>
#include <elm/genstruct/Vector.h>
#include <elm/util/BitVector.h>
#include <otawa/cfg/Edge.h>
#include "smt.h"
#include "smt_variable_stack.h"
#include "analysis.h"
#include "debug.h"

using namespace CVC4::kind;
using elm::BitVector;
using otawa::Edge;
using CVC4::Expr;

SMT::SMT(): smt(&em), integer(em.integerType())
{
	smt.setLogic("QF_LIA"); // Quantifier-Free (no forall, exists...) Linear Integer Arithmetic
}

// TODO: remove
void SMT::testSimplify(SLList<Analysis::LabelledPredicate> labelled_preds)
{
	for(SLList<Analysis::LabelledPredicate>::Iterator it(labelled_preds); it; it++)
	{
		Analysis::LabelledPredicate lp = *it;
		Predicate p = lp.pred();
		Option<CVC4::Expr> e = getExpr(p);
		if(!e) continue;
		CVC4::Expr e2 = smt.simplify(*e);
		DBG_STD("before: p = " << *e)
		DBG_STD(COLOR_IRed "         => " << e2)
	}
}

Option<SLList<Analysis::Path> > SMT::seekInfeasiblePaths(SLList<Analysis::LabelledPredicate> labelled_preds)
{
	testSimplify(labelled_preds);

	if(checkPredSat(labelled_preds, true))
		return elm::none; // no inconsistency found

	removeIncompletePredicates(labelled_preds); // optimization: incomplete predicates are not sent to the SMT therefore they cannot play a role in the UNSAT
	SLList<Analysis::Path> paths = getAllInfeasiblePaths(labelled_preds); // exhaustive list of paths
	AVLMap<const Edge*, unsigned int> map_pathpoint_to_bit;
	Vector<const Edge*> map_bit_to_pathpoint;
	genPathPointBitMaps(paths, map_pathpoint_to_bit, map_bit_to_pathpoint); // generate maps to convert Path <-> BitVector
	const Vector<BitVector>& bitcode_vector = genBitCodes(paths, map_pathpoint_to_bit, map_bit_to_pathpoint.length()); // generate the list of BitVectors corresponding to the list of paths
	const BitVector& paths_to_keep = getListOfPathsToKeep(bitcode_vector); // get the list of relevant paths (removes redundant bitcode such as 101 in {101, 100})
	
	return elm::some(filterPaths(bitcode_vector, map_bit_to_pathpoint, paths_to_keep, true)); // generate the list of non-redundant paths and return it
}

void SMT::removeIncompletePredicates(SLList<Analysis::LabelledPredicate>& labelled_preds)
{
	SLList<Analysis::LabelledPredicate>::Iterator iter(labelled_preds);
	while(iter)
	{
		if(!(*iter).pred().isComplete())
		{
			SLList<Analysis::LabelledPredicate>::Iterator prev_iter(iter);
			iter++;
			labelled_preds.remove(prev_iter);
		}
		else
			iter++;
	}
}

SLList<Analysis::Path> SMT::getAllInfeasiblePaths(const SLList<Analysis::LabelledPredicate>& labelled_preds, int index)
{	// Algorithm 3 (n~2^n)
	bool index_in_range = false;
	SLList<Analysis::LabelledPredicate> labelled_preds_truncated = labelled_preds;
	SLList<Analysis::LabelledPredicate>::Iterator iter(labelled_preds_truncated);
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
		// typedef SLList<const Edge*> Path;
		SLList<const Edge*> path;
		SLList<Analysis::Path> path_list;
		for(SLList<Analysis::LabelledPredicate>::Iterator parse_iter(labelled_preds); parse_iter; parse_iter++)
			path += (*parse_iter).label();
		path_list += path;
		return path_list;
	}
	labelled_preds_truncated.remove(iter);
	// DBG(COLOR_Whi << "LPs: " << labelled_preds)
	// DBG(COLOR_Whi << "LPs_truncated: " << labelled_preds_truncated)

	if(checkPredSat(labelled_preds_truncated))
	{	// SAT: keep predicate in list
		return getAllInfeasiblePaths(labelled_preds, index+1);
	}
	else
	{	// UNSAT: fork
		SLList<Analysis::Path> paths = getAllInfeasiblePaths(labelled_preds_truncated, index); // without the unnecessary predicate
							   paths+= getAllInfeasiblePaths(labelled_preds,         index+1); // with the unnecessary predicate
		return paths;
	}
}

// generate maps
void SMT::genPathPointBitMaps(const SLList<Analysis::Path>& paths, AVLMap<const Edge*, unsigned int>& map_pathpoint_to_bit, Vector<const Edge*>& map_bit_to_pathpoint)
{
	unsigned int bit_count = 0;
	for(SLList<Analysis::Path>::Iterator iter(paths); iter; iter++)
	{
		SLList<const Edge*> p = *iter;
		for(SLList<const Edge*>::Iterator subiter(p); subiter; subiter++)
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
Vector<BitVector> SMT::genBitCodes(const SLList<Analysis::Path>& paths, const AVLMap<const Edge*, unsigned int>& map_pathpoint_to_bit, unsigned int used_bits)
{
	BitVector bitcode(used_bits);
	Vector<BitVector> bitcode_vector;
	int index = 0;
	for(SLList<Analysis::Path>::Iterator iter(paths); iter; iter++)
	{
		bitcode.clear();
		SLList<const Edge*> p = *iter;
		for(SLList<const Edge*>::Iterator subiter(p); subiter; subiter++)
			bitcode.set(*map_pathpoint_to_bit.get(*subiter));
			// bitcode |= 1u << *map_pathpoint_to_bit.get(*subiter);
		bitcode_vector.push(bitcode);
	}
	return bitcode_vector;
}

BitVector SMT::getListOfPathsToKeep(const Vector<BitVector>& bitcode_vector)
{
	const int number_of_paths = bitcode_vector.length();
	BitVector paths_to_keep(number_of_paths, true); // a priori, keep all paths
	for(int i = 0; i < number_of_paths; i++)
	{
		if(!paths_to_keep[i])
			continue;
		for(int j = i+1; j < number_of_paths; j++)
		{
			if(!paths_to_keep[j])
				continue;
			const BitVector &opd1 = bitcode_vector[i], &opd2 = bitcode_vector[j];
			// DBG("\t#" << i << "(" << opd1 << ") <= #" << j << "(" << opd2 << ") : " << DBG_TEST(opd1 <= opd2, false))
			if(opd1 <= opd2)
			{	// opd2 is superfluous
				paths_to_keep.set(j, false);
				continue;
			}
			// DBG("\t#" << i << "(" << opd1 << ") >= #" << j << "(" << opd2 << ") : " << DBG_TEST(opd1 >= opd2, false))
			if(opd1 >= opd2)
			{	// opd1 is superfluous
				paths_to_keep.set(i, false);
				break;
			}
		}
	}
	return paths_to_keep;
}

SLList<Analysis::Path> SMT::filterPaths(const Vector<BitVector>& bitcode_vector, const Vector<const Edge*>& map_bit_to_pathpoint, const BitVector& paths_to_keep, bool print_results)
{
	SLList<Analysis::Path> filtered_paths;
	int index = -1;
	for(Vector<BitVector>::Iterator iter(bitcode_vector); iter; iter++)
	{
		index++;
		if(!paths_to_keep[index])
			continue;
		Analysis::Path path; // this is a SLList
		elm::String str = "[";
		bool first = true;
		for(BitVector::OneIterator oneiter(bitcode_vector[index]); oneiter; oneiter++)
		{
			const Edge* e = map_bit_to_pathpoint[*oneiter];
			if(first)
				first = false;
			else
				str = str.concat(_ << ", ");
			str = str.concat(_ << e->source()->number() << "->" << e->target()->number());
			path.addLast(e); // preserving order for aesthetic purposes
		}
		str = str.concat(_ << "]");
		if(print_results)
			DBG(COLOR_IRed "[" COLOR_RCol "Inf. path found: " << str << " (bitcode=" << bitcode_vector[index] << ")" COLOR_IRed "]")
		filtered_paths += path;
	}
	return filtered_paths;
}

// check predicates satisfiability
bool SMT::checkPredSat(const SLList<Analysis::LabelledPredicate>& labelled_preds, bool print_results)
{
	smt.push();
	for(SLList<Analysis::LabelledPredicate>::Iterator iter(labelled_preds); iter; iter++)
	{
		Predicate pred = (*iter).pred();
		if(Option<Expr> expr = getExpr(pred))
		{
			// DBG_STD(COLOR_Red << "Assumption: " << *expr)
			smt.assertFormula(*expr);
		}
	}
		
	CVC4::Result result = smt.checkSat(em.mkConst(true)); // check satisfability
	smt.pop();
	if(print_results)
		DBG(COLOR_BIWhi << "SMT call:" << (result.isSat() ? COLOR_BGre : COLOR_BIRed) << (result.isSat() ? " SAT" : " UNSAT"))
	return result.isSat();
}

Option<Expr> SMT::getExpr(const Predicate& p)
{
	if(!p.isComplete())
		return elm::none;
	return em.mkExpr(getKind(p), getExpr(p.leftOperand()), getExpr(p.rightOperand()));
}

Option<Expr> SMT::getExpr(const Operand& o)
{
	if(!o.isComplete())
		return elm::none;
		
	SMTOperandVisitor visitor(em, variables);
	o.accept(visitor);
	return elm::some(visitor.result());
}

Kind_t SMT::getKind(condoperator_t opr)
{
	switch(opr)
	{
		case CONDOPR_LT:
			return LT;
		case CONDOPR_LE:
			return LEQ;
		case CONDOPR_EQ:
			return EQUAL;
		case CONDOPR_NE:
			return DISTINCT;
		default:
			assert(false);
	}
}
 