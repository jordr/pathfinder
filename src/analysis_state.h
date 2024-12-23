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
 
#ifndef _ANALYSIS_STATE_H
#define _ANALYSIS_STATE_H

#include <elm/avl/Set.h>
#include <elm/data/HashTable.h>
// #include <elm/data/List.h>
#include <otawa/cfg/Edge.h>
#include <otawa/cfg/features.h>
#include <otawa/dfa/State.h> // dfa::State: isInitialized(addr), get(addr, _)...
#include <otawa/sem/PathIter.h>
#include "analysis.h"
#include "detailed_path.h"
#include "loop_bound.h"
#include "pretty_printing.h"
#include "struct/constant_variables.h"
#include "struct/labelled_predicate.h"
#include "struct/local_variables.h"
#include "struct/var_maker.h"

using namespace otawa;
using otawa::sem::PathIter;
using elm::data::HashTable;


class Widenor;
class Analysis::State {
private:
	typedef elm::data::HashTable<Constant, const Operand*, ConstantHash> mem_t;

	const context_t* context;
	// v2
	DAG* dag;
	LocalVariables lvars;
	mem_t mem;
	struct memid_t {
		memid_t(const Block* b, short id) : b(b), id(id) { }
		const Block* b;
		short id;
		friend io::Output& operator<<(io::Output& out, const memid_t& m)
			{ return m.b ? (out << m.b->cfg() << ":" << m.b->index() << ":" << m.id) : (out << "i"); }
	} memid;

	bool bottom; // true=is Bottom, false= is Top
	DetailedPath path;
#ifdef V1
	ConstantVariables constants; // remember in an array the variables that have been identified to a constant (e.g. t2 = 4)
#endif
	elm::List<LabelledPredicate> labelled_preds; // previously generated predicates
	elm::List<LabelledPredicate> generated_preds; // predicates local to the current BB
	elm::List<LabelledPredicate> generated_preds_taken; // if there is a conditional, the taken preds will be saved here and the not taken preds will stay in generated_preds
		// that have been updated and need to have their labels list updated (add the next edge to the LabelledPreds struct)
	class PredIterator;
	class SemanticParser;

public:
	explicit State(bool bottom = false); // false: create an invalid state, true: create a bottom state
	// State(Block* entryb, const context_t& context, bool init = true);
	State(Edge* entry_edge, const context_t& context, DAG* dag, bool init);
	State(const State& s);
	inline const DetailedPath& getDetailedPath() const { return path; }
	inline Edge* lastEdge() const { return path.lastEdge(); }
	inline const elm::List<LabelledPredicate>& getLabelledPreds() const { return labelled_preds; }
#ifdef V1
	inline const ConstantVariables& getConstants() const { return constants; }
#endif
	inline const LocalVariables& getLocalVariables() const { return lvars; }
	inline const mem_t& getMemoryTable() const { return mem; }
	// inline void onLoopEntry(Block* loop_header) { path.onLoopEntry(loop_header); }
	inline void onLoopExit(Option<Block*> maybe_loop_header = elm::none) { path.onLoopExit(maybe_loop_header); }
	inline void onCall(SynthBlock* sb)   { path.onCall(sb); }
	inline void onReturn(SynthBlock* sb) { path.onReturn(sb); }
	inline bool isBottom() const { return bottom; }
	inline bool isValid() const { return context != NULL; } // this is so that we can have empty states that do not use too much memory
	inline DAG& getDag() const { return *dag; }
	inline const OperandVar& getSP() const { return context->sp; }

	// analysis_state.cpp
	// State& operator=(const State& s);
	inline elm::String dumpPath() const { return path.toString(); }
	elm::String dumpMem() const;
	elm::String dumpEverything() const;
	void initializeWithDFA();
	void merge(const States& ss, Block* b, VarMaker& vm);
	void apply(const State& s, VarMaker& vm, bool local_sp, bool clear_path = false);
	void applyPredicates(const State& s, OperandEndoVisitor& cc, bool wipe_memory);
	void prepareFixPoint();
	void finalizeLoop(OperandIter* n, VarMaker& vm);
	bool equiv(const State& s) const;
	void appendEdge(Edge* e);
	void removeConstantPredicates();
	void collectTops(VarCollector &bv) const;
	void removeTautologies();
	inline void clearPreds() { labelled_preds.clear(); generated_preds.clear(); }
	inline void clearPath() { path.clear(); }
	inline void resetSP() { lvars[context->sp] = dag->cst(SP); }
	void widening(const Operand* n);
	LoopBound getLoopBound(const Operand* oi) const;

	// analysis_bb.cpp
	void processBB(const BasicBlock *bb, VarMaker& vm, int flags);
	void processSemInst1(const otawa::sem::inst& inst, const sem::inst& last_condition);
	int  processSemInst2(SemanticParser& semp);
	int invalidateStackBelow(const Constant& stack_limit);

	inline void dumpPredicates() const { for(PredIterator iter(*this); iter; iter++) DBG(*iter); }
	inline const State* operator->(void) const { return this; }
	friend io::Output& operator<<(io::Output& out, const State& s) { return s.print(out); }

private:
	// Private methods
	// analysis.cpp
	void setPredicate(PredIterator &iter, const LabelledPredicate &labelled_predicate);
	void movePredicateToGenerated(PredIterator &iter);
	void removePredicate(PredIterator &iter);
	elm::List<LabelledPredicate> labelPredicateList(const elm::List<LabelledPredicate>& pred_list, Edge* label);
	io::Output& print(io::Output& out) const;

	// analysis_bb2.cpp
	void set(const OperandVar& var, const Operand* expr, bool set_updated = true);
	void setMem(Constant addr, const Operand* expr);
	void wipeMemory(VarMaker& vm);
	inline void setMemoryInitPoint(const otawa::Block* b, short id) { memid = memid_t(b, id); }
	const Operand* getPtr(t::int32 var_id) const;
	void updateLabels(const sem::inst& inst);
	void clampPredicates(VarMaker &vm);

	// analysis_bb1.cpp
	LabelledPredicate makeLabelledPredicate(condoperator_t opr, const Operand* opd1, const Operand* opd2, Path& labels) const;
	bool tryToKeepVar(const OperandVar& var);//, const Predicate*& removed_predicate);
	bool invalidateVar(const OperandVar& var, bool invalidate_constant_info = true);
	// bool invalidate(const Operand& var, bool invalidate_constant_info = true);
	bool invalidateMem(const OperandMem& addr);
	bool invalidateMem(const OperandVar& var);
	bool invalidateTempVars();
	// bool replaceOperand(const Operand& opd, const Operand& expr);
	bool replaceVar(const OperandVar& var, const Operand* expr);
	bool replaceMem(const OperandMem& opd, const Operand* expr, const Path& labels);
	bool replaceTempVar(const OperandVar& temp_var, const Operand* expr);
	bool replaceTempVar(const OperandMem& mem, const Operand* expr);
	bool update(const OperandVar& opd_to_update, const Operand* opd_modifier, Path& labels);
	Option<Constant> findConstantValueOfMemCell(const OperandMem& mem, Path &labels);
	Option<t::int32> findStackRelativeValueOfVar(const OperandVar& var, Path& labels);
	bool findValueOfCompVar(const OperandVar& var, Operand const*& opd_left, Operand const*& opd_right, Path& labels);
	Option<OperandMem> getOperandMem(const OperandVar& var, Path& labels);
	bool invalidateAllMemory();
	// Option<Predicate> getPredicateGeneratedByCondition(sem::inst condition, bool taken, Path& labels);
	Predicate getConditionalPredicate(sem::cond_t kind, const Operand* opd_left, const Operand* opd_right, bool taken);
	Option<Constant> getConstantValueOfReadOnlyMemCell(const OperandMem& addr_mem, otawa::sem::type_t type) const;
	int getSizeOfType(otawa::sem::type_t type) const;
#ifdef V1
	void updateLabelsWithReplacedConstantsInfo(Path& labels, const Vector<OperandVar>& replaced_vars) const;
	inline bool isConstant(const OperandVar& var) const { return constants.isConstant(var); }
	inline elm::avl::Set<Edge*> getLabels(const OperandVar& opdv) const { return constants.getLabels(opdv); }
	inline elm::avl::Set<Edge*> getLabels(const OperandVar& opdv1, const OperandVar& opdv2) const { return constants.getLabels(opdv1, opdv2); }
#endif

	// Widening
	class WideningProgress {
	public:		
		enum { INITIAL_FORM=false, GENERAL_FORM=true };
		WideningProgress(const LocalVariables& lvars) : vars(lvars.getSize(), (bool)INITIAL_FORM), lv(lvars) { }
		inline void setVar(short i) { vars.set(i, GENERAL_FORM); }
		inline void setVar(const OperandVar& var) { setVar(lv.getIndex(var)); }
		inline void setMem(const OperandMem& mem) { mems.add(mem.addr()); }
		inline bool operator[](short i) const { return vars[i]; }
		inline bool operator[](const OperandVar& var) const { return (*this)[lv.getIndex(var)]; }
		inline bool operator[](const OperandMem& mem) const { return mems.contains(mem.addr()); }
		inline bool operator[](Pair<Constant, const Operand*> pair) const { return mems.contains(pair.fst); }

		io::Output& printMems(io::Output& out) const {
			for(Set<Constant>::Iter i(mems); i; i++)
				out << *i << ", ";
			return out;
		}
		friend io::Output& operator<<(io::Output& out, const WideningProgress& wp)
			{ out << "[" << wp.vars << "; "; wp.printMems(out); return out << "]";}

	private:
		BitVector vars;
		Set<Constant> mems;
		const LocalVariables& lv;
	};
	Option<const Operand*> widen(Operand const* x, const Operand& x0, const Operand* n, const WideningProgress& wprogress, Widenor& widenor) const;
	const Operand* widenArithmeticProgression(Operand const* x, const Operand& x0, const Operand* n) const;


	// DAG stuff
	inline const Operand *Cst(const Constant& cst)    { return dag->cst(cst); }
	inline const Operand *Var(const OperandVar& opdv) { return dag->var(opdv); }
	inline const Operand *Mem(const Constant& cst) 	  { return dag->mem(cst); }
	inline const Operand *Mem(const OperandMem& opdm) { return dag->mem(opdm); }
	inline const Operand *neg (const Operand *arg) 						 { return dag->neg(arg); }
	inline const Operand *add (const Operand *arg1, const Operand *arg2) { return dag->add(arg1, arg2); }
	inline const Operand *sub (const Operand *arg1, const Operand *arg2) { return dag->sub(arg1, arg2); }
	inline const Operand *mul (const Operand *arg1, const Operand *arg2) { return dag->mul(arg1, arg2); }
	inline const Operand *mulh(const Operand *arg1, const Operand *arg2) { return dag->mulh(arg1, arg2); }
	inline const Operand *div (const Operand *arg1, const Operand *arg2) { return dag->div(arg1, arg2); }
	inline const Operand *mod (const Operand *arg1, const Operand *arg2) { return dag->mod(arg1, arg2); }
	inline const Operand *cmp (const Operand *arg1, const Operand *arg2) { return dag->cmp(arg1, arg2); }

	// PredIterator class
	class PredIterator: public PreIterator<PredIterator, const LabelledPredicate&> {
		enum pred_iterator_state_t
		{
			GENERATED_PREDS, // must have !gp_iter.ended()
			LABELLED_PREDS, // must have gp_iter.ended() && !lp_iter.ended()
			DONE, // must have gp_iter.ended() && lp_iter.ended()
		};

	public:
		inline PredIterator(void) { }
		inline PredIterator(const Analysis::State& analysis_state)
			: state(GENERATED_PREDS), gp_iter(analysis_state.generated_preds), lp_iter(analysis_state.labelled_preds) { updateState(); }
		inline PredIterator(const PredIterator& source): gp_iter(source.gp_iter), lp_iter(source.lp_iter) { }
		inline PredIterator& operator=(const PredIterator& i)
			{ state = i.state; gp_iter = i.gp_iter; lp_iter = i.lp_iter; return *this; }
		
		inline bool ended(void) const
			{ return (state == DONE); }
		inline const LabelledPredicate& item(void) const
			{ return (state == GENERATED_PREDS) ? gp_iter.item() : lp_iter.item(); }
		// this behaves fine when called while state == DONE. We use this in the code as of 2014-11-14 (for movePredicateToGenerated)
		void next(void) {
			if(state == GENERATED_PREDS) gp_iter++;
			if(state == LABELLED_PREDS) lp_iter++;
			updateState();
		}
		inline const Predicate& pred(void) const { return item().pred(); }
		inline const Path& labels(void) const { return item().labels(); }
		void updateState() {
			if(state == GENERATED_PREDS && !gp_iter) { nextState(); updateState(); }
			if(state == LABELLED_PREDS && !lp_iter) { nextState(); updateState(); }
		}
	private:
		inline void nextState() {
			if(state == GENERATED_PREDS) state = LABELLED_PREDS;
			else if(state == LABELLED_PREDS) state = DONE;
		}
		friend class Analysis::State;
		pred_iterator_state_t state;
		elm::List<LabelledPredicate>::Iter gp_iter; // Generated predicates (local) iterator 
		elm::List<LabelledPredicate>::Iter lp_iter; // Labelled predicates (previous BBs) iterator
	}; // PredIterator class
	
	class MutablePredIterator: public PreIterator<MutablePredIterator, LabelledPredicate&> {
		enum pred_iterator_state_t {
			GENERATED_PREDS, // must have !gp_iter.ended()
			LABELLED_PREDS, // must have gp_iter.ended() && !lp_iter.ended()
			DONE, // must have gp_iter.ended() && lp_iter.ended()
		};

	public:
		inline MutablePredIterator(Analysis::State& analysis_state)
			: state(GENERATED_PREDS), gp_iter(analysis_state.generated_preds), lp_iter(analysis_state.labelled_preds)
			{ updateState(); }
		inline bool ended(void) const
			{ return (state == DONE); }
		inline LabelledPredicate& item(void)
			{ return (state == GENERATED_PREDS) ? gp_iter.item() : lp_iter.item(); }
		void next(void) {
			if(state == GENERATED_PREDS) gp_iter++;
			if(state == LABELLED_PREDS) lp_iter++;
			updateState();
		}
	private:
		void updateState() {
			if(state == GENERATED_PREDS && !gp_iter) { nextState(); updateState(); }
			if(state == LABELLED_PREDS && !lp_iter) { nextState(); updateState(); }
		}
		inline void nextState() {
			if(state == GENERATED_PREDS)
				state = LABELLED_PREDS;
			else if(state == LABELLED_PREDS)
				state = DONE;
		}

		friend class Analysis::State;
		pred_iterator_state_t state;
		elm::List<LabelledPredicate>::MutableIterator gp_iter;
		elm::List<LabelledPredicate>::MutableIterator lp_iter;
	};  

	struct MemCell {
		MemCell(Constant addr, const Operand* val) : addr(addr), val(val) { }
		Constant addr;
		const Operand* val;

		inline bool operator>(const MemCell& mc) const { return this->addr < mc.addr; } // opposite
		inline bool operator==(const MemCell& mc) const { return this->addr == mc.addr && this->val == mc.val; }
	};
}; // State class

extern const Analysis::State bottom;

#endif
