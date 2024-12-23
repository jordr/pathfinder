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
 
#ifndef _COMPOSITOR_H
#define _COMPOSITOR_H

#include <elm/data/HashTable.h>
#include "analysis_state.h"
// #include "strarith.h"
#include "struct/DAG.h"
#include "struct/local_variables.h"
#include "struct/operand.h"
#include "struct/predicate.h"

// Compositor: a class for State composition
class Compositor : public OperandEndoVisitor
{
	typedef elm::HashTable<Constant, const Operand*, ConstantHash> mem_t;
public:
	Compositor(const Analysis::State& s, bool sp_is_local)
		: dag(s.getDag()), sp(NULL), lvars(s.getLocalVariables()), mem(s.getMemoryTable())
	{
		if(sp_is_local)
			sp = lvars[s.getSP()] ? lvars[s.getSP()] : dag.var(s.getSP());
		else
			sp = dag.cst(SP);
	}
	const Operand* visit(const class OperandConst& g) {
		ASSERT(g.value().isValid());
		if(g.value().isAbsolute())
			return &g;
		else if(g.value().isRelativePositive())
			return dag.smart_add(sp, dag.cst(g.value().val()));
		else
			return dag.smart_sub(dag.cst(g.value().val()), sp);
	}
	const Operand* visit(const class OperandVar& g)  { return lvars[g] ? lvars[g] : &g; }
	const Operand* visit(const class OperandMem& g)  { return mem.exists(g.addr()) ? mem[g.addr()] : &g; }
	const Operand* visit(const class OperandTop& g)  { return &g; } // this will be handled elsewhere by allocating a new top
	const Operand* visit(const class OperandIter& g) { return &g; /*ASSERTP(false, "OperandIter found by the Compositor");*/ }
	const Operand* visit(const class OperandArith& g)
		{ return dag.smart_autoOp(g.opr(), g.leftOperand().accept(*this), g.isBinary() ? g.rightOperand().accept(*this) : NULL); }
	// inline Predicate visit(const Predicate& p) { return Predicate(p.opr(), p.left()->accept(*this), p.right()->accept(*this)); }

private:
	DAG &dag;
	const Operand* sp;
	const LocalVariables &lvars;
	const mem_t &mem;
};

#endif
