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
 * Implementing CVC4 as the SMT solver
 */
#ifndef _CVC4_CVC4_SMT_H
#define _CVC4_CVC4_SMT_H

#include <cvc4/expr/expr_manager.h>
#include <cvc4/smt/smt_engine.h>
#include "../analysis.h" // Analyis::Path
#include "../smt.h" // abstract SMT class inheritance
#include "cvc4_operand_visitor.h"
#include "cvc4_variable_stack.h"

using namespace CVC4::kind;
using CVC4::Expr;

class CVC4SMT : public SMT
{
public:
	CVC4SMT(int flags);
	static inline elm::String name() { return "cvc4"; }
	
private:
	CVC4::ExprManager em;
	CVC4::SmtEngine smt;
	CVC4VariableStack variables;
	elm::List<Option<Expr> > exprs;

	// SMT virtual pure methods
	void initialize(const elm::List<LabelledPredicate>& labelled_preds);
	void initialize(const LocalVariables& lv, const elm::HashTable<Constant, const Operand*, ConstantHash>& mem, DAG& dag);
	bool checkPredSat();
	bool retrieveUnsatCore(Analysis::Path& path, const elm::List<LabelledPredicate>& labelled_preds, std::basic_string<char>& unsat_core_output);
	// bool checkPredSat(const elm::List<LabelledPredicate>& labelled_preds);
	Option<Expr> getExpr(const Predicate& p);
	Option<Expr> getExpr(const Operand& o);
	inline Expr getRegExpr(t::int32 reg_id)
		{ return variables.getExpr(em, OperandVar(reg_id), VARIABLE_PREFIX); }
	inline Expr getMemExpr(Constant mem_id)
		{ return variables.getExpr(em, OperandMem(mem_id), VARIABLE_PREFIX); }
	Kind_t getKind(const Predicate& p) const;

};

#endif
