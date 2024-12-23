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
 * Micro analysis: defines how we parse a BasicBlock
 */

#include <otawa/cfg/BasicBlock.h>
#include <otawa/sem/inst.h>
#include <otawa/sem/PathIter.h>
#include <elm/string/String.h>
#include "struct/predicate.h"
#include "struct/operand.h"
#include "debug.h"
#include "analysis_state.h"
#include "v2/analysis_sem2.h"

using namespace otawa::sem;

/**
 * @brief      Process a BasicBlock
 *
 * @param[in]  bb             The BasicBlock to parse
 * @param[in]  flags		  The flags
 * @param[in]  vm    		  The var maker
 */
void Analysis::State::processBB(const BasicBlock *bb, VarMaker& vm, int flags)
{
	DBG("Processing " << (otawa::Block*)bb << " (" << bb->address() << ") of path " << dumpPath())
	elm::List<LabelledPredicate> generated_preds_before_condition;
	generated_preds.clear();
	generated_preds_taken.clear();
	
	short inst_id = 0;
	// parse assembly instructions
	for(BasicBlock::InstIter insts(bb); insts; insts++, inst_id++)
	{
		//DBG(color::BIPur() << *insts)
		DBG(color::Pur() << *insts)
		sem::Block block;
		insts->semInsts(block);
		
		sem::inst last_condition(NOP);
		PathIter seminsts;
		SemanticParser semp(*this, vm);
		// parse semantical instructions
		for(seminsts.start(*insts); seminsts; seminsts++)
		{
			DBG(color::IPur() << *seminsts)
			
			if(seminsts.isCond()) // IF
			{	// backup the list of generated predicates before entering the condition
				generated_preds_before_condition.addAll(generated_preds); // side effect: reverses the order of the list
				DBG(color::IBlu() << "(Parsing taken path)")
				last_condition = *seminsts; // save this for later
			}
			if(seminsts.pathEnd()) // CONT
			{ 	// dumping the current generated_preds into the previous_paths_preds list
				invalidateTempVars(); // do the end-of-instruction tempvar invalidation first
				DBG(color::IBlu() << "(Parsing not taken path)")
				generated_preds_taken = generated_preds;
				generated_preds = generated_preds_before_condition;
			}
			if((flags & VERSION) == 1)
				processSemInst1(seminsts, last_condition);
			else if(semp.process(seminsts) != 0)
			{
				wipeMemory(vm);
				setMemoryInitPoint(bb, inst_id);
			}
		}
		// all temporary variables are freed at the end of any assembly instruction, so invalidate them
		invalidateTempVars();
	}
	if( !((flags&VERSION) == 1) && (flags&CLAMP_PREDICATE_SIZE) )
		clampPredicates(vm);
	// if(! (dbg_flags & DBG_DETERMINISTIC))
	// 	DBG("dag:" << *dag)
	if(dbg_verbose == DBG_VERBOSE_ALL)
	{
		if(generated_preds_taken)
		{
			DBG("Predicates generated: ")
			DBG("|-> taken path: " << generated_preds_taken)
			DBG("|-> not taken path: " << generated_preds)
		}
		else
			DBG("Predicates generated: " << generated_preds)
#ifdef V1
		DBG("Constants updated: " << constants.printChanges()) // TODO!! check that a mess doesn't happen with constants being updated by both taken and not taken path
#endif
	}
	DBG(dumpEverything());
	ASSERTP(!(flags & SP_CRITICAL) || lvars(context->sp).isConstant(), "SP value was lost: " << lvars(context->sp) << ", aborting");
}

/**
 * @brief      Try to replace constants in the predicate, update labels and pretty print accordingly, then return the final labelled predicate ready to be added to the list
 *
 * @param      opr     The operator
 * @param      opd1    The first operand
 * @param      opd2    The second operand
 * @param      labels  Labels to add to the generated predicate
 *
 * @return     The generated LabelledPredicate
 */
LabelledPredicate Analysis::State::makeLabelledPredicate(condoperator_t opr, const Operand* opd1, const Operand* opd2, Path& labels) const
{
	ASSERT(opd1 && opd2);
	Vector<OperandVar> replaced_vars;
#ifdef V1
	const ConstantVariablesCore& cvc = static_cast<ConstantVariablesCore>(constants);
	// If we have predicates such as ?16 = ?4 ~ t1, make sure none of these are identified as constants in the constantVariables table!
	opd1 = opd1->replaceConstants(*dag, cvc, replaced_vars);
	opd2 = opd2->replaceConstants(*dag, cvc, replaced_vars);
	if(dbg_verbose == DBG_VERBOSE_ALL && replaced_vars) // pretty printing of replaceConstants
	{
		int count = 0;
		elm::String str = "";
		for(Vector<OperandVar>::Iter iter(replaced_vars); iter; iter++)
		{
			if(count++)
				str = _ << str << ", ";
			str = _ << str << *iter << ":" << constants[*iter];
			if(constants.getLabels(*iter))
				str = _ << str << "[" << pathToString(constants.getLabels(*iter)) << "]";
		}
		DBG(color::IPur() << DBG_SEPARATOR << color::IBlu() << " Replaced constant" << (count>1?"s ":" ") << str)
	}
	updateLabelsWithReplacedConstantsInfo(labels, replaced_vars);
#endif
	Predicate p = Predicate(opr, opd1, opd2);
	DBG(color::IPur() << DBG_SEPARATOR << color::IGre() << " + " << p)
	return LabelledPredicate(p, labels);
}

// warning: if successful, removes a predicate
bool Analysis::State::tryToKeepVar(const OperandVar& var)//, const Predicate*& removed_predicate)
{
	// removed_predicate = NULL;
	// try and identify a value for ?3 (look for a ?3 = X predicate)
	for(PredIterator piter(*this); piter; piter++)
	{
		const Predicate &p = piter.pred();
		if(p.opr() == CONDOPR_EQ)
		{
			const int left_involves_var = p.leftOperand().involvesVariable(var);
			const int right_involves_var = p.rightOperand().involvesVariable(var);
			if(left_involves_var == 1 && right_involves_var == 0) // var is in left operand
			{
				if(p.leftOperand() == var) // left operand is exactly var (that is, predicate is "var = ???")
				{
					const Operand* expr = p.right(); // backup the "???" expr
					// removed_predicate = &p;
					removePredicate(piter); // remove current predicate
					replaceVar(var, expr); // [??? / var]
					DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, &var, expr))
					return true;
				}
			}
			else if(left_involves_var == 0 && right_involves_var == 1) // var is in right operand
			{
				if(p.rightOperand() == var) // right operand is exactly var (that is, predicate is "??? = var")
				{
					const Operand* expr = p.left(); // backup the "???" expr
					// removed_predicate = &p;
					removePredicate(piter); // remove current predicate
					replaceVar(var, expr); // [??? / var]
					DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, expr, &var))
					return true;
				}
			}
		}
	}
	return false;
}

// returns true if a variable has been invalidated
bool Analysis::State::invalidateVar(const OperandVar& var, bool invalidate_constant_info)
{
	// const Predicate* removed_predicate; // throw this away
	bool rtn = tryToKeepVar(var);//, removed_predicate);
	if(!rtn) // no X expression has been found to match ?3 = X, thus we have to remove every occurrence of ?3
	{
		for(PredIterator piter(*this); piter; )
		{
			if(piter.pred().involvesVariable(var))
			{
				DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << *piter)
				removePredicate(piter);
				rtn = true;
			}
			else piter++;
		}
	}

#ifdef V1
	if(invalidate_constant_info)
		constants.invalidate(var);
#endif
	return rtn;
}

/**
 * @brief This tries to find a matching OperandMem for the provided OperandVar, then calls invalidateMem(OperandMem)
 */
bool Analysis::State::invalidateMem(const OperandVar& var)
{
	Path labels_to_throw;
	Option<OperandMem> addr = getOperandMem(var, labels_to_throw);
	if(!addr)
	{
		DBG(color::IPur() << DBG_SEPARATOR " " << color::IYel() << "Could not simplify " << var << ", invalidating whole memory")
		return invalidateAllMemory();
	}
	return invalidateMem(*addr);
}

/**
 * @brief Replace every occurrence of the OperandMem parameter by a constant value if possible, removes the occurrences from the predicate lists otherwise
 */
bool Analysis::State::invalidateMem(const OperandMem& addr)
{
	//return invalidate(addr); // TODO! why was this here?
	Path labels;
	if(Option<Constant> maybe_val = findConstantValueOfMemCell(addr, labels))
		return replaceMem(addr, dag->cst(*maybe_val), labels); // try to keep the info
	bool rtn = false;
	for(PredIterator piter(*this); piter; )
	{
		if(piter.pred().involvesMemoryCell(addr))
		{
			DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << *piter)
			removePredicate(piter);
			rtn = true;
		}
		else piter++;
	}
	return rtn;
}

/**
 * @brief This function will try to keep the information contained in the tempvars by replacing them by their values in other predicates before removing them
 */
bool Analysis::State::invalidateTempVars()
{
	bool loop, rtn = false;

#ifdef V1
	rtn |= constants.invalidateTempVars();
#endif
	lvars.resetTempVars();

	// First step: try and replace everything we can
	do {
		loop = false;
		for(elm::List<LabelledPredicate>::Iter iter(generated_preds); iter; iter++)
		{
			if(iter->pred().countTempVars())
			{
				OperandVar temp_var(0);
				Operand const* expr = NULL;
				if(iter->pred().getIsolatedTempVar(temp_var, expr)) // returns false if several tempvars
				{
					// try to keep the info
					rtn |= replaceTempVar(temp_var, expr);
					//*then remove the predicate
					DBG(color::IYel() << "- " << iter->pred())
					generated_preds.remove(iter);
					loop = true;
					break;
				}
			}
		}
	} while(loop);
	
	// Second step: remove everything that is left and still contains a tempvar
	for(elm::List<LabelledPredicate>::Iter iter(generated_preds); iter; )
	{
		if(iter->pred().countTempVars())
		{	// remove the predicate
			DBG(color::IYel() << "- " << iter->pred())
			generated_preds.remove(iter);
			continue;
		}
		iter++;
	}
	return rtn;
}

int Analysis::State::invalidateStackBelow(const Constant& stack_limit)
{
	int count = 0;
	ASSERT(stack_limit.isRelative());
	// do not try to replace everything unlike invalidateTempVars - the point is to get rid of useless obsolete data
	// wait what... no we sometimes have A = B and B >= 5, gotta keep A >= 5 if B is irrelevant
	for(PredIterator piter(*this); piter; piter++) //)
	{
		if(piter.pred().involvesStackBelow(stack_limit))
		// while(const Option<Constant>& addr_involved = piter.pred().involvesStackBelow(stack_limit))
		{
			removePredicate(piter);
			count++;
			continue;
		}
			/*
			// const Predicate* removed_predicate = NULL;
			bool replaced_predicates = false;
			const OperandMem opd_to_remove(OperandConst(addr_involved));

			// reparse the predicates to look for some way to keep this damned opd_to_remove operand
			for(PredIterator ppiter(*this); ppiter; ppiter++)
			{
				const Predicate &p = ppiter.pred();
				if(p.opr() == CONDOPR_EQ)
				{
					const int left_involves_opd = p.leftOperand().involvesOperand(opd_to_remove);
					const int right_involves_opd = p.rightOperand().involvesOperand(opd_to_remove);
					// This could really be improved with some basic arithmetic
					if(left_involves_opd == 1 && right_involves_opd == 0) // var is in left operand
					{
						if(p.leftOperand() == opd_to_remove) // left operand is exactly var (that is, predicate is "var = ???")
						{
							Operand* expr = p.rightOperand().copy(); // backup the "???" expr
							removePredicate(ppiter); // remove current predicate
							replaceVar(opd_to_remove, *expr); // [??? / var]
							DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, var, *expr))
							delete expr;
							replaced_predicates = true;
							break;
						}
					}
					else if(left_involves_opd == 0 && right_involves_opd == 1) // var is in right operand
					{
						if(p.rightOperand() == opd_to_remove) // right operand is exactly var (that is, predicate is "??? = var")
						{
							Operand* expr = p.leftOperand().copy(); // backup the "???" expr
							removePredicate(ppiter); // remove current predicate
							replaceVar(opd_to_remove, *expr); // [??? / var]
							DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << Predicate(CONDOPR_EQ, *expr, var))
							delete expr;
							replaced_predicates = true;
							break;
						}
					}
				}
			}

			if(replaced_predicates)
			{
				count++;
				qskldjsjklfjkdsfjksdlfs;
				// recursive call... what's the use of the while actually? cause we could have removed the current predicate, fuck
			}
		}
		if(should_iterate)
			piter++;*/
	}
	DBG(color::IYel() << "- " << count << " predicates")
	return count;
}

// this mindlessly replaces all occurrences of var by expr
bool Analysis::State::replaceVar(const OperandVar& var, const Operand* expr)
{
	bool rtn = false;
	for(PredIterator piter(*this); piter; )
	{
		if(piter.pred().involvesVariable(var))
		{
			Predicate p = piter.pred();
			String prev_str = _ << piter.pred();
			if(p.update(*dag, dag->var(var), expr)) // something updated
			{
				if(rtn == false) // first time
				{
					DBG(color::IBlu() << "[" << *expr << " / " << var << "]")
					rtn = true;
				}
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::Cya()  << "- " << prev_str)
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "+ " << p)
				setPredicate(piter, LabelledPredicate(p, piter.labels()));
				movePredicateToGenerated(piter); // this does piter++
				continue;
			}
		}
		piter++;
	}
	return rtn;
}

// this mindlessly replaces all occurrences of var by expr
/*bool Analysis::State::replaceOperand(const Operand& opd, const Operand& expr)
{
	bool rtn = false;
	for(PredIterator piter(*this); piter; )
	{
		if(piter.pred().involvesOperand(opd))
		{
			Predicate p = piter.pred();
			String prev_str = _ << piter.pred();
			if(p.update(opd, expr)) // something updated
			{
				if(rtn == false) // first time
				{
					DBG(color::IBlu() << "[" << expr << " / " << opd << "]")
					rtn = true;
				}
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::Cya()  << "- " << prev_str)
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "+ " << p)
				setPredicate(piter, LabelledPredicate(p, piter.labels()));
				movePredicateToGenerated(piter); // this does piter++
				continue;
			}
		}
		piter++;
	}
	return rtn;
}*/

/**
 * @brief      Replaces all occurences of a temp var by some expr
 *
 * @param      temp_var  The temporary variable
 * @param      expr      The expression to replace it with
 *
 * @return     True if something was replaced, False otherwise
 */
bool Analysis::State::replaceTempVar(const OperandVar& temp_var, const Operand* expr)
{
	bool rtn = false;
	for(elm::List<LabelledPredicate>::Iter iter(generated_preds); iter; iter++)
	{
		if(iter->pred().involvesVariable(temp_var))
		{
			Predicate p(iter->pred());
			String prev_str = _ << p;			
			p.update(*dag, dag->var(temp_var.addr()), expr);
			if(!p.isIdent()) // if we just transformed t1 = X into X = X, don't add to generated_preds
			{
				if(rtn == false) // first time
				{
					DBG(color::IBlu() << "[" << *expr << " / " << temp_var << "]")
					rtn = true;
				}
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::Cya()  << "- " << prev_str)
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "+ " << p)
				generated_preds.set(iter, LabelledPredicate(p, iter->labels()));
			}
		}
	}
	return rtn;
}

/**
 * @fn bool Analysis::State::replaceMem(const OperandMem& opdm, const Operand* expr, const Path& labels)
 * @brief replaces all occurrences of opdm in all predicates by expr + append "labels"
 */
bool Analysis::State::replaceMem(const OperandMem& opdm, const Operand* expr, const Path& labels)
{
	bool rtn = false;
	elm::String prev_str;
	for(elm::List<LabelledPredicate>::MutableIterator iter(generated_preds); iter; )
	{
		if(dbg_verbose == DBG_VERBOSE_ALL)
			prev_str = _ << iter.item().pred();
		if(iter.item().updatePred(*dag, dag->mem(opdm), expr))
		{
			iter.item().addLabels(labels);
			if(rtn == false) // first time
			{
				DBG(color::IBlu() << "[" << *expr << " / " << opdm << "]")
				rtn = true;
			}
			rtn = true;
			DBG(color::IBlu() << DBG_SEPARATOR " " << color::Cya()  << "- " << prev_str)
			if(iter.item().pred().isIdent())
			{
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "  (not adding identity)")				
				generated_preds.remove(iter);
			}
			else 
			{
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "+ " << iter.item())
				iter++;
			}
		}
		else iter++;
	}
	for(elm::List<LabelledPredicate>::MutableIterator iter(labelled_preds); iter; )
	{
		if(dbg_verbose == DBG_VERBOSE_ALL)
			prev_str = _ << iter.item();
		if(iter.item().updatePred(*dag, dag->mem(opdm), expr))
		{
			iter.item().addLabels(labels);
			if(rtn == false) // first time
			{
				DBG(color::IBlu() << "[" << *expr << " / " << opdm << "]")
				rtn = true;
			}
			rtn = true;
			if(!iter.item().pred().isIdent())
			{
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::Cya()  << "- " << prev_str)
				DBG(color::IBlu() << DBG_SEPARATOR " " << color::ICya() << "+ " << iter.item())
				generated_preds += iter.item(); // move to generated preds
			}
			labelled_preds.remove(iter);
		}
		else iter++;
	}
	return rtn;
}

// returns true if a variable has been updated
// second operand is usually an OperandArith
// WARNING: it's important to do this only after constants have been updated, because opd_modifier WILL be replaced with constants
bool Analysis::State::update(const OperandVar& opd_to_update, const Operand* opd_modifier, Path& labels)
{
	// for example instead of doing [?13+t1/?13], do [?13+4/?13]
	Vector<OperandVar> replaced_vars;
#ifdef V1
	opd_modifier = opd_modifier->replaceConstants(*dag, static_cast<ConstantVariablesCore>(constants), replaced_vars);
	updateLabelsWithReplacedConstantsInfo(labels, replaced_vars);
#endif

	// amongst other things this can simplify constant arithopr such as 8+(4-2)
	if(Option<const Operand*> opd_modifier_simplified = opd_modifier->simplify(*dag))
	{
		DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " Simplified " << *opd_modifier << " to " << **opd_modifier_simplified)
		opd_modifier = opd_modifier_simplified;
	}

#	ifdef NO_UTF8
		DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << opd_to_update << " -> " << *opd_modifier << "]")
#	else
		DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " [" << opd_to_update << " → " << *opd_modifier << "]")
#	endif
	
	bool rtn = false;
	for(PredIterator piter(*this); piter; piter++)
	{
		if(piter.pred().involvesOperand(opd_to_update))
		{
			DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " " DBG_SEPARATOR << color::Cya() << " - " << *piter)
			
			// updating the predicate
			Predicate p = piter.pred();
			p.update(*dag, dag->var(opd_to_update.addr()), opd_modifier);

			LabelledPredicate lp = LabelledPredicate(p, piter.labels());
			setPredicate(piter, lp);
			DBG(color::IPur() << DBG_SEPARATOR << color::Blu() << " " DBG_SEPARATOR << color::ICya() << " + " << *piter)
			movePredicateToGenerated(piter);
			rtn = true;
		}
	}
	return rtn;
}

/**
 * @fn bool Analysis::State::invalidateAllMemory()
 * @brief      Invalidate all predicates involving memory
 * @rerturn    True if something was invalidated, False otherwise
 */
bool Analysis::State::invalidateAllMemory()
{
	bool rtn = false;
	for(PredIterator piter(*this); piter; )
	{
		if(piter.pred().involvesMemory())
		{
			DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << *piter)
			removePredicate(piter);
			rtn = true;
		}
		else
			piter++;
	}
	return rtn;
}

#ifdef V1
/*
 * Operand::replaceConstants returns a Vector of replaced constant variables,
 * then we have to process ConstantVariables constants to find the matching labels and add them to the labels list
 */
void Analysis::State::updateLabelsWithReplacedConstantsInfo(Path& labels, const Vector<OperandVar>& replaced_vars) const
{
	for(Vector<OperandVar>::Iter iter(replaced_vars); iter; iter++)
		labels += constants.getLabels(*iter);
}
#endif

/**
 * @brief      warning: must be called with a suppported condition
 *
 * @param      kind       The condition kind
 * @param      opd_left   The first operand
 * @param      opd_right  The second operand
 * @param      taken      Taken branch or not?
 *
 * @return     The conditional predicate.
 */
Predicate Analysis::State::getConditionalPredicate(cond_t kind, const Operand* opd_left, const Operand* opd_right, bool taken)
{
	condoperator_t opr;
	bool reverse = false;
	if(!taken)
		kind = invert(kind); // kind = !kind
	switch(kind) // TODO: handle unsigned better
	{
		case EQ:
			opr = CONDOPR_EQ;
			break;
		case ULT:
		case LT:
			opr = CONDOPR_LT;
			break;
		case ULE:
		case LE:
			opr = CONDOPR_LE;
			break;
		case UGE:
		case GE:
			opr = CONDOPR_LE;
			reverse = true;
			break;
		case UGT:
		case GT:
			opr = CONDOPR_LT;
			reverse = true;
			break;
		case NE:
			opr = CONDOPR_NE;
			break;
		default: // NO_COND, ANY_COND, MAX_COND
			DBGW("unsupported: " << kind); // invalid condition, exit
			return Predicate(CONDOPR_EQ, Top, Top); // TODO! hax
	}
	if(reverse)
		return Predicate(opr, opd_right, opd_left);
	else
		return Predicate(opr, opd_left, opd_right);
}

/**
 * @brief Check if addr_mem is the constant address of some read-only data, if so returns the constant data value
 */
Option<Constant> Analysis::State::getConstantValueOfReadOnlyMemCell(const OperandMem& addr_mem, otawa::sem::type_t type) const
{
	const Constant& addr = addr_mem.addr();
	if(!addr.isAbsolute())
		return none;
	if(!context->dfa_state->isInitialized(addr.val()))
		return none;

	#define RETURN_CONSTANT(type)\
	{\
		type v;\
		context->dfa_state->get(addr.val(), v);\
		return Constant(v);\
	}
	switch(type)
	{
		case INT8: // TODO: ok?
		case UINT8:
			RETURN_CONSTANT(t::uint8);
		case INT16:
		case UINT16:
			RETURN_CONSTANT(t::uint16);
		case INT32:
		case UINT32:
			RETURN_CONSTANT(t::uint32);
		case INT64: 
		case UINT64:
			RETURN_CONSTANT(t::uint64);
		case FLOAT32:
			RETURN_CONSTANT(float);
		case FLOAT64:
			RETURN_CONSTANT(double);
		case MAX_TYPE:
		case NO_TYPE:
			return none;
	}
	#undef RETURN_CONSTANT
	return none;
}

/**
 * @brief      Returns an overestimation of the size of a type
 *
 * @param      type  The type
 *
 * @return     The size of the type.
 */
int Analysis::State::getSizeOfType(otawa::sem::type_t type) const
{
	switch(type)
	{
		case INT8:
		case UINT8:
			return 1;
		case INT16:
		case UINT16:
			return 2;
		case INT32:
		case UINT32:
		case FLOAT32:
			return 4;
		case INT64:
		case UINT64:
		case FLOAT64:
			return 4;
		case MAX_TYPE:
		case NO_TYPE:
		default:
			return 8;
	}
}
