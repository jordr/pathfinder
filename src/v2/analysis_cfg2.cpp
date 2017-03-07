/**
 * v3
 */

#include <elm/data/util.h> // requires elm v2
#include <otawa/cfg/Edge.h>
#include "analysis2.h"
#include "../analysis_states.h"
#include "../progress.h"
#include "../assert_predicate.h"
#include "../struct/var_maker.h"

using namespace elm::io;

/**
 * @fn void Analysis2::processCFG(Block* entry);
 * @brief Runs the analysis with no virtualization
*/
void Analysis2::processCFG(CFG* cfg, bool use_initial_data)
{
	ASSERT(! (flags&VIRTUALIZE_CFG));
	DBGG(IPur() << "==>\"" << cfg->name() << "\"")
	
	WorkingList wl;
	VarMaker* vm_backup = vm;
	vm = new VarMaker();
/* begin */
	/* for e ∈ E(G) */
		/* s_e ← nil */
	/* for h ∈ H(G) */
		/* s_h ← nil */
		/* status_h ← ENTER */
	/* for e ∈ entry.outs */
	for(Block::EdgeIter e(cfg->entry()->outs()); e; e++)
	{
		/* s_e ← T */
		LockPtr<States> s_entry(new States());
		s_entry->push(topState(cfg->entry()));
		if(use_initial_data)
			s_entry->states()[0].initializeWithDFA();
		EDGE_S(*e) = s_entry;
		/* wl ← sink(e) */
		wl.push((*e)->target()); // only one outs, firstBB.
	}

	/* while wl ≠ {} do */
	while(!wl.isEmpty())
	{
		/* b ← pop(wl) */
		Block *b = wl.pop();
		/* pred ← 	b.ins \ B(G) if b ∈ H(G) ∧ status_b = ENTER */
		/* 			b.ins ∩ B(G) if b ∈ H(G) ∧ status_b ∈ {FIX, LEAVE} */
		/* 			b.ins 		 if b ∈/ H(G) */
		const Vector<Edge*> pred(LOOP_HEADER(b) ? (loopStatus(b) == ENTER
				? nonBackIns(b) /* if b ∈ H(G) ∧ status_b = ENTER */
				: backIns(b) /* if b ∈ H(G) ∧ status_b ∈ {FIX, LEAVE} */
			) : allIns(b) /* if b ∉ H(G) */
		);

		/* if ∀e ∈ pred, s_e ≠ nil then */
		if(allEdgesHaveTrace(pred))
		{
			/* s ← |_|e∈pred s_e */
			LockPtr<States> s = join(pred);

			/* for e ∈ pred */
			for(Vector<Edge*>::Iter e(pred); e; e++)
				/* s_e ← nil */
				EDGE_S.remove(e);
			
			/* succ ← b.outs */
			bool propagate = true;
			/* if b ∈ H(G) then */
			if(LOOP_HEADER(b))
			{
				ASSERT(s->count() <= 1);
				/* if status_b = LEAVE then */
				if(loopStatus(b) == LEAVE) /////
				{
					/* if ∃e ∈ b.ins | s_e = nil then */
					if(anyEdgeHasTrace(b->ins()))
						/* wl ← wl ∪ {b} */
						wl.push(b);
					/* s_b ← nil */
					LH_S.remove(b);
					/* succ ← {} */
					propagate = false;
				}
				else /* else s_b ← s */
					LH_S(b) = s->one();
				switch(loopStatus(b))
				{
					/* status_b ← FIX if status_b = ENTER */
					case ENTER:
						LH_STATUS(b) = FIX;
						LH_S0(b) = s->one();
						//s->onLoopEntry(b);
						break;
					/* status_b ← ACCEL if status_b = FIX ∧ s ≡ s_b */
					case FIX: if(s->one().equiv(LH_S(b)))
					{
						LH_STATUS(b) = ACCEL;
						s->prepareFixPoint();
					}
						break;
					/* ... */
					case ACCEL:
						LH_STATUS(b) = LEAVE;
						s->widening(loopIterOpd(b));
						break;
					/* status_b ← ENTER if status_b = LEAVE */
					case LEAVE:
						LH_STATUS(b).remove();
						break;
				}
			}
			I(b, s); // update s
			/* for e ∈ succ \ {EX_h | b ∈ L_h ∧ status_h =/ LEAVE} */
			const Vector<Edge*> succ(propagate ? outsWithoutUnallowedExits(b) : nullVector<Edge*>());
			for(Vector<Edge*>::Iter e(succ); e; e++)
			{
				/* s_e ← I*[e](s) */
				EDGE_S(e) = Analysis::I(e, s);
				for(LoopExitIterator l(*e); l; l++)
					EDGE_S(e)->appliedTo(LH_S0(*l), *vm);
// DBGG(color::IRed() << cfg->name() << ".vm = " << *vm << " -- " << vm)
				/* ips ← ips ∪ ipcheck(s_e , {(h, status_h ) | b ∈ L_h }) */
				if(inD_ip(e))
					ip_stats += ipcheck(*EDGE_S.ref(e), infeasible_paths);
				/* wl ← wl ∪ {sink(e)} */
				wl.push(outsAlias(e->sink()));
			}
		}
	}
/* end */
	DBG(cfg->name() << ".vm = " << *vm << " (" << vm << ")")
	DBGG(IPur() << "<==\"" << cfg->name() << "\"")
	CFG_S(cfg)->minimize(*vm, flags&CLEAN_TOPS); // reduces the VarMaker to the minimum
	CFG_VARS(cfg) = LockPtr<VarMaker>(vm);
	vm = vm_backup;
	ASSERTP(elm::forall(States::Iter(**CFG_S(cfg)), SPCanEqual(), static_cast<const OperandConst*>(dag->cst(SP))),
		context.sp << " is definitely not SP+0. " << Dim() << "(" << cfg->name() << ")" << RCol());
	if(flags&ASSUME_IDENTICAL_SP)
		CFG_S(cfg)->resetSP();
}

/**
 * @brief      Interpretation function of a Block
 */
void Analysis2::I(Block* b, LockPtr<States> s)
{
	if(flags&SHOW_PROGRESS)
		progress->onBlock(b);
	if(b->isBasic())
	{
		DBGG(Bold() << "-\tI(b=" << b << ") " << NoBold() << IYel() << "x" << s->count() << RCol() << printFixPointStatus(b))
		for(States::Iter si(s->states()); si; si++)
			(*s)[si].processBB(b->toBasic(), *vm, flags);
	}
	else if(b->isEntry())
		s->onCall((*getCaller(b->cfg()))->toSynth());
	else if(b->isCall())
	{
		CFG* called_cfg = b->toSynth()->callee();
		if(!CFG_S.exists(called_cfg)) // if the called CFG has not been processed yet
			processCFG(called_cfg, false);

		// merging tops
		DBGG("Importing " << CFG_VARS(called_cfg)->length() << " tops from " << called_cfg->name() << "...")
		vm->import((const VarMaker&)**CFG_VARS(called_cfg));

		// working on the paths
		s->onCall(b->toSynth());
		s->apply(**CFG_S(called_cfg), *vm, true);
		s->onReturn(b->toSynth());
	}
	else if(b->isExit()) // main
		CFG_S(b->cfg()) = s; // we will never free this, which shouldn't be a problem because it should only be freed at the end of analysis
	else
		DBGW("not doing anything");
}
