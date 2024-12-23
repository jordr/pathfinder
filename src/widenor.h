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
 
#ifndef _WIDENOR_H
#define _WIDENOR_H

#include "analysis_state.h"
#include "struct/operand.h"

// rolls back an iter
class RollerBack : public OperandEndoVisitor
{
public:
	RollerBack(DAG& dag, const Operand* opd_iter) : dag(dag), opdi(opd_iter) { }
	const Operand* visit(const class OperandConst& g) { return &g; }
	const Operand* visit(const class OperandVar& g) { return &g; }
	const Operand* visit(const class OperandMem& g) { return &g; }
	const Operand* visit(const class OperandTop& g)  { return &g; }
	const Operand* visit(const class OperandIter& g) { return Arith::add(dag, Constant(-1), static_cast<const Operand*>(&g)); }
	const Operand* visit(const class OperandArith& g)
		{ return Arith::autoOp(dag, g.opr(), g.leftOperand().accept(*this), g.isBinary() ? g.rightOperand().accept(*this) : NULL); }
private:
	DAG& dag;
	const Operand* opdi;
};

// Widenor: a class for State composition
// this assumes all variables & memory involved are already general expr
class Widenor : public OperandEndoVisitor
{
	typedef elm::HashTable<Constant, const Operand*, ConstantHash> mem_t;
public:
	// self is the operand that should not be replaced, n is the OperandIter
	Widenor(const Analysis::State& s, const Operand* self, const Operand* n)
		: dag(s.getDag()), lvars(s.getLocalVariables()), mem(s.getMemoryTable()), self(self), rollback(dag, n) { }
	void setSelf(const Operand* x) { self = x; }

	const Operand* visit(const class OperandConst& g) { return &g; }
	const Operand* visit(const class OperandVar& g) { 
		if(g == *self)
			return &g;
		return lvars[g] ? lvars[g]->accept(rollback) : dag.var(g); // we need to (smartly) replace I by I-1
	}
	const Operand* visit(const class OperandMem& g)  { 
		if(g == *self)
			return &g;
		return (*mem[g.addr()])->accept(rollback); // we need to (smartly) replace I by I-1
	}
	const Operand* visit(const class OperandTop& g)  { return &g; }
	const Operand* visit(const class OperandIter& g) { return &g; }
	const Operand* visit(const class OperandArith& g)
		{ return Arith::autoOp(dag, g.opr(), g.leftOperand().accept(*this), g.isBinary() ? g.rightOperand().accept(*this) : NULL); }

private:
	DAG& dag;
	const LocalVariables &lvars;
	const mem_t &mem;
	// WARNING: this self doesn't come from the DAG, so do not compare pointers! We just need to be able to change it and references don't do that...
	const Operand* self; // do not replace this one
	RollerBack rollback;
};

#endif
