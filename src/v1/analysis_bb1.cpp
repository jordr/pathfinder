/*
 *	V1 of micro analysis: defines how we parse a BasicBlock (class used for -1)
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

#include <otawa/sem/PathIter.h>
#include <otawa/sem/inst.h>
#include <elm/string/String.h>
#include "../analysis_state.h"
#include "../debug.h"
#include "../struct/predicate.h"
#include "../struct/DAG.h"

using namespace otawa::sem;

/**
  * @brief processes a semantic instruction (v1)
  * @param inst	          Instruction to process
  * @param last_condition If we are in a conditional segment, the sem inst corresponding to the conditional instruction, NOP otherwise
  */
void Analysis::State::processSemInst1(const sem::inst& inst, const sem::inst& last_condition)
{
#ifdef V1
	elm::Pair<const Operand*, const Operand*> opds(NULL, NULL);
	condoperator_t opr = CONDOPR_EQ; // default is =
	Path labels; // default is {}
	bool make_pred = false;
	
	// some shortcuts
	const t::int16 &a = inst.a(), &b = inst.b(), &d = inst.d();
	const t::int32 &cst = inst.cst();
	const t::int16 &reg = inst.reg(), &addr = inst.addr();
	
	switch(inst.op)
	{
		case NOP:
			break;
		case BRANCH:
			break;
		case CONT:
			{
				const Operand *opd_left = NULL, *opd_right = NULL;
				if(findValueOfCompVar(OperandVar(last_condition.sr()), opd_left, opd_right, labels))
				{
					Predicate p = getConditionalPredicate(last_condition.cond(), opd_left, opd_right, false); // false because we need to invert condition (else branch)
					make_pred = true;
					opr = p.opr();
					opds = pair(p.left(), p.right());
				}
			}
			break; // we cannot generate a predicate otherwise
		case IF:
			{
				const Operand *opd_left = NULL, *opd_right = NULL;
				if(findValueOfCompVar(OperandVar(last_condition.sr()), opd_left, opd_right, labels))
				{
					Predicate p = getConditionalPredicate(last_condition.cond(), opd_left, opd_right, true);
					make_pred = true;
					opr = p.opr();
					opds = pair(p.left(), p.right());
				}
			}
			break; // we cannot generate a predicate otherwise
		case LOAD: // reg <- MEM_type(addr)
			invalidateVar(reg);
			if(Option<OperandMem> addr_mem = getOperandMem(addr, labels))
			{
				// if it's a constant address of some read-only data
				if(Option<Constant> addr_const_value = getConstantValueOfReadOnlyMemCell(*addr_mem, inst.type()))
				{
					DBG(color::IPur() << DBG_SEPARATOR " " << color::IBlu() << "R-O memory data " << *addr_mem << " simplified to " << *addr_const_value)
					constants.set(d, ConstantVariables::LabelledValue(*addr_const_value, labels, true)); 
				}
				// or maybe we can link it to a constant value from the predicates
				else if(Option<Constant> addr_const_value = findConstantValueOfMemCell(*addr_mem, labels))
				{
					DBG(color::IPur() << DBG_SEPARATOR " " << color::IBlu() << "Memory data " << *addr_mem << " simplified to " << *addr_const_value)
					constants.set(d, ConstantVariables::LabelledValue(*addr_const_value, labels, true));
				}
				// or maybe it's a 1/2byte non aligned access and we can get its value via a divmod (f.e [0x8001] = [0x8000])
				else if(!(*addr_mem).isAligned())
				{
					sem::type_t type = inst.type();
					const int shift = (*addr_mem).addr().value().val() % 4;
					ASSERTP(getSizeOfType(type) + shift <= 4, "unaligned access is overflowing to next memcell!")
					// could improve with a shifted predicate
					// DBGW("TODO: load at unaligned addr: " << *addr_mem << ", not implemented yet (probably unimpactful)")
				}
				// if we can't value it, just say reg = [addr]
				else
				{
					opds = pair(Var(reg), Mem(*addr_mem));
					make_pred = true;
				}
			}
			else
			{
				OperandVar addr_var = OperandVar(addr);
				DBG(color::IPur() << DBG_SEPARATOR " " << color::IYel() << "Could not simplify " << addr_var)
			}
			break;
		case STORE:	// MEM_type(addr) <- reg
			if(Option<OperandMem> addr_mem = getOperandMem(addr, labels))
			{
				if(!(*addr_mem).isAligned())
				{
					const int shift = (*addr_mem).addr().value().val() % 4;
					DBG(color::IYel() << "Unaligned STORE to " << *addr_mem << ", invalidating " << OperandMem((*addr_mem).addr().value() - shift))
					sem::type_t type = inst.type();
					ASSERTP(getSizeOfType(type) + shift <= 4, "unaligned access is overflowing to next memcell!")
					// ASSERTP(false, "store at unaligned access")
					
					invalidateMem(OperandMem((*addr_mem).addr().value() - shift));
					// TODO could improve with a shifted predicate
				}
				else
				{
					invalidateMem(*addr_mem);
					opds = pair(Var(reg), Mem(*addr_mem));
					make_pred = true;
				}
			}
			else
			{
				OperandVar addr_var = OperandVar(addr);
				DBG(color::IPur() << DBG_SEPARATOR " " << color::IYel() << "Could not simplify " << addr_var << ", invalidating whole memory")
				invalidateAllMemory();
			}
			break;
		case SET:
			invalidateVar(d);
			if(isConstant(a)) // if a is already identified as a constant
				constants.set(d, constants[a], getLabels(a)); // then constants[d] = constants[a]
			else // no use generating this predicate if it is a constant, because the ConstantVariables object handles that
			{
				opds = pair(Var(d), Var(a));
				make_pred = true;
			}
			break;
		case SETI:
			invalidateVar(d);
			// everything should already be handled by the ConstantVariables object
			constants.set(d, cst); // constants[d] = cst
			break;
		case SETP:
			DBG(color::BIRed() << "Unimplemented operator SETP running!")
			ASSERT(!UNTESTED_CRITICAL);
			invalidateVar(d);
			break;
		case CMP:
		case CMPU:
			opds = pair(Var(d), cmp(Var(a), Var(b)));
			make_pred = true;
			invalidateVar(d);
			break;
		case ADD:
			if(d == a)
			{
				if(d == b) // d <- d+d
				{	// [d/2 / d]
					opds = pair(mod(Var(d), Cst(2)), Cst(0));
					make_pred = true; // d % 2 = 0
					if(isConstant(d))
						constants.set(d, constants[d]*2, getLabels(d));
					update(OperandVar(d), div(Var(d), Cst(2)), labels);
				}
				else // d <- d+b
				{	// [d-b / d]
					// update(OperandVar(d), sub(Var(d), Var(b)), labels); //!\\ do not do this before settings constants
					// otherwise it gives f.e. /* ?2 = ?3, ?3==2 */ ?3 := ?3+1 -> /* ?2 = [?3-1]=[2-1], ?3==3... that's bad! */
					if(isConstant(d))
					{
						if(isConstant(b))
							constants.set(d, constants[d]+constants[b], getLabels(d, b));
						else
						{
							// d = constants[d] + b
							opds = pair(Var(d), add(Cst(constants[d]), Var(b)));
							make_pred = true;
							constants.invalidate(d);
						}
						update(OperandVar(d), sub(Var(d), Var(b)), labels);
					}
				}
			}
			else 
			{
				if(d == b) // d <- d+a
				{	// [d-a / d]
					if(isConstant(d))
					{
						if(isConstant(a))
							constants.set(d, constants[d]+constants[a], getLabels(d, a));
						else
						{
							// d = a + constants[d]
							opds = pair(Var(d), add(Var(a), Cst(constants[d])));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					update(OperandVar(d), sub(Var(d), Var(a)), labels);
				}
				else
				{
					// d is not related to a or b, invalidate predicates that involve d
					invalidateVar(d);
					if(isConstant(a) && isConstant(b))
						constants.set(d, constants[a]+constants[b], getLabels(a, b));
					else
					{
						opds = pair(Var(d), add(Var(a), Var(b)));
						make_pred = true; // d = a+b
					}
					// the invalidation in constants is already handled by invalidateVar
				}
			}
			break;
		case SUB:
			if(d == a)
			{
				if(d == b) // d <- d-d
				{	// [0 / d], d is set to 0!
					invalidateVar(d);
					constants.set(d, 0);
				}
				else // d <- d-b
				{	// [d+b / d]
					if(isConstant(d))
					{
						if(isConstant(b))
							constants.set(d, constants[d]-constants[b], getLabels(d, b));
						else
						{
							// d = constants[d] - b
							opds = pair(Var(d), sub(Cst(constants[d]), Var(b)));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					update(OperandVar(d), add(Var(d), Var(b)), labels);
				}
			}
			else
			{
				if(d == b) // d <- a-d
				{	// [a-d / d], this function f has a f°f=Id property
					if(isConstant(d))
					{
						if(isConstant(a)) // everything is const, update const value of d
							constants.set(d, constants[a]-constants[d], getLabels(a, d));
						else
						{
							// d = a - constants[d]
							opds = pair(Var(d), sub(Var(a), Cst(constants[d])));
							make_pred = true;

							constants.invalidate(d);
						}
					}
					update(OperandVar(d), sub(Var(a), Var(d)), labels);
				}
				else // d <- a-b
				{
					invalidateVar(d);
					if(isConstant(a) && isConstant(b))
						constants.set(d, constants[a]-constants[b], getLabels(a, b));
					else
					{
						opds = pair(Var(d), sub(Var(a), Var(b)));
						make_pred = true;
						// the invalidation in constants is already handled by invalidateVar
					}
				}
			}
			break;
		case SHL:
			{
				/*if(d == b) // safety test, shouldn't happen unless assembly language allows variable bit shifting
				{	// well actually ldr r3, [r3, r2, lsl #2] makes it happen in ARM...
					invalidateVar(d);
					break;
				}*/
				// b is usually a tempvar that has been previously set to a constant value
				if(!isConstant(b) || !constants[b].isAbsolute())
				{
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << OperandVar(b) << " could not be identified to a constant value]")
					invalidateVar(d); // only invalidate now (otherwise we can't find t1 for shl t1, ?0, t1)
					break;
				}
				t::int32 b_val = constants[b].val();
				DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << OperandVar(b) << " identified as 0x" << io::hex(b_val) << "]")
				if(d == a) // d <- d << b
				{
					labels = getLabels(b);
					// we also add a predicate to say that d is now a multiple of 2^b
					if(isConstant(d)) // we must update constant value of d
					{
						const Path& l = getLabels(d, b);
						constants.set(d, constants[d] << b_val, l);
					}
					else
					{ 
						opds = pair(mod(Var(d), Cst(1 << b_val)), Cst(0));
						make_pred = true; // will use labels
					}
					update(OperandVar(d), div(Var(d), Cst(1 << b_val)), labels);
				}
				else // d <- a << b
				{
					if(isConstant(a))
					{
						const Path& l = getLabels(a, b);
						invalidateVar(d);
						constants.set(d, constants[a] << b_val, l);
					}
					else
					{
						invalidateVar(d);
						opds = pair(Var(d), mul(Var(a), Cst(1 << b_val)));
						make_pred = true;
					}
				}
			}
			break;
		case ASR: // TODO test: is this really legit?
		case SHR:
			{
				// b is usually a tempvar that has been previously set to a constant value
				if(!isConstant(b) || !constants[b].isAbsolute())
				{
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << OperandVar(b) << " could not be identified as a constant value]")
					invalidateVar(d);
					break;
				}
				else if(d == a) // d <- d >> b
				{
					invalidateVar(d);
					break; // not much we can do because we lost info (cf (*))
				}
				else // d <- a >> b
				{
					t::int32 b_val = constants[b].val();
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << OperandVar(b) << " identified as 0x" << io::hex(b_val) << "]")
					if(isConstant(a))
					{
						const Path& l = getLabels(a, b);
						invalidateVar(d);
						constants.set(d, constants[a] >> b_val, l);
					}
					else
					{
						labels = getLabels(b);
						invalidateVar(d);
						opds = pair(Var(d), div(Var(a), Cst(1 << b_val)));
						make_pred = true; // this will use labels
					}
				}
			}
			break;
		case NEG: // TODO test
			UNTESTED_CODE("operator")
			{
				if(a == d) // d <- -d
				{	// [-1 * d / d]
					if(isConstant(d))
						constants.set(d, -constants[d], getLabels(d));
					update(OperandVar(d), neg(Var(d)), labels); // TODO test
				}
				else
				{	
					invalidateVar(d);
					if(isConstant(a))
						constants.set(d, -constants[a], getLabels(a));
					else
					{
						opds = pair(Var(d), mul(Cst(-1), Var(a)));
						make_pred = true;
					}
				}
			}
			break;
		case NOT: // d <- ~a
			if(isConstant(a) && constants[a].isAbsolute()) 
			{
				t::int32 a_val = ~(constants[a].val());
				if(a_val < 0)
					a_val = -a_val; // TODO: handle better with unsigned support
				labels = getLabels(a); // save before in case d==a
				invalidateVar(d);
				constants.set(d, a_val, labels);
			}
			else invalidateVar(d);
			break;
		case AND: // d <- a & b
		case OR:  // d <- a | b
		case XOR: // d <- a ^ b
			if(isConstant(a) && isConstant(b) && constants[a].isAbsolute() && constants[b].isAbsolute())
			{
				t::int32 val;
				if(inst.op == AND)
					val = constants[a].val() & constants[b].val();
				else if(inst.op == XOR)
					val = constants[a].val() ^ constants[b].val();
				else //if(inst.op == OR)
					val = constants[a].val() | constants[b].val();
				labels = getLabels(a, b);
				invalidateVar(d); // don't do this earlier in case d==a or d==b
				constants.set(d, val, labels);
			}
			else if(inst.op == AND &&
				 (	(isConstant(a) && constants[a].isAbsolute() && !isConstant(b))
				 || (isConstant(b) && constants[b].isAbsolute() && !isConstant(a)) ))
			{
				const t::int32 cstv = constants[isConstant(a) ? a : b].val();
				const t::int16& varopd = isConstant(a) ? b : a;
				int x = cstv, i = 0;
				while(x&1)
				{
					x >>= 1;
					i++;
				}
				if(cstv < 0)
				{
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [Right operand of AND affects MSB]") // Most Significant Bit
					invalidateVar(d);
				}
				else if(x == 0) // cstv was 00...000111..111, and i is the count of 1s
				{
					int mod_factor = cstv+1; // if cstv is 111, mod factor is 8 = 1000
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [Detected a modulus " << mod_factor << " operation on " << OperandVar(varopd) << "]")
					if(d == varopd)
					{	// d <- d & k_b
					 	// we should find predicate with isolated d...
						invalidateVar(d);
					}
					else
					{	// d <- a & k_b
						invalidateVar(d);
						opr = CONDOPR_EQ;
						opds = pair(Var(d), mod(Var(varopd), Cst(mod_factor)));
						make_pred = true;
					}
				}
				else
				{
					DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [An operand could not be identified as a constant value]")
					invalidateVar(d);
				}
			}
			else
			{
				DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [An operand could not be identified as a constant value]")
				invalidateVar(d);
			}
			break;
		case MULU:
			UNTESTED_CODE("unsigned variant")
		case MUL:
			if(d == a)
			{
				if(d == b) // d <- d*d
				{
					UNTESTED_CODE("case of operator MUL")
					invalidateVar(d, KEEP_CONSTANT_INFO); // we have no way to do [sqrt(d) / d], so just invalidate
					opr = CONDOPR_LE; // and add a "0 <= d" predicate
					opds = pair(Cst(0), Var(d));
					make_pred = true;
					if(isConstant(d))
						constants.set(d, constants[d]*constants[d], getLabels(d));
					else
						constants.invalidate(d);
				}
				else // d <- d*b
				{	// [d/b / d] // we will have to assume that 0/0 is scratch!
					if(isConstant(d))
					{
						invalidateVar(d, KEEP_CONSTANT_INFO); // keep constants[d], we'll need it
						if(isConstant(b))
							constants.set(d, constants[d]*constants[b], getLabels(d, b));
						else // d <- #d * b
						{
							opds = pair(Var(d), mul(Cst(constants[d]), Var(b)));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					else
					{	// we add a predicate to say that d is now a multiple of b
						if(isConstant(b))
						{
							opds = pair(Cst(0), mod(Var(d), Cst(constants[b]))); // d % b (%0 is to consider!, Cst(0);
							make_pred = true;
						} // I don't think it's worth it to add this "b divides d" predicate if b is not constant... maybe I'm wrong
						update(OperandVar(d), div(Var(d), Var(b)), labels);
					}
				}
			}
			else
			{
				if(d == b) // d <- a*d
				{	// [d/a / d] // we will have to assume that 0/0 is scratch!
					if(isConstant(d))
					{
						invalidateVar(d, KEEP_CONSTANT_INFO); // keep constants[d], we'll need it
						if(isConstant(a))
							constants.set(d, constants[d]*constants[a], getLabels(d, a));
						else
						{
							opds = pair(Var(d), mul(Cst(constants[d]), Var(a)));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					else
					{	// we add a predicate to say that d is now a multiple of a
						if(isConstant(a))
						{
							opds = pair(Cst(0), mod(Var(d), Cst(constants[a]))); // d % a (%0 is to consider!, Cst(0);
							make_pred = true;
						} // I don't think it's worth it to add this "a divides d" predicate if a is not constant... maybe I'm wrong
						update(OperandVar(d), div(Var(d), Var(a)), labels);
					}
				}
				else // d <- a*b
				{
					invalidateVar(d);
					if(isConstant(a) && isConstant(b))
						constants.set(d, constants[a]*constants[b], getLabels(a, b));
					else
					{
						const Operand* opd21 = isConstant(a) ? Cst(constants[a]) : Var(a);
						const Operand* opd22 = isConstant(b) ? Cst(constants[b]) : Var(b);
						opds = pair(Var(d), mul(opd21, opd22));
						make_pred = true;
					}
				}
			}
			break;
		case MULH:
			if(d == a)
			{
				if(d == b) // d <- d*d >>32
				{
					UNTESTED_CODE("case of operator MULH")
					invalidateVar(d, KEEP_CONSTANT_INFO); // we have no way to do [sqrt(d) / d], so just invalidate
					opr = CONDOPR_LE; // and add a "0 <= d" predicate
					opds = pair(Cst(0), Var(d));
					make_pred = true;
					if(isConstant(d))
						constants.set(d, (t::int64(constants[d].val())*t::int64(constants[d].val()))>>32, getLabels(d));
					else
						constants.invalidate(d);
				}
				else // d <- d*b >>32
				{	// [d/b / d] // we will have to assume that 0/0 is scratch!
					invalidateVar(d, KEEP_CONSTANT_INFO);
					if(isConstant(d))
					{
						if(isConstant(b))
							constants.set(d, (t::int64(constants[d].val())*t::int64(constants[b].val()))>>32, getLabels(d, b));
						else
						{
							UNTESTED_CODE("case of operator MULH")
							opds = pair(Var(d), mulh(Cst(constants[d]), Var(b)));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					else
						constants.invalidate(d);
				}
			}
			else
			{
				if(d == b) // d <- a*d >>32
				{	// [d/a / d] // we will have to assume that 0/0 is scratch!
					invalidateVar(d, KEEP_CONSTANT_INFO);
					if(isConstant(d))
					{
						if(isConstant(a))
							constants.set(d, (t::int64(constants[a].val())*t::int64(constants[d].val()))>>32, getLabels(d, a));
						else
						{
							opds = pair(Var(d), mulh(Var(a), Cst(constants[d])));
							make_pred = true;
							constants.invalidate(d);
						}
					}
					else
						constants.invalidate(d);
				}
				else // d <- a*b >>32
				{
					invalidateVar(d);
					if(isConstant(a) && isConstant(b))
						constants.set(d, (t::int64(constants[a].val())*t::int64(constants[b].val()))>>32, getLabels(a, b));
					else
					{
						const Operand* opd21 = isConstant(a) ? Cst(constants[a]) : Var(a);
						const Operand* opd22 = isConstant(b) ? Cst(constants[b]) : Var(b);
						opds = pair(Var(d), mulh(opd21, opd22));
						make_pred = true;
					}
				}
			}
			break;
		case DIVU:
			UNTESTED_CODE("unsigned variant")
		case DIV:
			if(d == a)
			{
				if(d == b) // d <- d / d
				{	// [1 / d]
					// TODO: this should be okay to assume d != 0, right? otherwise the program is faulty?
					// or should we use a SMT solver mode that supports / 0?
					UNTESTED_CODE("operator")
					invalidateVar(d);
					opds = pair(Var(d), Cst(1));
					make_pred = true; // d = 1
					constants.set(d, 1);
				}
				else // d <- d / b
				{	/* TODO (*): we can actually handle this, the following way:
					* Example: 2y - (x + z) = 5		(problem: if - is *, there is no bijective reverse function...)
					* Instruction : x <- x / 2 		(can be DIV or SHR/ASR!)
					* <=> x + z = 2y - 5
					* <=> x = 2y - 5 - z
					* Now x <- x / 2
					* <=> x = (2y - 5 - z) / 2y 		(instead of trying to update x!)
					* 
					* this looks hard to implement though
					*/
					invalidateVar(d, KEEP_CONSTANT_INFO); // we cannot just write [d*a / d] because we lost info
					if(isConstant(d))
					{
						if(isConstant(b))
							constants.set(d, constants[d]/constants[b], getLabels(d, b));
						else
						{
							UNTESTED_CODE("operator")
							Constant d_val = constants[d]; // remember this value to use it in predicate
							constants.invalidate(d);
							opds = pair(Var(d), div(Cst(d_val), Var(b)));
							make_pred = true;
						}
					}
				}
			}
			else
			{
				if(d == b) // d <- a / d
				{
					invalidateVar(d, KEEP_CONSTANT_INFO);
					if(isConstant(d))
					{
						if(isConstant(a))
							constants.set(d, constants[a]/constants[d], getLabels(a, d));
						else
						{
							Constant d_val = constants[d]; // remember this value to use it in predicate
							constants.invalidate(d);
							opds = pair(Var(d), div(Var(a), Cst(d_val)));
							make_pred = true;
						}
					}
				}
				else // d <- a / b
				{
					invalidateVar(d);
					if(isConstant(a) && isConstant(b))
						constants.set(d, constants[a]/constants[b], getLabels(a, b));
					else
					{
						opds = pair(Var(d), div(Var(a), Var(b)));
						make_pred = true; // d = a / b
					}
				}
			}
			break;
		case MODU:
		case MOD:
			invalidateVar(d);
			if(d == a || d == b)
				break;
			opds = pair(Var(d), mod(Var(a), Var(b)));
			if(isConstant(a) && isConstant(b))
				constants.set(d, constants[a]%constants[b], getLabels(a, b));
			else
				make_pred = true;
			break;
		default:
			DBG(color::BIRed() << "Unknown seminst running!")
			ASSERT(!UNTESTED_CRITICAL);
		case SPEC: // special instruction (d: code, cst: sub-code)
		case SCRATCH:
			invalidateVar(d);
	}
	if(make_pred)
		generated_preds += makeLabelledPredicate(opr, opds.fst, opds.snd, labels);
#else
	crash();
#endif
}

Option<OperandMem> Analysis::State::getOperandMem(const OperandVar& var, Path& labels)
{
#ifdef V1
	if(isConstant(var))
		return OperandMem(constants[var]); // TODO: get labels?
	if(Option<t::int32> val = findStackRelativeValueOfVar(var, labels))
		return OperandMem(OperandConst(SP+*val));
	return none;
#else
	crash();
#endif
}

// TODO: maybe we should improve this to handle affine equations ("[SP-12] -1 = 0" etc...)
Option<Constant> Analysis::State::findConstantValueOfMemCell(const OperandMem& mem, Path &labels)
{
	for(PredIterator piter(*this); piter; piter++)
	{
		const Predicate& p = piter.pred();
		if(p.opr() != CONDOPR_EQ)
			continue;
		if(p.leftOperand() == mem)
		{
			if(Option<Constant> maybe_val = p.rightOperand().evalConstantOperand())
			{
				labels += piter.labels();
				return maybe_val;
			}
		}
		else if(p.rightOperand() == mem)
		{
			if(Option<Constant> maybe_val = p.leftOperand().evalConstantOperand())
			{
				labels += piter.labels();
				return maybe_val;
			}
		}
	}
	return elm::none;
}

/**
 * @fn bool Analysis::State::findStackRelativeValueOfVar(const OperandVar& var, OperandConst& val);
 * For a variable tX or ?X, try to find a predicate tX=sp+cst or ?X=sp+cst, and return cst when successful.
 * @return true if a value was found
 */
Option<t::int32> Analysis::State::findStackRelativeValueOfVar(const OperandVar& var, Path &labels)
{
	for(PredIterator piter(*this); piter; piter++)
	{
		if(piter.pred().opr() != CONDOPR_EQ)
			continue;
		// Algorithm 3 (affine): ((?var +- ...) +- ... = (...)
		if(piter.pred().isAffine(var)) // try and look for an affine case ((.. + ..)-..) = (..-..)
		{
			AffineEquationState eqstate(var);
			piter.pred().leftOperand().parseAffineEquation(eqstate);
			eqstate.reverseSign();
			piter.pred().rightOperand().parseAffineEquation(eqstate);
			if(eqstate.spCounter() == +1 && eqstate.varCounter() == -1) // var = sp + delta
			{
				labels += piter.labels();
				return some((t::int32)eqstate.delta());
			}
			else if(eqstate.spCounter() == -1 && eqstate.varCounter() == +1) // sp = var + delta
			{
				labels += piter.labels();
				return some((t::int32)-eqstate.delta()); // var = sp + (-delta)
			}
			// else algorithm failed (for example var = -sp or var = sp+sp)
		}
	}
	DBG(color::IRed() << "findStackRelativeValueOfVar failed! (store/load to T)")
	// DBG(dumpEverything())
	return none; // no matches found
}
/**
 * @fn bool Analysis::State::findValueOfCompVar(const OperandVar& var, Operand*& opd_left, Operand*& opd_right);
 * For a register variable ?X, try to find a predicate ?X = (opd_left ~ opd_right), and return false if it cannot find any
 * @return true if a value was found
 */
bool Analysis::State::findValueOfCompVar(const OperandVar& var, Operand const*& opd_left, Operand const*& opd_right, Path& labels)
{
	// We only explore the local generated_preds. This seems reasonable since we should be able to find this predicate in the local BB
	// BUT this does not exempt us from looking for labels because these predicates can be themselves associated to predicates of previous BBs
	for(elm::List<LabelledPredicate>::Iter iter(generated_preds); iter; iter++)
	{
		const Predicate& p = iter->pred();
		if(p.opr() == CONDOPR_EQ) // operator is =
		{
			if(p.leftOperand() == var) // left operand matches our register
			{
				if(p.rightOperand().kind() == ARITH)
				{
					const OperandArith& opd = (const OperandArith&)p.rightOperand();
					if(opd.opr() == ARITHOPR_CMP) // other operand has to be ??? ~ ???
					{
						opd_left = opd.left();
						opd_right = opd.right();
						labels += iter->labels();
						return true;
					}
				}
			}
			if(p.rightOperand() == var) // right operand matches our register
			{
				if(p.leftOperand().kind() == ARITH)
				{
					const OperandArith& opd = (OperandArith&)p.leftOperand();
					if(opd.opr() == ARITHOPR_CMP) // other operand has to be ??? ~ ???
					{
						opd_left = opd.left();
						opd_right = opd.right();
						labels += iter->labels();
						return true;
					}
				}
			}
		}
	}
	return false; // No matches found
}

// generic function on works
// TODO: documentation
/*bool Analysis::State::invalidate(const Operand& var, bool invalidate_constant_info)
{
	bool rtn = false;

	// try and identify a value for ?3 (look for a ?3 = X predicate)
	for(PredIterator piter(*this); piter; piter++)
	{
		const Predicate &p = piter.pred();
		if(p.opr() == CONDOPR_EQ)
		{
			const int left_involves_var = p.leftOperand().involvesOperand(var);
			const int right_involves_var = p.rightOperand().involvesOperand(var);
			if(left_involves_var == 1 && right_involves_var == 0) // var is in left operand
			{
				if(p.leftOperand() == var) // left operand is exactly var (that is, predicate is "var = ???")
				{
					const Operand* expr = p.right(); // backup the "???" expr
					removePredicate(piter); // remove current predicate
					replaceOperand(var, *expr); // [??? / var]
					DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, &var, expr))
					rtn = true;
					break; // stop and move on
				}
			}
			else if(left_involves_var == 0 && right_involves_var == 1) // var is in right operand
			{
				if(p.rightOperand() == var) // right operand is exactly var (that is, predicate is "??? = var")
				{
					const Operand* expr = p.left(); // backup the "???" expr
					removePredicate(piter); // remove current predicate
					replaceOperand(var, *expr); // [??? / var]
					DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, expr, &var))
					rtn = true;
					break; // stop and move on
				}
			}
		}
	}

	if(!rtn) // no X expression has been found to match ?3 = X, thus we have to remove every occurrence of ?3
	{
		for(PredIterator piter(*this); piter; )
		{
			if(piter.pred().involvesOperand(var))
			{
				DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << *piter)
				removePredicate(piter);
				rtn = true;
			}
			else piter++;
		}
	}

	if(invalidate_constant_info)
		constants.invalidateOperand(var);
	return rtn;
}*/

