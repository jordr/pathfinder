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
 * General analysis::state methods
 */

#include "struct/arith.h"
#include "analysis_states.h"
#include "compositor.h"
#include "cfg_features.h"
#include "widenor.h"

/**
 * @class Analysis::State
 * @author Jordy Ruiz 
 * @brief Abstract state of a set of path of the program
 */
const Analysis::State bottom(true);

// bottom=false: it's bot. bottom=true: it's top
Analysis::State::State(bool bottom) : context(NULL), dag(NULL), lvars(), mem(53), memid(NULL, 0), bottom(bottom), path()
#ifdef V1
	, constants()
#endif
	{ }

Analysis::State::State(Edge* entry_edge, const context_t& context_, DAG* dag, bool init)
	: context(&context_), dag(dag), lvars(*dag, context->max_tempvars, context->max_registers)
	, mem(53), memid(NULL, 0), bottom(false), path(entry_edge ? entry_edge->target()->cfg() : NULL)
#ifdef V1
	, constants(context->max_tempvars, context->max_registers)
#endif
{
	generated_preds.clear(); // generated_preds := [[]]
	labelled_preds.clear(); // labelled_preds := [[]]
	if(init)
	{
		path.addLast(entry_edge);
#ifdef V1
		constants.set(context->sp, SP, Set<Edge*>::null, false); // set that ?13==SP (since SP is the value of ?13 at the beginning of the program)
#endif
		set(context->sp, dag->cst(SP));
	}
}

Analysis::State::State(const State& s)
	: context(s.context), dag(s.dag), lvars(s.lvars), mem(s.mem), memid(s.memid), bottom(s.bottom), path(s.path),
#ifdef V1
	constants(s.constants),
#endif
	  labelled_preds(s.labelled_preds), generated_preds(s.generated_preds), generated_preds_taken(s.generated_preds_taken)//, fixpoint(s.fixpoint)
	{ }

void Analysis::State::appendEdge(Edge* e)
{
	// add edge to the end of the path
	this->path.addLast(e);
	// we now need to label the correct list of predicates
	elm::List<LabelledPredicate> &relevant_preds = (isConditional(e->source()) && e->isTaken())
		? generated_preds_taken // conditional TAKEN
		: generated_preds; // non-conditional, NOT TAKEN
	labelled_preds += labelPredicateList(relevant_preds, e); // label our list of predicates with the current edge then append it
#ifdef V1
		constants.label(e); // label the constants as well
#endif
	relevant_preds.clear(); // TODO!! this is too strong in case of multiple not taken...
	// v2
	lvars.onEdge(e);
}


// PredIterator
void Analysis::State::setPredicate(PredIterator &iter, const LabelledPredicate &labelled_predicate)
{
	ASSERT(!iter.ended());
	if(iter.state == PredIterator::GENERATED_PREDS)
		generated_preds.set(iter.gp_iter, labelled_predicate);
	else if(iter.state == PredIterator::LABELLED_PREDS)
		labelled_preds.set(iter.lp_iter, labelled_predicate);
	else
		ASSERTP(false, "Analysis::setPredicate(): unhandled iter.state!")
}

/**
 * @fn void Analysis::State::movePredicateToGenerated(PredIterator &iter);
 * @brief Move predicates to the local list (so that they get updated with an edge)
 * @warning this may result in iter.ended()!
**/
void Analysis::State::movePredicateToGenerated(PredIterator &iter)
{
	if(iter.state == PredIterator::GENERATED_PREDS)
		return; // do not do anything
	else if(iter.state == PredIterator::LABELLED_PREDS)
	{
		generated_preds += *iter;
		labelled_preds.remove(iter.lp_iter);
		iter.updateState();
	}
	else ASSERTP(false, "Analysis::movePredicateToGenerated(): unhandled iter.state!")
}

void Analysis::State::removePredicate(PredIterator &iter)
{
	ASSERT(!iter.ended());
	if(iter.state == PredIterator::GENERATED_PREDS)
		generated_preds.remove(iter.gp_iter);
	else if(iter.state == PredIterator::LABELLED_PREDS)
		labelled_preds.remove(iter.lp_iter);
	else ASSERTP(false, "Analysis::removePredicate(): unhandled iter.state!")
	iter.updateState();
}

elm::List<LabelledPredicate> Analysis::State::labelPredicateList(const elm::List<LabelledPredicate>& pred_list, Edge* label)
{
	elm::List<LabelledPredicate> rtn;
	for(elm::List<LabelledPredicate>::Iter iter(pred_list); iter; iter++)
	{
		LabelledPredicate lp(*iter);
		lp.addLabel(label);
		rtn += lp;
	}
	return rtn;
}

io::Output& Analysis::State::print(io::Output& out) const
{
	// out << ":" << labelled_preds << "/" << constants;
	if(!isBottom())
		// return (out << getPathString()); // TODO!
		return (out << path);
	else
#		ifndef NO_UTF8
			return (out << "⊥");
#		else
			return (out << "(bottom)");
#		endif
}


/**
 * @brief      This function is *this -> s -> s o *this, state composition. Updates current state. Updates path.
 * @param  s   state to apply
 * @param  vm  The VarMaker
 * @param local_sp This tells us whether we should scale the SP or not
 * @param clear_path Clear the path of the state
 */
void Analysis::State::apply(const State& s, VarMaker& vm, bool local_sp, bool clear_path)
{
	// DBG("f="<<this->dumpEverything() << ",\ng = " << s.dumpEverything())
	Compositor cc(*this, local_sp);

	const bool mem_was_reset = s.memid.b != NULL;
	bool wipe_memory;
	{
		ASSERT(s.isValid())
		bool sp_was_lost = !s.lvars[context->sp];
		sp_was_lost = sp_was_lost || !s.lvars[context->sp]->accept(cc)->isAConst();
		wipe_memory = sp_was_lost || mem_was_reset;
	}
	applyPredicates(s, cc, wipe_memory);
	// nothing to do for our predicates actually!
	//for(MutablePredIterator pi(*this); pi; pi++)

	// applying lvars
	// this=f, s=g
	DBG("f = " << *this << ", g = " << s)
	// goal is lv = g o f
	LocalVariables lv(lvars); // we need some temporary to handle cases like [r0 -> r1, r1 -> r0]
	for(LocalVariables::Iter i(s.lvars); i; i++)
	{
		if(s.lvars[i] != NULL) // g[i] was modified
		{
			ELM_DBGV(1, "\tf°g(" << *i << ") = " << "f(" << *s.lvars[i] << ") = ")
			lv[i] = s.lvars[i]->accept(cc); // needs more info from f...
			if(dbg_verbose == DBG_VERBOSE_ALL)
				cout << *lv[i] << endl;
		}
		// else // g[i] is identity
	}
	DBG("")

	// applying memory
	// goal is mem = n o m with n = s.mem
	// ASSERTP(lvars[context->sp], "weird... should always be set") // this happens when we lose SP within a loop
	{
		const bool check_wipe_memory = s.memid.b != NULL || !lvars[context->sp] || !lvars[context->sp]->isAConst();
		ASSERT(check_wipe_memory == wipe_memory) // TODO remove
	}

	// ASSERTP(!(sp_was_lost && lvars[context->sp]->isConstant()), "more simplifications required: " << *lvars[context->sp])
	if(wipe_memory)
	{
		static CFG* last_fun_warning = NULL;
		if(s->path.function() != last_fun_warning)
		{
			if(mem_was_reset)
				DBGW("can't use mem data from \"" << s->path.function() << "\" because mem was reset at " << (Block*)s.memid.b)
			else
				DBGW("can't use mem data from \"" << s->path.function() << "\" because sp is " << lvars(context->sp))
			last_fun_warning = s->path.function();
		}
		wipeMemory(vm);
		setMemoryInitPoint(this->path.lastBlock(), 0);
	}
	else
	{
		mem_t m(this->mem); // save local mem
		for(mem_t::PairIterator ni(s.mem); ni; ni++)
		{
			ELM_DBGV(1, "\tf°g([" << (*ni).fst << "]) = ")
			Constant k = (*ni).fst;
			const Operand* updated_addr = dag->cst(k)->accept(cc);
			ASSERTP(updated_addr->isAConst(), "sp not a constant? It should have been checked before")
			k = updated_addr->toConstant();
			if(dbg_verbose == DBG_VERBOSE_ALL)
				elm::cout << "f(g([" << k << "])) = f(" << *(*ni).snd << ") = ";

			m.put(k, (*ni).snd->accept(cc));
			if(dbg_verbose == DBG_VERBOSE_ALL)
				elm::cout << **m[k] << endl;
		}
		// all the ni that are identity are properly handled, because m is initialized with mem
		mem = m;
	}
	lvars = lv;
	// DBG("f o g = " << color::IBlu() << this->dumpEverything())

	// merge path
	if(clear_path)
		this->path.clear();
	this->path.apply(s.getDetailedPath());
}

void Analysis::State::applyPredicates(const State& s, OperandEndoVisitor& cc, bool wipe_memory)
{
	// update their predicates then add them to us
	// be careful about multiplying per negative numbers? or not actually. I think it's a simple substitution
	for(elm::List<LabelledPredicate>::Iter pi(s.labelled_preds); pi; pi++)
	{
		Predicate p = pi->pred();
		if(wipe_memory && p.involvesMemory())
			continue; // Do not add
		LabelledPredicate lp = LabelledPredicate(
				Predicate(p.opr(), p.left()->accept(cc), p.right()->accept(cc)),
				pi->labels()
			);
		DBG(color::IGre() << " + " << lp.pred() << color::Gre() << " {from " << p << "}")
		this->labelled_preds += lp; // then add them
	}
}

// loop analysis should go that way: 1) normal parses with merge & fixpt 2) prepare&parse again 3) accel, parse with SMT ON 4) finalize
/**
 * @brief      This takes the fixpoint state of a loop and prepares it for accelerated iteration computation
 * 			   (so we should merge before this)
 */
void Analysis::State::prepareFixPoint()
{
	for(LocalVariables::Iter i(lvars); i; i++)
		if(lvars[i] && !lvars[i]->isAConst())
			lvars[i] = NULL; // here, the "Top" is the state at the beginning of the loop iteration
	
	Vector<Constant> todel;
	for(mem_t::PairIterator iter(mem); iter; iter++)
		if(! (*iter).snd->isAConst())
			todel.push((*iter).fst);
	for(Vector<Constant>::Iter i(todel); i; i++)
		mem.remove(todel[i]);

	DBGG("prepared fixpoint for accel: " << dumpEverything())
}

/******* widening ********/
// this function considers everything that isn't x0 to be constant in x
const Operand* Analysis::State::widenArithmeticProgression(const Operand* x, const Operand& x0, const Operand* n) const
{
	ASSERT(! x->involves(Top))
	AffineEquationState eqstate(x0);
	x->parseAffineEquation(eqstate); // contains spcount, varcount, and delta
	// const t::int32 a = eqstate.varCounter(), b = eqstate.delta();
	// x_n+1 = ax_n + b
	ASSERTP(eqstate.spCounter() == 0, "strange case where SP is added at every iteration... if this is not a bug, just scratch")
	ASSERTP(eqstate.varCounter() == 1, "lvars[" << x0 << "] = " << *x << ": treat case a != 1: " << eqstate.varCounter())
	return Arith::add(*dag, dag->get(x0), Arith::mul(*dag, n, dag->cst(eqstate.delta()))); // x_n = x0 + b*n
}

/**
 * @fn void Analysis::State::widening(const Operand* n);
 * @brief this = State after one iteration of a prepared fixpoint state (starting from \=/x, x=x0)
 * @param n loop iterator operand
 */
void Analysis::State::widening(const Operand* n)
{
	DBG("Starting widening with: " << dumpEverything())

	WideningProgress wprogress(lvars); // lvars contains size info
	bool fixpoint;
	Widenor widenor(*this, NULL, n);
	do
	{	// TODO!! should include the memory in that loop too
		fixpoint = true;
		DBGG(color::IBlu() << "widening, wprogress = " << wprogress)
		for(LocalVariables::Iter i(lvars); i; i++)
		{
			if(!wprogress[*i])
			{
				if(lvars[i]) // i was modified
				{
					if(Option<const Operand*> xn = widen(lvars[i], *i, n, wprogress, widenor))
					{
						DBG("\tgot " << **xn)
						lvars[i] = *xn;
						wprogress.setVar(*i); // set a flag that says we have accel'd i
						if(*xn != Top)
							fixpoint = false; // a Top wouldn't help handle more predicates
					}
					// else failed to widen
					else
						{ DBG("\tfailed") }
				}
				else // lvars[i] = i
				{
					wprogress.setVar(*i); // nothing to do: identity^n = identity	
					fixpoint = false;
				}
			}
		}
		for(mem_t::PairIterator i(mem); i; i++)
		{
			OperandMem opdm((*i).fst);
			if(!wprogress[opdm])
			{
				if(Option<const Operand*> xn = widen((*i).snd, opdm, n, wprogress, widenor))
				{
					DBG("\tgot " << **xn)
					mem[opdm.addr()] = *xn;
					wprogress.setMem(opdm);
					if(*xn != Top)
						fixpoint = false; // a Top wouldn't help handle more predicates
				}
			}
		}
	} while(!fixpoint);

	// now set to Top all those that could not be replaced
	for(LocalVariables::Iter i(lvars); i; i++)
	{
		if(!wprogress[*i])
		{
			DBG(color::IRed() << "In " << *i << ", could not replace variables of: " << lvars(*i))
			lvars[*i] = Top;
		}
	}
	elm::List<Constant> scratchs;
	for(mem_t::PairIterator i(mem); i; i++)
	{
		if(!wprogress[*i])
		{
			DBG(color::IRed() << "In " << (*i).fst << ", could not replace variables of: " << *(*i).snd)
			// mem[(*i).fst] = Top;
			scratchs += (*i).fst;
		}
	}
	for(elm::List<Constant>::Iter i(scratchs); i; i++)
		mem[*i] = Top;

	DBGG(Blu() << "Widening done, resulting in: " << this->dumpEverything())
}

/**
 * @fn Option<const Operand*> Analysis::State::widen(const Operand* x, const Operand& x0, const Operand* n, const WideningProgress& wprogress, Widenor& widenor) const;
 * @brief      Tries to find the general formula for x_n for: x0_i+1 = x
 *
 * @param      x      The formula for x0 after one iteration
 * @param      x0     The variable that was x before iterating
 * @param      n      The induction variable (that represents I)
 * @param      wprogress  Describes which localvariables have been turned into general form (x_n = ...)
 *
 * @return     None if all dependencies haven't been processed (in wprogress), the x_n otherwise (can be Top)
 */
Option<const Operand*> Analysis::State::widen(const Operand* x, const Operand& x0, const Operand* n, const WideningProgress& wprogress, Widenor& widenor) const
{
	DBG("widen(" << x0 << " <- " << color::Cya() << *x << color::RCol() << ", wprogress=" << wprogress << ")")
	if(*x == x0) // x = x0
		return x; // nothing to do, this is identity
	else if(x->involves(Top))
		return Top;
	else if(x->isAffine(x0) && x->involvesOperand(x0) == 1) // we do not handle stuff like x=2*x yet... we'd need 2^I anyway, ouch
		return widenArithmeticProgression(x, x0, n); // x_n = x0 + b*n
	else if(x->isLinear(true)) // x = y + z - k and such
	{
		bool ready = true; // checks if all dependencies have been cleared
		int x0_count = 0;
		const int nocst_noarith = 0xffff-(1<<CST)-(1<<ARITH);
		for(Operand::Iter j(x, nocst_noarith); j && ready; j++) // this should iter on everything but CST and ARITH
		{
			switch(j->kind())
			{
				case VAR:
				case MEM:
					if(**j == x0)
						x0_count++;
					else // if false, useless to continue looping
						ready &= (j->kind()==VAR ? wprogress[j->toVar()] : wprogress[j->toMem()]) == WideningProgress::GENERAL_FORM;
					break;
				case TOP:
				case ITER:
					ready = false;
					break;
				case CST:
				case ARITH:
					ASSERTP(false, "shouldn't happen, we flagged this out.\n lvars[" << x0 << "] = " << *x << " : j = " << **j);
					break;
			}
		}
		DBG("x = " << *x)

		if(ready)
		{
			widenor.setSelf(&x0); // this will not consider x0 as a pointer from the DAG
			x = x->accept(widenor);
			// at this point everything that isn't x0 is to be considered constant (for ex. if there's ?2 it's the ?2 of the beginning of the loop)
			// we indeed want the x0_count from BEFORE the replacements
			if(x == Top)
				return Top;
			else if(x0_count == 0) // easy case, everything on the right is constant. nothing to do
				return x;
			else if(x0_count == 1)
				return widenArithmeticProgression(x, x0, n); // x_n = x0 + b*n
			else
				ASSERTP(false, "handle multiple this in operand formula")
		}
		else
			return none; // we need to come back here later
	}
	else
	{
		DBG(color::IRed() << x0 << " too complex to widen: " << *x)
		return Top;
	}
}

/**
 * @fn void Analysis::State::setMemoryInitPoint(const otawa::Block* b, short id);
 * @brief      Sets the memory initial point (what the right operands will refer to).
 *
 * @param      b   The block
 * @param      id  The ID of the instruction
 */

/**
 * @brief      morph the OperandIter add predicates about it
 * @warning    in the leave iteration, we may not have i=max until the loop exit edges...
 * @param      n   The operand describing the loop in question
 * @param      vm  The VarMaker
 */
void Analysis::State::finalizeLoop(OperandIter* n, VarMaker& vm)
{
	n->finalize();
}

/**
 * @fn void Analysis::State::merge(const States& ss, Block* b, VarMaker& vm);
 * @brief merge all states into one (a bit brutal). Does not take in account current state. Should not be called with an empty list
 */
void Analysis::State::merge(const States& ss, Block* b, VarMaker& vm)
{
	ASSERTP(!ss.isEmpty(), "merging an empty list of states..."); // maybe just leave the state empty
	DBGG("-\tmerging from " << ss.count() << " state(s).")

	ss.checkForSatisfiableSP(); // check if there is a problem, like if one state has SP+4, another has SP+8

	// resetting stuff
	generated_preds.clear();
	generated_preds_taken.clear();
	labelled_preds.clear();
#ifdef V1
	elm::List<ConstantVariables> cvl;
	constants = ss.first().constants;
#endif
	lvars = ss.first().lvars;
	mem = ss.first().mem;
	memid = ss.first().memid;
	bool wipe_memory = false;
	// const mem_t* mtab[ss.count()];
	// int i = 0;
	// intialize to first element
	// copy firstElement.labelled_preds into labelled_preds with empty labels
	for(elm::List<LabelledPredicate>::Iter iter(ss.first().labelled_preds); iter; iter++)
		labelled_preds += LabelledPredicate(iter->pred(), Path::null);
	bool first = true;
	for(States::Iter siter(ss); siter; siter++)
	{
		if(first) // the first element is s itself, it's useless to merge s with s
		{
			first = false;
			continue;
		}

		// lvars = lvars ∩ siters->lvars
		lvars.merge(siter->lvars);
		// mem = mem ∩ siters->mem
		if(wipe_memory || siter->memid.b != this->memid.b)
			wipe_memory = true;
		else
		{
			const mem_t& smem = siter->mem;
			for(mem_t::PairIterator i(mem); i; i++)
			{
				if((*i).snd != smem.get((*i).fst, NULL)) // for each (k, v) in mem, if smem[k] != v, invalidate mem[k]
					mem[i] = Top;
			}
			for(mem_t::PairIterator i(smem); i; i++)
				if((*i).snd != mem.get((*i).fst, NULL)) // for each (k, v) in smem, if mem[k] != v, invalidate mem[k]
					mem[i] = Top;
		}
#ifdef V1
		cvl += (*siter).constants; // constants.merge(...) uses the info from "constants" so it's useless to add it at the first iteration
#endif
		// for each element of labelled_preds, we make sure it is in *siter
		for(elm::List<LabelledPredicate>::Iter iter(labelled_preds); iter; )
		{
			// do not use 'if(s.labelled_preds.contains(*iter))' as we want to use Predicate::operator== and not LabelledPredicate::operator==
			bool contains = false;
			for(elm::List<LabelledPredicate>::Iter subiter((*siter).labelled_preds); subiter; subiter++)
			{
				if((*subiter).pred() == iter->pred())
				{
					contains = true;
					break;
				}
			}
			if(contains)
				iter++;
			else
				labelled_preds.remove(iter);
		}
	}

#ifdef V1
	this->constants.merge(cvl);
#endif
	// this->path.merge(stateListToPathVector(sc)); // merge paths as well while keeping some flow info and shrink that in this->path
	this->path.clear();
	this->path.fromContext(b);
	if(wipe_memory)
	{
		wipeMemory(vm);
		if(Option<Block*> lb = path->lastBlock())
			setMemoryInitPoint(*lb, 0);
		else
			setMemoryInitPoint(b, 0); // this isn't in the case of an optional merge that wipes memory
	}
}

LoopBound Analysis::State::getLoopBound(const Operand* oi) const
{
	for(PredIterator pi(*this); pi; pi++)
		if(pi->involvesOperand(oi) == 1)
		{
			const Predicate& p = pi->pred();
			if(p.right()->isAConst())
			{
				int k = p.right()->toConstant().val();
				if(p.left() == oi) // if it's just N <(=) 19
				{
					if(p.opr() == CONDOPR_LE)
						return LoopBound(k+1);
					else if(p.opr() == CONDOPR_LT)
						return LoopBound(k);
				}
				else if(p.leftOperand().kind() == ARITH && p.leftOperand().toArith().opr() == ARITHOPR_ADD) // if it's N___ + 1 < 19
				{
					ASSERTP(p.leftOperand().toArith().right() != oi, "handle that case")
					if(p.leftOperand().toArith().left() == oi && p.leftOperand().toArith().rightOperand().isAConst())
					{
						k -= p.leftOperand().toArith().rightOperand().toConstant().val();
						if(p.opr() == CONDOPR_LE)
							return LoopBound(k+1);
						else if(p.opr() == CONDOPR_LT)
							return LoopBound(k);
					}
				}
			}
			if(dbg_flags & DBG_ONLY_LOOP_BOUNDS)
				elm::cout << "Complex Loop bound: " << *pi << endl;
			else
				DBGG("Complex Loop bound: " << *pi)
		}
	return LoopBound();
}

void Analysis::State::collectTops(VarCollector& vc) const
{
	for(LocalVariables::Iter i(lvars); i; i++)
		if(lvars[i])
			lvars[i]->collectTops(vc);
	for(mem_t::Iter i(mem); i; i++)
		i->collectTops(vc);
	for(PredIterator pi(*this); pi; pi++)
		pi->collectTops(vc);
}

/**
 * @brief      Removes all tautologies from lvars/preds AND also predicates that involve T
 */
void Analysis::State::removeTautologies(void)
{
	for(LocalVariables::Iter i(lvars); i; i++)
		if(lvars[i] && *lvars[i] == *i)
			lvars[i] = NULL;
	for(PredIterator piter(*this); piter; )
	{
		if(piter->pred().isTautology() || piter->pred().involvesOperand(*Top))
			removePredicate(piter);
		else
			piter++;
	}
}

/**
 * @fn inline Analysis::State Analysis::topState(Block* entry) const;
 * @brief Returns a Top state
 */
Analysis::State Analysis::topState(otawa::Block* entry) const
{
	return Analysis::State(theOnly(entry->outs()), context, dag, true);
}

void Analysis::State::initializeWithDFA()
{
	for(dfa::State::MemIter mi(context->dfa_state); mi; mi++)
	{
		const dfa::MemCell& mc = *mi;
		if(mc.value().isConst())
			setMem(mc.address().offset(), dag->cst(mc.value().value()));
		// else // let's not handle intervals and CLPs for now
	}
}

elm::String Analysis::State::dumpMem() const
{
	SortedList<MemCell> meml;
	for(mem_t::PairIterator i(mem); i; i++)
		meml.add(MemCell((*i).fst, (*i).snd));

	elm::String rtn = _ << memid << ", [" << endl;
	for(SortedList<MemCell>::Iter i(meml); i; i++)
		rtn = _ << rtn << "        [" << StringFormat(_ << (*i).addr << "]").width(8) << "| " << *(*i).val << endl;
	return rtn << "]";
}

elm::String Analysis::State::dumpEverything() const
{
	return _
		<< "--- DUMPING STATE ---" << endl
		<< "  * path= " << dumpPath() << endl
#ifdef V1
		<< "  * constants= " << constants << endl
#endif
		<< "  * labelled_preds= " << labelled_preds << endl
		<< "  * generated_preds= " << generated_preds << endl
		<< "  * lvars= [" << endl << lvars << "]" << endl
		<< "  * mem= " << dumpMem() << endl
		<< "\t--- END OF DUMP ---";
}

// this is not Leibniz equality, but a test to check for a fixpoint!
// <!> this compares labelled_preds only <!>
bool Analysis::State::equiv(const Analysis::State& s) const
{
	if(s.isBottom())
		return this->isBottom();
	// do not check the path or any of the edges!
#ifdef V1
	if(! this->constants.sameValuesAs(s.constants))
		return false;
#endif
	if(lvars != s.lvars)
		return false;
	// if(this->labelled_preds.count() != s.labelled_preds.count()) // This doesn't work because we sometimes add true values at each iteration
	// 	return false;
	for(elm::List<LabelledPredicate>::Iter self_iter(this->labelled_preds); self_iter; self_iter++)
	{
		bool contains = false;
		for(elm::List<LabelledPredicate>::Iter s_iter(s.labelled_preds); s_iter; s_iter++)
		{
			if(self_iter->pred() == s_iter->pred())
			{
				contains = true;
				break;
			}
		}
		if(!contains)
			return false;
	}
	DBGG("-	" << color::IGre() << "FIXPOINT!")
	DBG(s.dumpEverything())
	return true;
}

/**
 * @fn void Analysis::State::removeConstantPredicates();
 * @brief Removes constant predicates. Useful after a SMT call returning SAT, as the constant predicates of such states must be tautologies
 */
void Analysis::State::removeConstantPredicates()
{
	for(PredIterator piter(*this); piter; )
	{
		if(piter.pred().isConstant())
		{
			DBG(color::IPur() << DBG_SEPARATOR << color::IYel() << " - " << *piter)
			removePredicate(piter);
		}
		else piter++;
	}
}
