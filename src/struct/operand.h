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
 
#ifndef _OPERAND_H
#define _OPERAND_H

#include <otawa/cfg/Edge.h>
#include <elm/data/List.h>
#include <elm/data/Vector.h>
#include <elm/io/Output.h>
#include <elm/util/BitVector.h>
#include <elm/util/Option.h>
#include "constant.h"
#include "constant_variables_core.h"
#include "var_collector.h"

using otawa::Edge;

// using elm::Vector;

enum arithoperator_t
{
	// Unary
	ARITHOPR_NEG, // (-)   Arithmetic negation#define PROCESS_VAL(p) case(p): return out << #p; break;
	// ARITHOPR_NOT, // Should we implement bit inversion? // actually it's just 2^n - x - 1 !
	
	// Binary
	ARITHOPR_ADD, // (+)   Addition
	ARITHOPR_SUB, // (-)   Subtraction
	ARITHOPR_MUL, // (*)   Multiplication
	ARITHOPR_MULH,// (*H)  32 Most significant bits from 64-bit multiplication
	ARITHOPR_DIV, // (/)   Division
	ARITHOPR_MOD, // (mod) Modulo
	ARITHOPR_CMP, // (~)   Special operator to be used with CONDOPR_EQ and a register that indicates
                  //       that it contains information over the comparison of two other registers
};
io::Output& operator<<(io::Output& out, arithoperator_t opr);

enum operand_kind_t
{
	CST,	// Constant value
	ITER,	// Induction Variable
	VAR,	// Variable (register or temporary variable, for now)
	MEM,	// Memory cell
	TOP,	// Constant of unknown value
	ARITH,	// Arithmetic Expression
};
io::Output& operator<<(io::Output& out, operand_kind_t kind);

enum operandmem_kind_t
{
	OPERANDMEM_ABSOLUTE, // Const
	OPERANDMEM_RELATIVE, // sp+Const
};
io::Output& operator<<(io::Output& out, operandmem_kind_t kind);

class Operand;
class OperandConst;
class OperandVar;
class OperandMem;
class OperandTop;
class OperandIter;
class OperandArith;
class AffineEquationState;
class DAG;

// The visitor: an abstract class
class OperandVisitor
{
public:
	virtual bool visit(const OperandConst& o) = 0;
	virtual bool visit(const OperandVar& o) = 0;
	virtual bool visit(const OperandMem& o) = 0;
	virtual bool visit(const OperandTop& o) = 0;
	virtual bool visit(const OperandIter& o) = 0;
	virtual bool visit(const OperandArith& o) = 0;
};
class OperandEndoVisitor
{
public:
	virtual const Operand* visit(const OperandConst& o) = 0;
	virtual const Operand* visit(const OperandVar& o) = 0;
	virtual const Operand* visit(const OperandMem& o) = 0;
	virtual const Operand* visit(const OperandTop& o) = 0;
	virtual const Operand* visit(const OperandIter& o) = 0;
	virtual const Operand* visit(const OperandArith& o) = 0;
};

#define NONEW\
	friend class DAG;\
	inline void *operator new(size_t s) { return new char[s]; }

// Abstract Operand class
class Operand
{
public:
	virtual ~Operand() { }
	// virtual Operand* copy() const = 0;
	virtual unsigned int countTempVars() const = 0; // this will count a variable several times if it occurs several times
	virtual bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const = 0;
	virtual bool involves(const Operand* opd) const = 0; // uses DAG
	virtual int involvesOperand(const Operand& opd) const { return this->operator==(opd) ? 1 : 0; } // default case, to be overloaded for recursive classes
	virtual int involvesVariable(const OperandVar& opdv) const = 0; // TODO! rewrite using involvesOperand, if the int return thing is not critical?
	virtual Option<Constant> involvesStackBelow(const Constant& stack_limit) const = 0;
	virtual bool involvesMemoryCell(const OperandMem& opdm) const = 0;
	virtual const Operand* involvesMemory() const = 0;
	virtual int count() const { return 1; } // count operands
	virtual void collectTops(VarCollector& vc) const = 0;
	virtual Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const = 0;
	virtual void markUsedRegisters(BitVector &uses) const = 0;
	virtual bool isComplete() const = 0;
	virtual bool isConstant() const = 0;
	virtual bool isLinear(bool only_linear_opr) const = 0;
	virtual bool isAffine(const Operand& opd) const = 0;
	virtual void parseAffineEquation(AffineEquationState& state) const = 0;
	virtual Option<Constant> evalConstantOperand() const = 0; // all uses commented out?
	virtual elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const = 0; // First operand is the new this, second operand is the extracted constant, will only extract from sums // TODO: support substraction
	virtual Option<const Operand*> simplify(DAG& dag) const = 0; // Warning: Option=none does not warrant that nothing has been simplified!
	virtual const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const = 0;
	virtual bool accept(OperandVisitor& visitor) const = 0;
	virtual const Operand* accept(OperandEndoVisitor& visitor) const = 0;
	virtual operand_kind_t kind() const = 0;
	friend inline io::Output& operator<<(io::Output& out, const Operand& o) { return o.print(out); }
	virtual bool operator==(const Operand& o) const = 0;
	virtual bool operator< (const Operand& o) const = 0;
	
	inline const bool isAConst() const { return kind() == CST; }
	virtual inline const OperandConst& toConst() const { ASSERTP(false, "not an OperandConst: " << *this << " (" << kind() << ")"); }
	virtual inline const Constant& toConstant()  const { ASSERTP(false, "not a Constant: " 		<< *this << " (" << kind() << ")"); }
	virtual inline const OperandVar& toVar() 	 const { ASSERTP(false, "not an OperandVar: " 	<< *this << " (" << kind() << ")"); }
 	virtual inline const OperandMem& toMem() 	 const { ASSERTP(false, "not an OperandMem: " 	<< *this << " (" << kind() << ")"); }
	virtual inline const OperandTop& toTop() 	 const { ASSERTP(false, "not an OperandTop: " 	<< *this << " (" << kind() << ")"); }
	virtual inline const OperandIter& toIter() 	 const { ASSERTP(false, "not an OperandIter: " 	<< *this << " (" << kind() << ")"); }
	virtual inline const OperandArith& toArith() const { ASSERTP(false, "not an OperandArith: "	<< *this << " (" << kind() << ")"); }

	class Iter : public PreIterator<Iter, const Operand*> {
	public:
		enum {
			WITH_CST = 1 << CST,
			WITH_VAR = 1 << VAR,
			WITH_MEM = 1 << MEM,
			WITH_TOP = 1 << TOP,
			WITH_ITER = 1 << ITER,
			WITH_ARITH = 1 << ARITH,
		};

		Iter(const Operand* o, const int selector_flags = 0xffff-WITH_CST);
		inline bool ended() const
			{ return q.isEmpty(); }
		inline const Operand* item() const
			{ return q.first(); }
		void next();
		
	private:
		void check();

		elm::List<const Operand*> q;
		const int flags;
	};
private:
	virtual io::Output& print(io::Output& out) const = 0;
};

// Constant values
class OperandConst : public Operand
{
public:
	OperandConst(const OperandConst& opd);
	OperandConst(const Constant& value);
	OperandConst(); // for use in Option
	~OperandConst();
	
	inline Constant value() const { return _value; }
	
	unsigned int countTempVars() const { return 0; }
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const;
	bool involves(const Operand* o) const { return this == o; }
	inline int involvesVariable(const OperandVar& opdv) const { return 0; }
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const;
	bool involvesMemoryCell(const OperandMem& opdm) const { return false; }
	const Operand* involvesMemory() const { return NULL; }
	// int count() const;
	inline void collectTops(VarCollector& vc) const { }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const;
	Option<Constant> evalConstantOperand() const;
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const { return Pair<const Operand*, Constant>(NULL, _value); }
	Option<const Operand*> simplify(DAG& dag) const; // Warning: Option=none does not warrant that nothing has been simplified!
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const; // warning: Option=none does not warrant that nothing has been replaced!
	void parseAffineEquation(AffineEquationState& state) const;
	inline void markUsedRegisters(BitVector& uses) const { }
	inline bool isComplete() const { return true; }
	inline bool isConstant() const { return true; }
	inline bool isLinear(bool only_linear_opr)   const { return true; }
	inline bool isAffine(const Operand& opd) const { return true; }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return CST; }
	inline operator Constant() const { return _value; }
	OperandConst& operator=(const OperandConst& opd);
	inline bool operator==(const Operand& o) const { return (kind() == o.kind()) && toConstant() == o.toConstant(); }
	bool operator< (const Operand& o) const;
	inline const OperandConst& toConst() const { return *this; }
	inline const Constant& toConstant() const { return _value; }
	friend inline io::Output& operator<<(io::Output& out, const OperandConst& o) { return o.print(out); }
private:
	// NONEW;
	io::Output& print(io::Output& out) const;

	Constant _value;
};

// Variables
class OperandVar : public Operand
{
public:
	OperandVar(); // for Vector<OperandVar>
	OperandVar(const OperandVar& opd);
	OperandVar(t::int32 addr);
	// ~OperandVar();
	
	inline t::int32 addr() const { return _addr; }
	inline bool isTempVar() const { return _addr < 0; }
	
	// Operand* copy() const;
	unsigned int countTempVars() const { return isTempVar() ? 1 : 0; }
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const;
	bool involves(const Operand* o) const { return this == o; }
	// int involvesOperand(const Operand& opd) const;
	inline int involvesVariable(const OperandVar& opdv) const { return (int)(opdv == *this); }
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const;
	bool involvesMemoryCell(const OperandMem& opdm) const { return false; }
	const Operand* involvesMemory() const;
	// int count() const;
	inline void collectTops(VarCollector& vc) const { }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const;
	Option<Constant> evalConstantOperand() const;
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const { return Pair<const Operand*, Constant>(this, 0); }
	Option<const Operand*> simplify(DAG& dag) const; // Warning: Option=none does not warrant that nothing has been simplified!
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const; // warning: Option=none does not warrant that nothing has been replaced!
	void parseAffineEquation(AffineEquationState& state) const;
	inline void markUsedRegisters(BitVector& uses) const { if(!isTempVar()) uses.set(_addr); }
	inline bool isComplete() const { return true; }
	inline bool isConstant() const { return false; }
	inline bool isLinear(bool only_linear_opr)   const { return true; }
	inline bool isAffine(const Operand& opd) const { return *this == opd; }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return VAR; }
	OperandVar& operator=(const OperandVar& opd);
	bool operator==(const Operand& o) const;
	bool operator< (const Operand& o) const;
	friend inline io::Output& operator<<(io::Output& out, const OperandVar& o) { return o.print(out); }
	inline const OperandVar& toVar() const { return *this; }
private:
	// NONEW;
	io::Output& print(io::Output& out) const;
	t::int32 _addr;
};

class OperandMem : public Operand
{	
public:
	OperandMem(const OperandMem& opd);
	OperandMem(const OperandConst& opdc);
	OperandMem(); // for use in Option
	
	inline const OperandConst& addr() const { return _opdc; }
	inline bool isAligned() const { return _opdc.value().val() % 4 == 0; }
	
	// Operand* copy() const { return new OperandMem(*this); }
	unsigned int countTempVars() const { return 0; }
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const;
	bool involves(const Operand* o) const { return this == o; }
	// int involvesOperand(const Operand& opd) const;
	inline int involvesVariable(const OperandVar& opdv) const;
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const;
	bool involvesMemoryCell(const OperandMem& opdm) const;
	const Operand* involvesMemory() const;
	// int count() const;
	inline void collectTops(VarCollector& vc) const { }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const;
	Option<Constant> evalConstantOperand() const;
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const { return Pair<const Operand*, Constant>(this, 0); }
	Option<const Operand*> simplify(DAG& dag) const; // Warning: Option=none does not warrant that nothing has been simplified!
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const; // warning: Option=none does not warrant that nothing has been replaced!
	void parseAffineEquation(AffineEquationState& state) const;
	inline void markUsedRegisters(BitVector& uses) const { }
	inline bool isComplete() const { return true; }
	inline bool isConstant() const { return false; }
	inline bool isLinear(bool only_linear_opr)   const { return true; }
	inline bool isAffine(const Operand& opd) const { return *this == opd; }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return MEM; }
	OperandMem& operator=(const OperandMem& opd);
	inline bool operator==(const Operand& o) const { return (kind() == o.kind()) && (addr() == ((OperandMem&)o).addr()); }
	bool operator< (const Operand& o) const;
	friend inline io::Output& operator<<(io::Output& out, const OperandMem& o) { return o.print(out); }
	inline const OperandMem& toMem() const { return *this; }
private:
	// NONEW;
	io::Output& print(io::Output& out) const;

	OperandConst _opdc;
};

class OperandTop : public Operand
{
public:
	// OperandTop();
	OperandTop(int id);
	OperandTop(const OperandTop& opd);
	OperandTop(const OperandTop& opd, int offset);
	
	inline bool isUnidentified() const { return id == -1; } // blank Top
	inline bool isIdentified() const { return !isUnidentified(); }
	inline int getId() const { return id; } // shouldn't be used
	inline void scale(int offset) { id += offset; } // for internal use by VarMaker
	
	unsigned int countTempVars() const { return 0; }
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const;
	bool involves(const Operand* o) const { return this == o; }
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const { return elm::none; }
	inline int involvesVariable(const OperandVar& opdv) const { return 0; }
	const Operand* involvesMemory() const { return NULL; }
	// int count() const;
	bool involvesMemoryCell(const OperandMem& opdm) const { return false; }
	inline void collectTops(VarCollector& vc) const { vc.collect(id); }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const;
	Option<Constant> evalConstantOperand() const;
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const { return Pair<const Operand*, Constant>(this, 0); }
	Option<const Operand*> simplify(DAG& dag) const; // Warning: Option=none does not warrant that nothing has been simplified!
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const; // warning: Option=none does not warrant that nothing has been replaced!
	void parseAffineEquation(AffineEquationState& state) const { crash(); } // should never happen, I think?
	inline void markUsedRegisters(BitVector& uses) const { }
	inline bool isComplete() const { return isIdentified(); }
	inline bool isConstant() const { return false; }
	inline bool isLinear(bool only_linear_opr)   const { return true; }
	inline bool isAffine(const Operand& opd) const { return *this == opd; }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return TOP; }
	OperandTop& operator=(const OperandTop& opd);
	inline bool operator==(const Operand& o) const { return kind() == o.kind() && id == ((OperandTop&)o).id; }
	bool operator< (const Operand& o) const;
	friend inline io::Output& operator<<(io::Output& out, const OperandTop& o) { return o.print(out); }
	inline const OperandTop& toTop() const { return *this; }
private:
	// NONEW;
	io::Output& print(io::Output& out) const;

	int id;
	// static int next_id;
};
extern OperandTop const* const Top;

// Induction variables
class OperandIter : public Operand
{
public:
	OperandIter(const otawa::Block* loop, bool done = false) : lid(loop), done(done) { }
	OperandIter(const OperandIter& opd) : lid(opd.lid), done(opd.done) { }
	
	inline const otawa::Block* loop() const { return lid; }
	inline bool isDone() const { return done; }
	inline void finalize() { done = true; }
	
	unsigned int countTempVars() const { return 0; }
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const { expr = this; return false; }
	bool involves(const Operand* o) const { return this == o; }
	inline int involvesVariable(const OperandVar& opdv) const { return false; }
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const { return none; }
	bool involvesMemoryCell(const OperandMem& opdm) const { return false; }
	const Operand* involvesMemory() const { return NULL; }
	// int count() const;
	inline void collectTops(VarCollector& vc) const { }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const { return (this == opd) ? some(opd_modifier) : none; }
	Option<Constant> evalConstantOperand() const { return none; }
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const { return Pair<const Operand*, Constant>(this, 0); }
	Option<const Operand*> simplify(DAG& dag) const { return none; }
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const { return this; }
	void parseAffineEquation(AffineEquationState& state) const { crash(); }
	inline void markUsedRegisters(BitVector& uses) const { }
	inline bool isComplete() const { return true; }
	inline bool isConstant() const { return false; }
	inline bool isLinear(bool only_linear_opr) const { return true; }
	inline bool isAffine(const Operand& opd) const { return *this == opd; }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return ITER; }
	OperandIter& operator=(const OperandIter& opd) { lid = opd.lid; done = opd.done; return *this; }
	inline bool operator==(const Operand& o) const { return o.kind() == ITER && lid == static_cast<const OperandIter&>(o).lid && done == static_cast<const OperandIter&>(o).done; }
	bool operator< (const Operand& o) const;
	friend inline io::Output& operator<<(io::Output& out, const OperandIter& o) { return o.print(out); }
	inline const OperandIter& toIter() const { return *this; }
private:
	io::Output& print(io::Output& out) const;

	const otawa::Block* lid;
	bool done;
};

// Arithmetic Expressions
class OperandArith : public Operand
{
public:
	// OperandArith(const OperandArith& opd);
	// OperandArith(arithoperator_t opr, const Operand& opd1_); // unary constructor 
	// OperandArith(arithoperator_t opr, const Operand& opd1_, const Operand& opd2_);
	OperandArith(arithoperator_t opr, const Operand* opd1_, const Operand* opd2_ = NULL);
	// ~OperandArith();
	
	inline arithoperator_t opr() const { return _opr; }
	inline const Operand* left() const { return opd1; }
	inline const Operand* right() const { return opd2; }
	inline const Operand& leftOperand() const { return *opd1; }
	inline const Operand& rightOperand() const { ASSERT(isBinary()); return *opd2; }
	
	inline bool isUnary()  const { return _opr <  ARITHOPR_ADD; }
	inline bool isBinary() const { return _opr >= ARITHOPR_ADD; }
	
	// Operand* copy() const;
	unsigned int countTempVars() const;
	bool involves(const Operand* o) const { return this == o || opd1 == o || opd2 == o; }
	int involvesOperand(const Operand& opd) const { return opd1->involvesOperand(opd) + (opd2 ? opd2->involvesOperand(opd) : 0); }
	inline int involvesVariable(const OperandVar& opdv) const;
	Option<Constant> involvesStackBelow(const Constant& stack_limit) const;
	bool involvesMemoryCell(const OperandMem& opdm) const;
	bool getIsolatedTempVar(OperandVar& temp_var, Operand const*& expr) const;
	const Operand* involvesMemory() const;
	int count() const { return isBinary() ? opd1->count() + opd2->count() : opd1->count(); }
	inline void collectTops(VarCollector& vc) const { opd1->collectTops(vc); if(isBinary()) opd2->collectTops(vc); }
	Option<const Operand*> update(DAG& dag, const Operand* opd, const Operand* opd_modifier) const;
	Option<Constant> evalConstantOperand() const;
	elm::Pair<const Operand*, Constant> extractAdditiveConstant(DAG& dag) const;
	Option<const Operand*> simplify(DAG& dag) const; // Warning: Option=none does not warrant that nothing has been simplified!
	const Operand* replaceConstants(DAG& dag, const ConstantVariablesCore& constants, Vector<OperandVar>& replaced_vars) const; // warning: Option=none does not warrant that nothing has been replaced!
	void parseAffineEquation(AffineEquationState& state) const;
	inline void markUsedRegisters(BitVector& uses) const { opd1->markUsedRegisters(uses); if(isBinary()) opd1->markUsedRegisters(uses); }
	inline bool isComplete() const { return _opr != ARITHOPR_CMP && opd1->isComplete() && (isUnary() || opd2->isComplete()); }
	inline bool isConstant() const { return opd1->isConstant() && (isUnary() || opd2->isConstant()); }
	bool isLinear(bool only_linear_opr) const;
	inline bool isAffine(const Operand& opd) const
		{ return ((_opr == ARITHOPR_ADD) || (_opr == ARITHOPR_SUB)) && opd1->isAffine(opd) && opd2->isAffine(opd); }
	inline bool accept(OperandVisitor& visitor) const { return visitor.visit(*this); }
	inline const Operand* accept(OperandEndoVisitor& visitor) const { return visitor.visit(*this); }
	inline operand_kind_t kind() const { return ARITH; }
	// OperandArith& operator=(const OperandArith& opd);
	inline bool operator==(const Operand& o) const;
	bool operator< (const Operand& o) const;
	friend inline io::Output& operator<<(io::Output& out, const OperandArith& o) { return o.print(out); }
	inline const OperandArith& toArith() const { return *this; }
private:
	// NONEW;
	io::Output& print(io::Output& out) const;
	
	arithoperator_t _opr;
	const Operand* opd1;
	const Operand* opd2; // unused if operator is unary
};

// for v1 only // no longer
class AffineEquationState // WARNING: this only makes sense when we know the equation is affine in one var
{
public:
	AffineEquationState(const Operand& var) : _is_negative(false), _delta(0), _sp_counter(0), _var_counter(0), _var(var) { }
	inline short delta()       const { return sign()*_delta;     }
	inline short spCounter()   const { return sign()*_sp_counter;  }
	inline short varCounter()  const { return sign()*_var_counter; } // this includes mem too now
	inline void addToDelta(short d) { _delta += d; }
	inline void onVarFound(const Operand& var) { if(var == _var) _var_counter++; }
	inline void onSpFound(bool sign = SIGN_POSITIVE) { if(sign == SIGN_POSITIVE) _sp_counter++; else _sp_counter--; }
	void reverseSign() {
		_is_negative ^= 1;
		_delta = -_delta;
		_sp_counter = -_sp_counter;
		_var_counter = -_var_counter;
	}
private:
	// NONEW;
	inline short sign() const { if(_is_negative) return -1; else return +1; }

	bool _is_negative;
	short _delta;
	short _sp_counter;
	short _var_counter;
	const Operand& _var;
};

#endif
