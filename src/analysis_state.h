#ifndef _ANALYSIS_STATE_H
#define _ANALYSIS_STATE_H

#include <elm/avl/Set.h>
#include <elm/genstruct/HashTable.h>
// #include <elm/genstruct/SLList.h>
#include <otawa/cfg/Edge.h>
#include <otawa/cfg/features.h>
#include <otawa/dfa/State.h> // dfa::State: isInitialized(addr), get(addr, _)...
#include <otawa/sem/PathIter.h>
#include "analysis.h"
#include "constant_variables.h"
#include "detailed_path.h"
// #include "halfpredicate.h"
#include "labelled_predicate.h"
#include "local_variables.h"

using namespace otawa;
using otawa::sem::PathIter;
using elm::genstruct::SLList;
// using elm::genstruct::Vector;
using elm::genstruct::HashTable;

class Analysis::State {
private:
	typedef HashTable<Constant, const Operand*, ConstantHash> mem_t;
	// HashTable<t::int32, const Operand*> lvars;

	const dfa::State* dfa_state;
	OperandVar sp; // the Stack Pointer register

	// v2
	DAG* dag;
	LocalVariables lvars;
	//LocalVariables lvars_taken;
	mem_t mem;

	bool bottom; // true=is Bottom, false= is Top
	DetailedPath path;
#ifdef V1
	ConstantVariables constants; // remember in an array the variables that have been identified to a constant (e.g. t2 = 4)
#endif
	SLList<LabelledPredicate> labelled_preds; // previously generated predicates
	SLList<LabelledPredicate> generated_preds; // predicates local to the current BB
	SLList<LabelledPredicate> generated_preds_taken; // if there is a conditional, the taken preds will be saved here and the not taken preds will stay in generated_preds
		// that have been updated and need to have their labels list updated (add the next edge to the LabelledPreds struct)
	class PredIterator;

public:
	explicit State(bool bottom = false); // false: create an invalid state, true: create a bottom state
	// State(Block* entryb, const context_t& context, bool init = true);
	State(Edge* entry_edge, const context_t& context, bool init);
	State(const State& s);
	inline const DetailedPath& getDetailedPath() const { return path; }
	inline Edge* lastEdge() const { return path.lastEdge(); }
	inline const SLList<LabelledPredicate>& getLabelledPreds() const { return labelled_preds; }
#ifdef V1
	inline const ConstantVariables& getConstants() const { return constants; }
#endif
	inline const LocalVariables& getLocalVariables() const { return lvars; }
	inline const mem_t& getMemoryTable() const { return mem; }
	inline elm::String getPathString() const { return path.toString(); /*orderedPathToString(path.toOrderedPath());*/ }
	inline void onLoopEntry(Block* loop_header) { path.onLoopEntry(loop_header); }
	inline void onLoopExit(Option<Block*> maybe_loop_header = elm::none) { path.onLoopExit(maybe_loop_header); }
	inline void onCall(SynthBlock* sb)   { path.onCall(sb); }
	inline void onReturn(SynthBlock* sb) { path.onReturn(sb); }
	inline bool isBottom() const { return bottom; }
	inline bool isValid() const { return dfa_state != NULL /*&& constants.isValid()*/; } // this is so that we can have empty states that do not use too much memory
	inline DAG& getDag() const { return *dag; }

	// analysis_state.cpp
	template <class C> Vector<DetailedPath> stateListToPathVector(const C& sl) const;
	elm::String dumpEverything() const;
	void merge(const States& ss, Block* b);
	void accel(const State& s0);
	bool equiv(const State& s) const;
	void appendEdge(Edge* e);
	void removeConstantPredicates();

	// analysis_bb.cpp
	void processBB(const BasicBlock *bb, int version_flags = Analysis::WITH_V1 | Analysis::WITH_V2);
	void processSemInst1(const otawa::sem::PathIter& seminsts, const sem::inst& last_condition);
	void processSemInst2(const otawa::sem::PathIter& seminsts, const sem::inst& last_condition);
	// void throwInfo();
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
	SLList<LabelledPredicate> labelPredicateList(const SLList<LabelledPredicate>& pred_list, Edge* label);
	io::Output& print(io::Output& out) const;

	// analysis_bb.cpp (v2)
	void set(const OperandVar& var, const Operand* expr, bool set_updated = true);
	void setMem(Constant addr, const Operand* expr);
	inline void scratch(const OperandVar& var);
	void scratchAllMemory();
	inline const Operand* getPtr(t::int32 var_id) const;
	void updateLabels(const PathIter& seminsts);
	static bool affectsRegister(t::uint16 op);
	static bool affectsMemory(t::uint16 op);
	void store(OperandVar addr, const Operand* b);
	// smart functions
	const Operand* smart_add(const Operand* a, const Operand* b);
	const Operand* smart_add(Constant x, const Operand* y);
	const Operand* smart_sub(const Operand* a, const Operand* b);
	const Operand* smart_sub(Constant x, const Operand* y);
	const Operand* smart_mul(const Operand* a, const Operand* b);
	const Operand* smart_mul(const Operand* a, Constant c);
	const Operand* smart_div(const Operand* a, const Operand* b);
	const Operand* smart_div(const Operand* a, Constant c);
	const Operand* smart_divmul(const Operand* x, Constant k, Constant c);
	const Operand* smart_muldiv(const Operand* x, Constant k, Constant c);

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
	Option<OperandConst> findConstantValueOfVar(const OperandVar& var); // changed to a simple lookup to "constants"
	Option<Constant> findConstantValueOfMemCell(const OperandMem& mem, Path &labels);
	Option<t::int32> findStackRelativeValueOfVar(const OperandVar& var, Path& labels);
	bool findValueOfCompVar(const OperandVar& var, Operand const*& opd_left, Operand const*& opd_right, Path& labels);
	Option<OperandMem> getOperandMem(const OperandVar& var, Path& labels);
	bool invalidateAllMemory();
#ifdef V1
	void updateLabelsWithReplacedConstantsInfo(Path& labels, const Vector<OperandVar>& replaced_vars) const;
#endif
	// Option<Predicate> getPredicateGeneratedByCondition(sem::inst condition, bool taken, Path& labels);
	Predicate getConditionalPredicate(sem::cond_t kind, const Operand* opd_left, const Operand* opd_right, bool taken);
	Option<Constant> getConstantValueOfReadOnlyMemCell(const OperandMem& addr_mem, otawa::sem::type_t type) const;
	int getSizeOfType(otawa::sem::type_t type) const;
#ifdef V1
	inline bool isConstant(const OperandVar& var) const { return constants.isConstant(var); }
	inline elm::avl::Set<Edge*> getLabels(const OperandVar& opdv) const { return constants.getLabels(opdv); }
	inline elm::avl::Set<Edge*> getLabels(const OperandVar& opdv1, const OperandVar& opdv2) const { return constants.getLabels(opdv1, opdv2); }
#endif

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
		
		inline bool ended(void) const { return (state == DONE); }
		inline const LabelledPredicate& item(void) const {
			return (state == GENERATED_PREDS) ? gp_iter.item() : lp_iter.item();
			// switch(state) {
			// 	case GENERATED_PREDS: return gp_iter.item();
			// 	case LABELLED_PREDS: return lp_iter.item();
			// 	default: ASSERT(false);
			// }
		}
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
		void nextState() { if(state == GENERATED_PREDS) state = LABELLED_PREDS; else if (state == LABELLED_PREDS) state = DONE; }

		friend class Analysis::State;
		pred_iterator_state_t state;
		SLList<LabelledPredicate>::Iterator gp_iter; // Generated predicates (local) iterator 
		SLList<LabelledPredicate>::Iterator lp_iter; // Labelled predicates (previous BBs) iterator
	}; // PredIterator class
}; // State class

extern const Analysis::State bottom;

class Analysis::States : public elm::Lock {
public:
	inline States() { }
	inline States(int cap) : s(cap) { }
	inline States(const Vector<Analysis::State>& state_vector) : s(state_vector) { }
	inline States(const States& ss) : s(ss.s) { }
	// return the unique state, or bottom if none. it is an error to call this when s.count() > 1
	inline State one() const { ASSERTP(s.count() <= 1, "multiple states available"); return s ? s.first() : bottom; }
	inline bool isEmpty() const { return s.isEmpty(); }
	inline int count() const { return s.count(); }
	inline const State& first() const { return s.first(); }
	inline const Vector<State>& states() const { return s; }
	inline Vector<State>& states() { return s; }
	inline void push(const Analysis::State& state) { return s.push(state); }

	inline void onCall(SynthBlock* sb)   { for(Iterator i(this->s); i; i++) s[i].onCall(sb); }
	inline void onReturn(SynthBlock* sb) { for(Iterator i(this->s); i; i++) s[i].onReturn(sb); }
	inline void onLoopEntry(Block* b)    { for(Iterator i(this->s); i; i++) s[i].onLoopEntry(b); }
	inline void onLoopExit (Block* b)    { for(Iterator i(this->s); i; i++) s[i].onLoopExit(b); }
	inline void onLoopExit (Edge* e)
		{ Block* h = LOOP_EXIT_EDGE(e); for(LoopHeaderIter i(e->source()); i.item() != h; i++) onLoopExit(*i); onLoopExit(h); }

	typedef Vector<State>::Iter Iterator;
	inline void remove(const Iterator& i) { s.remove(i); }

	// inline operator Vector<State>() { return s; }
	inline State& operator[](const Iterator& i) { return s[i]; }
	inline States& operator=(const Vector<State>& sv) { s = sv; return *this; }
	inline States& operator=(const States& ss) { s = ss.s; return *this; }
	inline friend io::Output& operator<<(io::Output& out, const States& ss) { return out << ss.s; };

private:
	Vector<State> s;
};

template <class C> Vector<DetailedPath> Analysis::State::stateListToPathVector(const C& cl) const
{
	Vector<DetailedPath> rtn;
	for(typename C::Iterator iter(cl); iter; iter++)
		rtn.add(iter->getDetailedPath());
	return rtn;
}

#endif
