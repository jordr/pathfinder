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
 
#ifndef DAG_H
#define DAG_H

#include <elm/array.h>
#include <elm/types.h>
#include <elm/data/HashTable.h>
#include <elm/data/quicksort.h>
#include <elm/io/Output.h>
#include <otawa/program.h>
#include "arith.h"
#include "predicate.h"

// using namespace elm;
using namespace otawa;

// Comment / un-comment this to enable sum canonicalization (sometimes maybe time expensive)
// #define	DAG_SUM

class DAG {

	class Key {
	public:
		inline Key(char operation = -1, const Operand *argument1 = 0, const Operand *argument2 = 0)
			: op(operation), arg1(argument1), arg2(argument2) { }
		inline char operation(void) const { return op; }
		inline const Operand *argument1(void) const { return arg1; }
		inline const Operand *argument2(void) const { return arg2; }

 		inline bool operator==(const Key& k) const { return op == k.op && arg1 == k.arg1 && arg2 == k.arg2; }
 		inline bool operator!=(const Key& k) const { return !operator==(k); }
 		inline t::hash hash(void) const { return Hasher() << op << arg1 << arg2; }
	private:
		char op; // save space
		const Operand *arg1, *arg2;
	};

	class Pred {
	public:
		Pred(void): op(CONDOPR_NE), arg1(0), arg2(0) { }
		Pred(condoperator_t operation, const Operand *argument1, const Operand *argument2)
			: op(operation), arg1(argument1), arg2(argument2) { }
		inline condoperator_t operation(void) const { return condoperator_t(op); }
		inline const Operand *argument1(void) const { return arg1; }
		inline const Operand *argument2(void) const { return arg2; }

		inline bool operator==(const Pred& k) const { return op == k.op && arg1 == k.arg1 && arg2 == k.arg2; }
		inline bool operator!=(const Pred& k) const { return !operator==(k); }
		inline t::hash hash(void) const { return Hasher() << op << arg1 << arg2; }

	private:
		char op; // save space
		const Operand *arg1, *arg2;
	};

	enum
	{
		ARITHOPR_MEM = ARITHOPR_CMP + 1, // Key.argument1 = OperandConst*
		ARITHOPR_TOP = ARITHOPR_MEM + 1, // Key.argument2 = OperandTop*
	};

	class Compare {
	public:
		static int compare(const Pair<bool, const Operand *>& o1, const Pair<bool, const Operand *>& o2) {
			if(o1.fst < o2.fst) // j
				return -1;
			else if(o1.fst > o2.fst)
				return +1;
			// same sign
			else if(*o1.snd == *o2.snd)
				return 0;
			else if(*o1.snd < *o2.snd)
				return -1;
			else // *o1.snd > *o2.snd
				return +1;
		}
	};
 
 	typedef elm::HashTable<Key,      Operand *,   SelfHashKey<Key> >  op_map_t;
 	typedef elm::HashTable<Pred,     Predicate *, SelfHashKey<Pred> > pred_map_t;
 	typedef elm::HashTable<Constant, Operand *,   ConstantHash>       cst_map_t;

#	ifdef DAG_SUM
	class Sum: public OperandVisitor {
	public:
		Sum(DAG& dag): _dag(dag), _neg(false) { }
		virtual bool visit(const OperandConst& o) 	{ args.add(pair(_neg, static_cast<const Operand *>(&o))); return true; }
		virtual bool visit(const OperandVar& o) 	{ args.add(pair(_neg, static_cast<const Operand *>(&o))); return true; }
		virtual bool visit(const OperandMem& o)		{ args.add(pair(_neg, static_cast<const Operand *>(&o))); return true; }
		virtual bool visit(const OperandTop& o)		{ args.add(pair(_neg, static_cast<const Operand *>(&o))); return true; }

		virtual bool visit(const OperandArith& o) {
			bool save = _neg;
			if(o.opr() == ARITHOPR_ADD) {
				o.leftOperand().accept(*this);
				_neg = save;
				o.rightOperand().accept(*this);
				_neg = save;
			}
			else if(o.opr() == ARITHOPR_SUB) {
				o.leftOperand().accept(*this);
				_neg = !save;
				o.rightOperand().accept(*this);
				_neg = save;
			}
			else if(o.opr() == ARITHOPR_NEG) {
				_neg = !_neg;
				o.leftOperand().accept(*this);
				_neg = save;
			}
			else
				args.add(pair(_neg, static_cast<const Operand *>(&o)));
			return true;
		}

		const Operand *make(arithoperator_t op, const Operand *arg1, const Operand *arg2 = NULL) {
			// only for ADD/SUB/NEG
			// record and sort the expressions of the sum
			switch(op) {
			case ARITHOPR_NEG:
				_neg = true;
				arg1->accept(*this);
				break;
			case ARITHOPR_ADD:
				_neg = false;
				arg1->accept(*this);
				arg2->accept(*this);
				break;
			case ARITHOPR_SUB:
				_neg = false;
				arg1->accept(*this);
				_neg = true;
				arg2->accept(*this);
				break;
			default:
				crash();
				break;
			}
			genstruct::quicksort<Pair<bool, const Operand *>, Vector, Compare>(args);

			// rebuild the expression
			int i = args.count() - 2;
			bool neg = args[i + 1].fst;
			const Operand *res = args[i + 1].snd;
			while(i >= 0) {
				/*if(args[i].fst == neg)
					res = _dag.add(args[i].snd, res);
				else
					res = _dag.sub(args[i].snd, res);*/ // infinite loop, this calls make
				res = _dag.op(args[i].fst == neg ? ARITHOPR_ADD : ARITHOPR_SUB, args[i].snd, res);
				neg = args[i].fst;
				i--;
			}
			if(neg)
				res = _dag.op(ARITHOPR_NEG, res);
			args.clear();
			return res;
		}

	private:
		DAG& _dag;
		bool _neg;
		Vector<Pair<bool, const Operand *> > args;
	}; // Sum class
#	endif

public:
	DAG(int max_tempvars, int max_registers) : tmp_cnt(max_tempvars), var_cnt(max_registers)
#		ifdef DAG_SUM
			, sum(*this)
#		endif
	{
		vars = new Operand *[tmp_cnt + var_cnt];
		array::set(vars, tmp_cnt + var_cnt, null<Operand>());
	}

	~DAG(void);

	const Operand *var(int v);
	inline const Operand *var(const OperandVar& opdv)
		{ return var(opdv.addr()); }
	const Operand *cst(const Constant& cst);
	const Operand *mem(const OperandConst *addr);
	inline const Operand *mem(const Constant& cst)
		{ return mem(static_cast<const OperandConst*>(this->cst(cst))); }
	inline const Operand *mem(const OperandMem& opd_mem)
		{ return mem(opd_mem.addr().value()); }
	const Operand* get(const Operand& opd);

private:
	const Operand *op(arithoperator_t op, const Operand *arg);
	const Operand *op(arithoperator_t op, const Operand *arg1, const Operand *arg2);
	// unused
	const Operand *assocOp(arithoperator_t _op, const Operand *arg1, const Operand *arg2)
		{ return op(_op, arg1, arg2); }
	const Operand *comOp(arithoperator_t _op, const Operand *arg1, const Operand *arg2);

public:
	// inline const Operand *neg (const Operand *arg) { return op(ARITHOPR_NEG, arg); }
#	ifdef DAG_SUM
		inline const Operand *neg (const Operand *arg) 						 { return sum.make(ARITHOPR_NEG, arg); }
		inline const Operand *add (const Operand *arg1, const Operand *arg2) { return sum.make(ARITHOPR_ADD, arg1, arg2); }
		inline const Operand *sub (const Operand *arg1, const Operand *arg2) { return sum.make(ARITHOPR_SUB, arg1, arg2); }
#	else
		inline const Operand *neg (const Operand *arg) 						 { return op(ARITHOPR_NEG, arg); }
		inline const Operand *add (const Operand *arg1, const Operand *arg2) { return comOp(ARITHOPR_ADD, arg1, arg2); }
		inline const Operand *sub (const Operand *arg1, const Operand *arg2) { return op(ARITHOPR_SUB, arg1, arg2); }
#	endif
	inline const Operand *mul (const Operand *arg1, const Operand *arg2) { return comOp(ARITHOPR_MUL, arg1, arg2); }
	inline const Operand *mulh(const Operand *arg1, const Operand *arg2) { return comOp(ARITHOPR_MULH, arg1, arg2); }
	inline const Operand *div (const Operand *arg1, const Operand *arg2) { return op(ARITHOPR_DIV, arg1, arg2); }
	inline const Operand *mod (const Operand *arg1, const Operand *arg2) { return op(ARITHOPR_MOD, arg1, arg2); }
	inline const Operand *cmp (const Operand *arg1, const Operand *arg2) { return op(ARITHOPR_CMP, arg1, arg2); }
	// accepts non-binary calls with NULL second argument
	const Operand *autoOp(arithoperator_t op, const Operand *arg1, const Operand *arg2);
	const Operand *autoOp(arithoperator_t op, const Operand *arg1);

	inline const Operand* smart_add(const Operand* arg1, const Operand* arg2) { return Arith::add(*this, arg1, arg2); }
	inline const Operand* smart_sub(const Operand* arg1, const Operand* arg2) { return Arith::sub(*this, arg1, arg2); }
	inline const Operand* smart_mul(const Operand* arg1, const Operand* arg2) { return Arith::mul(*this, arg1, arg2); }
	inline const Operand* smart_div(const Operand* arg1, const Operand* arg2) { return Arith::div(*this, arg1, arg2); }
	inline const Operand *smart_autoOp(arithoperator_t op, const Operand *arg1, const Operand *arg2) { return Arith::autoOp(*this, op, arg1, arg2); }

	Predicate *pred(condoperator_t _op, const Operand *left, const Operand *right) {
		Pred k(_op, left, right);
		Predicate *p = pred_map.get(k, 0);
		if(!p) {
			p = new Predicate(_op, left, right);
			pred_map.put(k, p);
		}
		return p;
	}

	Predicate *comPred(condoperator_t _op, const Operand *left, const Operand *right) {
		if(left > right)
			return pred(_op, right, left);
		else
			return pred(_op, left, right);
	}

	inline Predicate *eq(const Operand *left, const Operand *right) { return comPred(CONDOPR_EQ, left, right); }
	inline Predicate *ne(const Operand *left, const Operand *right) { return comPred(CONDOPR_NE, left, right); }
	inline Predicate *lt(const Operand *left, const Operand *right) { return pred(CONDOPR_LT, left, right); }
	inline Predicate *le(const Operand *left, const Operand *right) { return pred(CONDOPR_LE, left, right); }
	inline Predicate *gt(const Operand *left, const Operand *right) { return pred(CONDOPR_LT, right, left); }
	inline Predicate *ge(const Operand *left, const Operand *right) { return pred(CONDOPR_LE, right, left); }

	friend io::Output& operator<<(io::Output& out, const DAG& dag) { return dag.print(out); }
private:
	io::Output& print(io::Output& out) const;

	int tmp_cnt, var_cnt;
	Operand **vars;
 	cst_map_t 	cst_map;
 	op_map_t 	op_map;
 	pred_map_t 	pred_map;
#	ifdef DAG_SUM
		Sum sum;
#	endif
}; // DAG class

#endif // DAG_H