#ifndef _ANALYSIS_H
#define _ANALYSIS_H

#include <otawa/cfg/Edge.h>
#include <elm/genstruct/SLList.h>
#include <otawa/sem/inst.h>
#include <elm/avl/Set.h>
#include <elm/util/Comparator.h>
#include "constant_variables.h"
#include "labelled_predicate.h"
#include "debug.h"

using namespace otawa;
using namespace elm::genstruct;
using namespace elm::avl;
using namespace debug;

// SLList
void addIndents(io::Output& out, int n);
template <class T> io::Output& operator<<(io::Output& out, const SLList<T>& l);

/*
class PathComparator: public elm::Comparator<SLList<const Edge*> > {
	static inline int compare(const SLList<const Edge*>& v1, const SLList<const Edge*>& v2)
		{ return 0; }
};
*/

class Analysis {
public:
	// typedef SLList<const Edge*> Path;
	typedef Set<const Edge*> Path;

	Analysis(CFG *cfg, int sp_id, unsigned int max_tempvars, unsigned int max_registers);
	inline Set<Path> infeasiblePaths() const { return infeasible_paths; }
	// inline Set<SLList<const Edge*>, PathComparator> infeasiblePaths() const { return infeasible_paths; }

	// bool invalidate_constant_info 
	enum
	{
		KEEP_CONSTANT_INFO = false,
		INVALIDATE_CONSTANT_INFO = true,
	};
	
private:
	OperandVar sp; // the Stack Pointer register
	ConstantVariables constants; // remember in an array the variables that have been identified to a constant (e.g. t2 = 4)

	SLList<SLList<LabelledPredicate> > labelled_preds; // previously generated predicates
	SLList<LabelledPredicate> generated_preds; // predicates local to the current BB
	SLList<LabelledPredicate> generated_preds_taken; // if there is a conditional, the taken preds will be saved here and the not taken preds will stay in generated_preds
	SLList<SLList<LabelledPredicate> > updated_preds; // this is reset before any BB analysis, indicates previously generated preds (in another BB)
		// that have been updated and need to have their labels list updated (add the next edge to the LabelledPreds struct)

	Set<Path> infeasible_paths; // TODO: Set<Path, PathComparator<Path> > path; to make Set useful
	int processed_paths;
	int total_paths;
	// bool solverHasBeenCalled;

	// PredIterator class
	class PredIterator: public PreIterator<PredIterator, LabelledPredicate> {
		enum pred_iterator_state_t
		{
			GENERATED_PREDS, // must have !gp_iter.ended()
			LABELLED_PREDS, // must have gp_iter.ended() && !lp_iter.ended()
			DONE, // must have gp_iter.ended() && lp_iter.ended()
		};

	public:
		inline PredIterator(void) { }
		inline PredIterator(const Analysis& analysis)
			: state(GENERATED_PREDS), gp_iter(analysis.generated_preds), lp_iter(analysis.labelled_preds.first())	{ updateState(); }
		inline PredIterator(const PredIterator& source): gp_iter(source.gp_iter), lp_iter(source.lp_iter) { }
		inline PredIterator& operator=(const PredIterator& i)
			{ state = i.state; gp_iter = i.gp_iter; lp_iter = i.lp_iter; return *this; }
		
		inline bool ended(void) const { return (state == DONE); }
		LabelledPredicate item(void) const {
			switch(state) {
				// case GENERATED_PREDS: return LabelledPredicate(gp_iter.item(), Path::null);
				case GENERATED_PREDS: return gp_iter.item();
				case LABELLED_PREDS: return lp_iter.item();
				default: assert(false);
			}
		}
		// this behaves fine when called while state == DONE. We use this in the code as of nov.14 (for movePredicateToGenerated)
		void next(void) {
			if(state == GENERATED_PREDS) gp_iter++;
			if(state == LABELLED_PREDS) lp_iter++;
			updateState();
		}

		inline Predicate pred(void) const { return item().pred(); }
		inline Path labels(void) const { return item().labels(); }

		void updateState() {
			if(state == GENERATED_PREDS && !gp_iter) { nextState(); updateState(); }
			if(state == LABELLED_PREDS && !lp_iter) { nextState(); updateState(); }
		}

	private:
		void nextState() { if(state == GENERATED_PREDS) state = LABELLED_PREDS; else if (state == LABELLED_PREDS) state = DONE; }

		friend class Analysis;
		pred_iterator_state_t state;
		SLList<LabelledPredicate>::Iterator gp_iter; // Generated predicates (local) iterator 
		SLList<LabelledPredicate>::Iterator lp_iter; // Labelled predicates (previous BBs) iterator
	};
	
	// Private methods
	// analysis.cpp
	void setPredicate(PredIterator &iter, const LabelledPredicate &labelled_predicate);
	void movePredicateToGenerated(PredIterator &iter);
	void removePredicate(PredIterator &iter);
	inline void dumpPredicates() { for(PredIterator iter(*this); iter; iter++) DBG(*iter); }
	
	// analysis_cfg.cpp
	void initializeAnalysis();
	void processCFG(CFG *cfg);
	void processBB(BasicBlock *bb);
	void placeboProcessBB(BasicBlock *bb);
	void processEdge(const Edge *edge);
	SLList<LabelledPredicate> labelPredicateList (const SLList<LabelledPredicate>& pred_list, const Edge* label);
	
	// analysis_bb.cpp
	void analyzeBB(const BasicBlock *bb);
	bool invalidateVar(const OperandVar& var, bool invalidate_constant_info = true);
	bool invalidateMem(const OperandMem& addr);
	bool invalidateMem(const OperandVar& var);
	bool invalidateTempVars();
	bool replaceVar(const OperandVar& var, const Operand& expr);
	bool replaceTempVar(const OperandVar& temp_var, const Operand& expr);
	bool update(const OperandVar& opd_to_update, const Operand& opd_modifier);
	Option<Constant> findConstantValueOfVar(const OperandVar& var); // changed to a simple lookup to "constants"
	// bool findConstantValueOfVar_old(const OperandVar& var, t::int32& val); // old version may be better?... think about a case where t1 is sp + 4 + 2 + 6
	Option<t::int32> findStackRelativeValueOfVar(const OperandVar& var, Path& labels);
	bool findValueOfCompVar(const OperandVar& var, Operand*& opd_left, Operand*& opd_right);
	Option<OperandMem> getOperandMem(const OperandVar& var, Path& labels);
	bool invalidateAllMemory();
	Predicate* getPredicateGeneratedByCondition(sem::inst condition, bool taken);
	inline bool isConstant(const OperandVar& var) const { return constants.isConstant(var); }
};

template <class T> io::Output& operator<<(io::Output& out, const SLList<T>& l)
{
	int count = l.count();
	if(count > 10)
	{
		out << "#" << count << "#";
		return out; // just exit if too many items
	}
	
	static int indent = 0;
	bool indented_output = (count > 5);
	
	if(indented_output)
		addIndents(out, indent++);
	out << color::Bold() << "[" << color::NoBold();
	if(indented_output)
		out << io::endl;
	bool first = true;
	for(typename SLList<T>::Iterator elems(l); elems; elems++)
	{
		if(first)
		{
			if(indented_output)
				addIndents(out, indent);
			out << *elems;
			first = false;
		}
		else
		{
			out << ", ";
			if(indented_output)
			{
				out << io::endl;
				addIndents(out, indent);
			}
			out << *elems;
		}
	}
	if(indented_output)
	{
		out << io::endl;
		addIndents(out, --indent);
	}
	
	return (out << color::Bold() << "]" << color::NoBold());
};

#endif
