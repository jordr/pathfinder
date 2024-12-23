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
 * DAG.cpp
 *  Created on: 15 avr. 2016
 *      Author: casse
 */

#include "../debug.h"
#include "DAG.h"

using namespace elm::color;

DAG::~DAG(void)
{
	for(int i = 0; i < tmp_cnt + var_cnt; i++)
		delete vars[i];
	delete [] vars;
	for(cst_map_t::Iter cst(cst_map); cst; cst++)
		delete *cst;
	for(op_map_t::Iter op(op_map); op; op++)
		delete *op;
	for(pred_map_t::Iter pred(pred_map); pred; pred++)
		delete *pred;
}

/**
 * @brief      Makes a variable operand pointer with the given variable index
 * @param      v      The index of the variable (register or temporary)
 * @return     A pointer to an OperandVar object
 */
const Operand *DAG::var(int v)
{
	ASSERT(0 <= v + tmp_cnt && v + tmp_cnt < tmp_cnt + var_cnt);
	if(!vars[v + tmp_cnt])
		vars[v + tmp_cnt] = new OperandVar(v);
	return vars[v + tmp_cnt];
}
/**
 * @brief      Makes a constant operand pointer with the given constant
 * @param      cst   The constant
 * @return     A pointer to an OperandConst object
 */
const Operand *DAG::cst(const Constant& cst)
{
	Operand *r = cst_map.get(cst, 0);
	if(!r)
	{
		r = new OperandConst(cst);
		cst_map.put(cst, r);
	}
	return r;
}
/**
 * @brief      Makes a memory operand pointer with the given address
 * @param      addr  The address of the OperandMem to create
 * @return     A pointer to an OperandMem object
 */
const Operand *DAG::mem(const OperandConst *addr)
{
	// ASSERT(addr);
	Key k(ARITHOPR_MEM, addr);
	Operand *r = op_map.get(k, 0);
	if(!r)
	{
		r = new OperandMem(*addr);
		op_map.put(k, r);
	}
	return r;
}
/**
 * @brief      Returns the operand pointer in DAG matching the given operand
 * @param      opd   The operand
 */
const Operand* DAG::get(const Operand& opd)
{
	switch(opd.kind())
	{
		case CST: return cst(opd.toConst());
		case VAR: return var(opd.toVar());
		case MEM: return mem(opd.toMem());
		case ARITH: return autoOp(opd.toArith().opr(), opd.toArith().left(), opd.toArith().isBinary() ? opd.toArith().right() : NULL);
		default: crash();
	}
}

const Operand *DAG::op(arithoperator_t op, const Operand *arg)
{	
	Key k(op, arg);
	Operand *r = op_map.get(k, 0);
	if(!r)
	{
		r = new OperandArith(op, arg);
		op_map.put(k, r);
	}
	return r;

}
const Operand *DAG::op(arithoperator_t op, const Operand *arg1, const Operand *arg2)
{
	// ASSERTP(arg1 && arg2, arg1 << arg2);
	Key k(op, arg1, arg2);
	Operand *r = op_map.get(k, 0);
	if(!r)
	{
		// DBG(color::IBlu() << "k=" << *k.argument1() << (arithoperator_t)k.operation() << *k.argument2() << " not in " << *this)
		r = new OperandArith(op, arg1, arg2);
		op_map.put(k, r);
	}
	return r;
}

const Operand *DAG::comOp(arithoperator_t _op, const Operand *arg1, const Operand *arg2)
{
	if(*arg2 < *arg1)
		return op(_op, arg2, arg1);
	else
		return op(_op, arg1, arg2);
}

// accepts non-binary calls with NULL second argument
const Operand *DAG::autoOp(arithoperator_t op, const Operand *arg1, const Operand *arg2)
{
	// ASSERTP(arg1 && arg2, arg1 << arg2);
	switch(op)
	{
		case ARITHOPR_ADD: return add(arg1, arg2);
		case ARITHOPR_SUB: return sub(arg1, arg2);
		case ARITHOPR_MUL: return mul(arg1, arg2);
		case ARITHOPR_MULH: return mulh(arg1, arg2);
		case ARITHOPR_DIV: return div(arg1, arg2);
		case ARITHOPR_MOD: return mod(arg1, arg2);
		case ARITHOPR_CMP: return cmp(arg1, arg2);
		case ARITHOPR_NEG: return neg(arg1);
		default: elm::crash();
	}
}

const Operand *DAG::autoOp(arithoperator_t op, const Operand *arg1)
{
	switch(op)
	{
		case ARITHOPR_NEG: return neg(arg1);
		default: elm::crash();
	}
}

/**
 * @brief      For debugging purposes, prints the contents of the DAG
 */
io::Output& DAG::print(io::Output& out) const
{
	bool first = true;
	out << "DAG\n\t-> cst:  ";
	for(cst_map_t::PairIterator i(cst_map); i; i++, first = false)
		out << (first?"":",  ") << (*i).fst;
		// for(int i = 0; i < int(sizeof(Constant)); i++)
		// 	out << io::hex(((unsigned char *)&c)[i]).pad('0').right().width(2) << " ";
	first = true;
	out << "\n\t-> op:   ";
	for(op_map_t::PairIterator i(op_map); i; i++, first = false)
		out << (first?"":",  ") << *(*i).snd;
	first = true;
	out << "\n\t-> pred: ";
	for(pred_map_t::PairIterator i(pred_map); i; i++, first = false)
		out << (first?"":",  ") << *(*i).snd;
	return out;
}

// ----------------- test stuff -----------------

void testSimplify(DAG& d)
{
	const Operand* zero  = d.cst(0);
	const Operand* one   = d.cst(1);
	const Operand* two   = d.cst(2);
	const Operand* three = d.cst(3);
	DBG("zero: "  << *zero)
	DBG("one: "   << *one)
	DBG("two: "   << *two)
	DBG("three: " << *three)

	const Operand* four =  d.cst(4);four=four;
	const Operand* t1 = d.var(-1);

	DBG("making neg(t1)")
	const Operand* e11 = d.neg(t1);
	DBG("neg(t1): " << *e11)
	const Operand* e1 = d.add(t1, e11);
	const Operand* f = d.sub(two, three);f=f;
	const Operand* fv2 = d.sub(two, three);fv2=fv2;

	DBG("e1: "    << *e1)
	DBG("f=fv2: " << DBG_TEST(f==fv2, true) << "  (" << *f << "==" << *fv2 << ")")
	// DBG("one+two: " << *d.add(one, two))
	// DBG("one+two simplified: " << **d.op(ARITHOPR_ADD, one, two)->simplify())
	// DBG("one+two simplified equal three: " << DBG_TEST(three == *d.op(ARITHOPR_ADD, one, two)->simplify(), true))
}

void testPredicates(DAG& d)
{
	const Operand* oprconst = d.cst(2);
	const Operand* oprvar = d.mem(0x4000);
	const Operand* e1 = d.mul(oprconst, oprvar); // e1 := 2 * @0x4000
	const Predicate* p1 = d.pred(CONDOPR_EQ, oprvar, e1); // p1 := @0x4000 = e1
	const Predicate* p1bis = d.eq(oprvar, e1); // p1 := @0x4000 = e1
	const Predicate* p2 = d.pred(CONDOPR_LE, oprconst, oprvar); // p2 := 2 <= @0x4000
	
	DBG("--- Equality over predicates ---")
	DBG("p1 = p1':\t"<< DBG_TEST(p1 == p1bis, true))
	DBG("p2 = p2:\t" << DBG_TEST(p2 == p2, true))
	DBG("p1 = p2:\t" << DBG_TEST(p1 == p2, false) << io::endl)
}

void testOperands(DAG& d)
{
	DBG("--- Equality over Operands  ---")
	const Operand* o1 = d.cst(12);
	const Operand* o1bis = d.cst(12);
	const Operand* o2 = d.cst(16);
	const Operand* o3 = d.mem(0x1004);
	const Operand* o4 = d.mem(0x1008);
	DBG("diag: " << d)
	DBG("o1 = o1:\t" << DBG_TEST(o1 == o1, true) 	<< " \t" << *o1 << "==" << *o1)
	DBG("o1 = o1':\t"<< DBG_TEST(o1 == o1bis, true) << " \t" << *o1 << "==" << *o1bis)
	DBG("o1 = o2:\t" << DBG_TEST(o1 == o2, false) 	<< " \t" << *o1 << "==" << *o2)
	DBG("o1 = o3:\t" << DBG_TEST(o1 == o3, false) 	<< " \t" << *o1 << "==" << *o3)
	DBG("o3 = o1:\t" << DBG_TEST(o3 == o1, false) 	<< " \t" << *o3 << "==" << *o1)
	DBG("o3 = o3:\t" << DBG_TEST(o3 == o3, true) 	<< " \t" << *o3 << "==" << *o3)
	DBG("o3 = o4:\t" << DBG_TEST(o3 == o4, false) 	<< " \t" << *o3 << "==" << *o4 << io::endl)
	
	const Operand* oae = d.mul(o1, o2);
	const Operand* oaebis = d.mul(o1, o2);
	DBG("o1*o2:\t" << *oae)
	const Operand* oae2 = d.mul(o2, o2);
	const Operand* reverse_oae = d.mul(o2, o1);
	const Operand* oae4 = d.add(o1, o2);
	DBG("oae = oae:\t"  << DBG_TEST(oae == oae, true))
	DBG("oae = oae':\t"  << DBG_TEST(oae == oaebis, true))
	DBG("oae = oae2:\t" << DBG_TEST(oae == oae2, false))
	DBG("oae = roae:\t" << DBG_TEST(oae == reverse_oae, true))
	DBG("oae = oae4:\t" << DBG_TEST(oae == oae4, false))
}
/*
int _main(void)
{
	elm::log::Debug::setSourceInfoFlag(false);
	DAG dag(16,16);
	testOperands(dag);
	testPredicates(dag);
	testSimplify(dag);
	DBG("diag: " << dag)
	DBG("==================================\n")
	Predicate *p = dag.eq(dag.sub(dag.neg(dag.add(dag.cst(2),dag.cst(2))), dag.sub(dag.mul(dag.cst(2),dag.cst(3)), dag.add(dag.mul(dag.cst(2),dag.cst(47)),dag.mul(dag.cst(2),dag.cst(13))))),
		   dag.add(dag.cst(13),dag.cst(47)));
	DBG("diag: " << dag)
	DBG("Original predicate: -(2+2) - (2*3 - (2*47 + 2*13)) = 13+47")
	DBG("Predicate: " << *p)
	return 0;
}
*/