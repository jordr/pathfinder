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
 * Implementing CVC4 as the SMT solver
 */
#include <cvc4/expr/command.h> // getUnsatCoreCommand
#include <cvc4/util/unsat_core.h>
#include <elm/util/BitVector.h>
#include "../struct/operand.h"
#include "../debug.h"
#include "cvc4_smt.h"

#define DONT_REMOVE_USELESS_ASSERTS

using namespace CVC4::kind;
using elm::BitVector;
using CVC4::Expr;

CVC4SMT::CVC4SMT(int flags): SMT(flags), smt(&em), variables(em)
{
	smt.setLogic("QF_LIA"); // Quantifier-Free (no forall, exists...) Linear Integer Arithmetic
	smt.setOption("incremental", CVC4::SExpr("false")); // non incremental
	smt.setOption("produce-unsat-cores", CVC4::SExpr("true"));
	smt.setOption("rewrite-divk", CVC4::SExpr("true"));
	// smt.setOption("dump-unsat-cores", CVC4::SExpr("true"));
	// smt.setOption("produce-proofs", CVC4::SExpr("true"));
	static int nyu = 0;
	if(dbg_&0x10)
	{
		if(!nyu)
		{
			nyu++;
			smt.setOption("dump", "assertions:pre-everything");
			smt.setOption("dump-to", "dump.log"); // this is actually global to CVC4... meaning setting it once per pathfinder execution is enough
		}
	}
}

// v1: all INITIAL_PREFIX "rk"
// v2: INITIAL: "rk", VARIABLE: "?k". Predicates like "?0 = r0 + 1"
void CVC4SMT::initialize(const elm::List<LabelledPredicate>& labelled_preds)//, mode_t mode = VARIABLE_PREFIX)
{
	variables.setMode(INITIAL_PREFIX);
	for(elm::List<LabelledPredicate>::Iter iter(labelled_preds); iter; iter++)
		exprs.addLast(getExpr(iter->pred()));
}

// static int saved_useless_asserts = 0;
void CVC4SMT::initialize(const LocalVariables& lv, const elm::HashTable<Constant, const Operand*, ConstantHash>& mem, DAG& dag)
{
	variables.setMode(INITIAL_PREFIX);
	BitVector used_regs(lv.maxRegisters(), false);

// experimental
#ifndef DONT_REMOVE_USELESS_ASSERTS // this improves the complexity of the SMT solving by removing asserts that have no chance of creating UNSAT
	Vector<const Operand*> v;
	for(HashTable<Constant, const Operand*, ConstantHash>::KeyIterator iter(mem); iter; iter++)
		v.push(dag.mem(*iter));
	const int key_nb = v.count();
	for(HashTable<Constant, const Operand*, ConstantHash>::Iter iter(mem); iter; iter++)
		for(Vector<const Operand*>::Iter vi(v); vi; )
		{
			if((*iter)->involvesOperand(**vi))
				v.remove(vi); // not useless
			else
				vi++;
		}
	for(int i = 0; i < lv.maxRegisters(); i++) // registers id are [0...n[
		for(Vector<const Operand*>::Iter vi(v); vi; )
		{
 			if(lv(i).involvesOperand(**vi))
				v.remove(vi); // not useless
			else
				vi++;
		}
	if(key_nb != v.count())
		cout << "Removed " << key_nb-v.count() << " useless asserts.";
	// cout << v.count() << "/" << key_nb << " useless" << endl;
	// saved_useless_asserts += v.count();
#endif

	for(elm::HashTable<Constant, const Operand*, ConstantHash>::PairIterator iter(mem); iter; iter++)
	{
#ifndef DONT_REMOVE_USELESS_ASSERTS
		if(v.contains(dag.mem((*iter).fst))) // useless assert
		{
			exprs += elm::none;
			continue;
		}
#endif
		// exprs += getExpr(Predicate(CONDOPR_EQ, dag.mem((*iter).fst), (*iter).snd));
		if(Option<Expr> e = getExpr(*(*iter).snd))
 			exprs += em.mkExpr(EQUAL, getMemExpr((*iter).fst), e); // TODO!! I forgot why... I think cuz [SP+8] = [SP+8] + 1. need example
		(*iter).snd->markUsedRegisters(used_regs);
	}
	for(int i = 0; i < lv.maxRegisters(); i++) // registers id are [0...n[
	{
 		if(used_regs[i] || lv[i])
 			if(Option<Expr> expr_right = getExpr(lv(i)))
 				exprs += em.mkExpr(EQUAL, getRegExpr(i), expr_right);
	}
}

// check predicates satisfiability
bool CVC4SMT::checkPredSat()
{
	try {
		// std::time_t timestamp = clock(); // Timestamp before analysis
		for(elm::List<Option<Expr> >::Iter iter(exprs); iter; iter++)
			if(*iter) {
				smt.assertFormula(**iter, true); // second parameter to true for unsat cores
				// std::cout << **iter << endl; // uncomment to print all asserted predicates
			}
		bool isSat = smt.checkSat(em.mkConst(true), true).isSat(); // check satisfability, the second parameter enables unsat cores
		
		if(!isSat) {
			if(dbg_&0x1)
				std::cout << "[[[" << smt.getUnsatCore() << "]]]" << endl;
			if(dbg_&0x2)
				{ char c; cin >> c >> c; }
			if(dbg_&0x4)
				abort();
		}

		// timestamp = (clock()-timestamp)*1000*1000/CLOCKS_PER_SEC;
		// smt.getStatistics().flushInformation((std::ostream&)std::cout);
		/*
		std::cerr << "DEBUG:\t" <<
			exprs.count() << "\t" <<
			smt.getStatistic("smt::SmtEngine::processAssertionsTime").getValue() << "\t" <<
			smt.getStatistic("smt::SmtEngine::theoryPreprocessTime").getValue() << "\t" <<
			smt.getStatistic("smt::SmtEngine::solveTime").getValue() << "\t" <<
			timestamp <<
		endl;
		*/
		return isSat;
	}
	catch(CVC4::LogicException e)
	{
#ifdef DBG_WARNINGS
		DBGW("non-linear call to CVC4, defaulting to SAT:")
		std::cerr << e;
#endif
		return true;
	}
}

// get unsat core and build a shortened path accordingly
// rtn false if failure, true otherwise
bool CVC4SMT::retrieveUnsatCore(Analysis::Path& path, const elm::List<LabelledPredicate>& labelled_preds, std::basic_string<char>& unsat_core_output)
{
	CVC4::UnsatCore unsat_core = smt.getUnsatCore(); // get an unsat subset of our assumptions
	bool empty = true;
	unsat_core_output = "[";
	for(CVC4::UnsatCore::const_iterator unsat_core_iter = unsat_core.begin(); unsat_core_iter != unsat_core.end(); unsat_core_iter++)
	{
		if(!empty)
			unsat_core_output += ", ";	
		unsat_core_output += (*unsat_core_iter).toString();
		empty = false;

		elm::List<LabelledPredicate>::Iter lp_iter(labelled_preds);
		elm::List<Option<Expr> >::Iter expr_iter(exprs);
		for(; lp_iter; lp_iter++, expr_iter++)
			if(*expr_iter && **expr_iter == *unsat_core_iter)
				path += (*lp_iter).labels();
	}
	unsat_core_output += "]";
	return !empty;
}

Option<Expr> CVC4SMT::getExpr(const Predicate& p)
{
	if(!p.isComplete() || (flags&Analysis::SMT_CHECK_LINEAR && !p.isLinear(!(flags&Analysis::ALLOW_NONLINEAR_OPRS))))
		return elm::none;
	return em.mkExpr(getKind(p), *getExpr(p.leftOperand()), *getExpr(p.rightOperand()));
}

Option<Expr> CVC4SMT::getExpr(const Operand& o)
{
	if(flags&Analysis::SMT_CHECK_LINEAR && !o.isLinear(!(flags&Analysis::ALLOW_NONLINEAR_OPRS)))
		return elm::none;
	// cout << "\e[0;92m" << (const string)(_ << o) << "\e[0;m" << " (" << o.isLinear() << ")" << endl;
	CVC4OperandVisitor visitor(em, variables);
	if(!o.accept(visitor))
		return elm::none;
	return elm::some(visitor.result());
}

Kind_t CVC4SMT::getKind(const Predicate& p) const
{
	switch(p.opr())
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
			abort();
	}
}
