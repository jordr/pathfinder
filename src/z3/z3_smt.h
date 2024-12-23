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
 * Implementing z3 as the SMT solver
 */
#ifndef _Z3_Z3_SMT_H
#define _Z3_Z3_SMT_H

#include <z3++.h>
#include "../analysis.h" // Analyis::Path
#include "../smt.h" // abstract SMT class inheritance

class Z3SMT : public SMT
{
public:
    Z3SMT(int flags);
    static inline elm::String name() { return "z3"; }
    
private:
    z3::context c;
    z3::solver s;
    z3::params p;
    z3::expr sp;
    // Z3VariableStack variables; // no need of this with z3
    Vector<unsigned int> labels_hash;

    // SMT virtual pure methods
    void initialize(const elm::List<LabelledPredicate>& labelled_preds);
    void initialize(const LocalVariables& lv, const HashTable<Constant, const Operand*, ConstantHash>& mem, DAG& dag) { }
    bool checkPredSat();
    bool retrieveUnsatCore(Analysis::Path& path, const elm::List<LabelledPredicate>& labelled_preds, std::basic_string<char>& unsat_core_output);
    z3::expr getExpr(const Predicate& p);
    z3::expr getExpr(const Operand& o);
};

#endif