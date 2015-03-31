// #define DBG_NO_DEBUG

#include <elm/io/Output.h>
#include <elm/types.h>
#include <elm/options.h>
#include <otawa/app/Application.h> // main Display class
#include <otawa/cfg/features.h> // COLLECTED_CFG_FEATURE
#include <otawa/hard/Platform.h>
#include <otawa/dfa/State.h> // INITIAL_STATE_FEATURE

#include "analysis.h"
#include "predicate.h"
#include "ffx.h"
#include "debug.h"
#include "constant.h"

using namespace elm;
using namespace otawa;

void testPredicates();
void testOperands();
void testSimplify();	

int dbg_flags = 0b0000; // global flags

class Display: public Application {
public:
    Display(void): Application("display", Version(1, 0, 0)),
    	// opt1(option::SwitchOption::Make(manager).cmd("-o").cmd("--com").description("option 1")) { }
    	opt_silent(option::SwitchOption::Make(*this).cmd("-s").cmd("--silent").description("run with minimal output")),
    	opt_supersilent(option::SwitchOption::Make(*this).cmd("--ss").cmd("--supersilent").description("run with no output")),
    	opt_output(option::SwitchOption::Make(*this).cmd("-o").cmd("--output").description("output the result of the analysis to a FFX file")),
    	opt_nocolor(option::SwitchOption::Make(*this).cmd("--no-color").description("do not use colors")),
    	opt_noinfo(option::SwitchOption::Make(*this).cmd("--no-info").description("do not print file/line number info")),
    	opt_linenumbers(option::SwitchOption::Make(*this).cmd("--line-nb").cmd("--line-numbers").description("number lines of the output")),
    	opt_notime(option::SwitchOption::Make(*this).cmd("--no-time").description("do not print execution time")),
    	opt_nopred(option::SwitchOption::Make(*this).cmd("--no-predicates").description("do not print debug info about predicates")),
    	opt_virtualize(option::SwitchOption::Make(*this).cmd("--virtualize").description("virtualize the CFG")) { }
        
protected:
	virtual void work(const string &entry, PropList &props) throw (elm::Exception) {
		/*SLList<LabelledPredicate> lps;
		OperandConst oprconst = OperandConst(2);
		OperandVar oprvar = OperandVar(0x4000);
		OperandArithExpr e1 = OperandArithExpr(ARITHOPR_MUL, oprconst, oprvar); // e1 := 2 * @0x4000
		Predicate p1 = Predicate(CONDOPR_EQ, oprvar, e1); // p1 := @0x4000 = e1
		Predicate p2 = Predicate(CONDOPR_LE, oprconst, oprvar); // p2 := 2 <= @0x4000
		for (int i = 0; i < 1000; ++i)
		{
			lps += LabelledPredicate(p1, Analysis::Path::null);
			lps += LabelledPredicate(p2, Analysis::Path::null);
			lps.clear();
		}
		return;
		*/


		workspace()->require(COLLECTED_CFG_FEATURE, props); // INVOLVED_CFGS
		workspace()->require(dfa::INITIAL_STATE_FEATURE, props); // dfa::INITIAL_STATE
		if(opt_virtualize)
			workspace()->require(VIRTUALIZED_CFG_FEATURE, props); // inline calls
		workspace()->require(LOOP_HEADERS_FEATURE, props); // LOOP_HEADER, BACK_EDGE
		workspace()->require(LOOP_INFO_FEATURE, props); // TODO! LOOP_EXIT_EDGE

        const CFGCollection *cfgs = INVOLVED_CFGS(workspace()); // retrieving the main CFG
        const dfa::State *inital_state = dfa::INITIAL_STATE(workspace()); // retrieving the initial state
		ASSERTP(cfgs->count() > 0, "no CFG found"); // make sure we have at least one CFG
		CFG *cfg = cfgs->get(0); // then get the first CFG
		int sp_id = workspace()->platform()->getSP()->number(); // retrieve the id of the stack pointer
		int max_registers = workspace()->platform()->regCount(); // retrieve the count of registers
		int max_tempvars = workspace()->process()->maxTemp(); // retrieve the maximum number of tempvars used
		int analysis_flags = 0;

		if(opt_silent || opt_supersilent)
			dbg_flags |= DBG_NO_DEBUG;
		if(opt_nocolor)
			dbg_flags |= DBG_NO_COLOR;
		if(opt_noinfo)
			dbg_flags |= DBG_NO_INFO;
		if(opt_linenumbers)
			dbg_flags |= DBG_LINE_NB;
		if(opt_notime)
			dbg_flags |= DBG_NO_TIME;
		if(opt_nopred)
			dbg_flags |= DBG_NO_PREDICATES;
		if(opt_virtualize)
			analysis_flags |= Analysis::FOLLOW_CALLS;
		if(opt_supersilent)
			analysis_flags |= Analysis::SUPERSILENT;
		Analysis analysis = Analysis(cfg, inital_state, sp_id, max_tempvars, max_registers, analysis_flags);

		// outputing to .ffx
		if(opt_output)
		{
			const Set<Analysis::Path>& infeasible_paths = analysis.infeasiblePaths();
			FFX ffx_output(infeasible_paths);
			ffx_output.output(entry+".ffx");
			if(!opt_supersilent)
				cout << "output to " + entry + ".ffx" << endl;
		}
	}

private:
	option::Manager manager;
	option::SwitchOption opt_silent, opt_supersilent, opt_output, opt_nocolor, opt_noinfo, opt_linenumbers, opt_notime, opt_nopred, opt_virtualize;
};

OTAWA_RUN(Display);

void testSimplify()
{
	OperandConst zero  = OperandConst(0);
	OperandConst one   = OperandConst(1);
	OperandConst two   = OperandConst(2);
	OperandConst three = OperandConst(3);
	OperandVar t1 = OperandVar(-1);
	OperandArithExpr e11 = OperandArithExpr(ARITHOPR_NEG, t1);
	// OperandArithExpr e12 = OperandArithExpr(ARITHOPR_SUB, two, three);
	OperandArithExpr e1 = OperandArithExpr(ARITHOPR_ADD, t1, e11);

	DBG("zero: "  << zero)
	DBG("one: "   << one)
	DBG("two: "   << two)
	DBG("three: " << three)
	DBG("e1: "    << e1)
	if(Option<Operand*> o = e1.simplify())
		DBG("e1 simplified: " << **o)
}

void testPredicates()
{
	OperandConst oprconst = OperandConst(2);
	OperandVar oprvar = OperandVar(0x4000);
	OperandArithExpr e1 = OperandArithExpr(ARITHOPR_MUL, oprconst, oprvar); // e1 := 2 * @0x4000
	Predicate p1 = Predicate(CONDOPR_EQ, oprvar, e1); // p1 := @0x4000 = e1
	Predicate p2 = Predicate(CONDOPR_LE, oprconst, oprvar); // p2 := 2 <= @0x4000
	
	DBG("--- Pretty printing of predicates ---")
	DBG("p1:\t" << p1)
	DBG("p2:\t" << p2 << io::endl)
	
	DBG("--- Equality over predicates ---")
	DBG("p1 = p1:\t" << DBG_TEST(p1 == p1, true))
	DBG("p2 = p2:\t" << DBG_TEST(p2 == p2, true))
	DBG("p1 = p2:\t" << DBG_TEST(p1 == p2, false) << io::endl)
}

void testOperands()
{
	DBG("--- Equality over Operands  ---")
	OperandConst o1 = OperandConst(12);
	OperandConst o1bis = OperandConst(12);
	OperandConst o2 = OperandConst(16);
	OperandVar o3 = OperandVar(0x1004);
	OperandVar o4 = OperandVar(0x1008);
	DBG("o1 = o1:\t" << DBG_TEST(o1 == o1, true))
	DBG("o1 = o1':\t" << DBG_TEST(o1 == o1bis, true))
	DBG("o1 = o2:\t" << DBG_TEST(o1 == o2, false))
	DBG("o1 = o3:\t" << DBG_TEST(o1 == o3, false))
	DBG("o3 = o1:\t" << DBG_TEST(o3 == o1, false))
	DBG("o3 = o3:\t" << DBG_TEST(o3 == o3, true))
	DBG("o3 = o4:\t" << DBG_TEST(o3 == o4, false) << io::endl)
	
	OperandArithExpr oae = OperandArithExpr(ARITHOPR_MUL, o1, o2);
	OperandArithExpr oaebis = OperandArithExpr(ARITHOPR_MUL, o1, o2);
	DBG("o1*o2:\t" << oae)
	OperandArithExpr oae2 = OperandArithExpr(ARITHOPR_MUL, o2, o2);
	OperandArithExpr oae3 = OperandArithExpr(ARITHOPR_MUL, o2, o1);
	OperandArithExpr oae4 = OperandArithExpr(ARITHOPR_ADD, o1, o2);
	DBG("oae = oae:\t"  << DBG_TEST(oae == oae, true))
	DBG("oae = oae':\t"  << DBG_TEST(oae == oaebis, true))
	DBG("oae = oae2:\t" << DBG_TEST(oae == oae2, false))
	DBG("oae = oae3:\t" << DBG_TEST(oae == oae3, false))
	DBG("oae = oae4:\t" << DBG_TEST(oae == oae4, false))
}
