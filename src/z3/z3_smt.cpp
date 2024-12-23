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
 * Implementing z3 as the SMT solver
 */
#include <elm/data/List.h>
#include "z3_operand_visitor.h"
#include "../debug.h"
#include "z3_smt.h"

Z3SMT::Z3SMT(int flags): SMT(flags), s(c), p(c), sp(c.int_const("SP"))
{
	p.set("unsat_core", true);
	s.set(p);
}

void Z3SMT::initialize(const elm::List<LabelledPredicate>& labelled_preds)
{
	for(elm::List<LabelledPredicate>::Iter iter(labelled_preds); iter; iter++)
	{
		const Predicate& p = iter->pred();
		if(p.isComplete())
		{
			const z3::expr& e = getExpr(p);
			z3::expr label = c.bool_const(Analysis::pathToString(iter->labels()).chars());
			labels_hash.push(label.hash());
			s.add(e, label);
		}
	}	
}

// check predicates satisfiability
bool Z3SMT::checkPredSat()
{
	return s.check();
}

// get unsat core and build a shortened path accordingly
// rtn false if failure, true otherwise
bool Z3SMT::retrieveUnsatCore(Analysis::Path& path, const elm::List<LabelledPredicate>& labelled_preds, std::basic_string<char>& unsat_core_output)
{
	z3::expr_vector core = s.unsat_core();
	bool empty = true;
	unsat_core_output = "[";
	for(unsigned int i = 0; i < core.size(); i++)
	{
		if(!empty)
			unsat_core_output += ", ";
		unsat_core_output += Z3_ast_to_string(c, core[i]);
		empty = false;

		Vector<unsigned int>::Iter labels_hash_iter(labels_hash);
		for(elm::List<LabelledPredicate>::Iter lp_iter(labelled_preds); lp_iter; lp_iter++)
		{	
			if(!lp_iter->pred().isComplete())
				continue;
			assert(labels_hash_iter);
			if(*labels_hash_iter == core[i].hash())
				path += lp_iter->labels();
			labels_hash_iter++;
		}
	}
	unsat_core_output += "]";
	return !empty;
}

z3::expr Z3SMT::getExpr(const Predicate& p)
{
	switch(p.opr())
	{
		case CONDOPR_LT:
			return getExpr(p.leftOperand()) < getExpr(p.rightOperand());
		case CONDOPR_LE:
			return getExpr(p.leftOperand()) <= getExpr(p.rightOperand());
		case CONDOPR_EQ:
			return getExpr(p.leftOperand()) == getExpr(p.rightOperand());
		case CONDOPR_NE:
			return getExpr(p.leftOperand()) != getExpr(p.rightOperand());
		default:
			assert(false);
	}
}

z3::expr Z3SMT::getExpr(const Operand& o)
{
	assert(o.isComplete()); // should have been checked for earlier
	Z3OperandVisitor visitor(c, sp);
	assert(o.accept(visitor)); // invalid operands are not tolerated at this point
	return visitor.result();
}

/*  ---sample---
	context c;
	expr x  = c.int_const("x");
	expr y  = c.int_const("y");
	solver s(c);

	// enabling unsat core tracking
	params p(c);
	p.set("unsat_core", true);
	s.set(p);

	// The following assertion will not be tracked.
	s.add(x > 0);

	// The following assertion will be tracked using Boolean variable p1.
	// The C++ wrapper will automatically create the Boolean variable.
	s.add(y > 0, "p1");

	// Asserting other tracked assertions.
	s.add(x < 10, "p2");
	s.add(y < 0,  "p3");

	std::cout << s.check() << "\n";
	std::cout << s.unsat_core() << "\n";
*/
