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
 
#ifndef _ANALYSIS_SEM2_H
#define _ANALYSIS_SEM2_H

class Analysis::State::SemanticParser
{	// TODO: make it act like an Iter, and make a SemanticParser for v1 too
public:
	SemanticParser(State& s, VarMaker& vm) : s(s), vm(vm), lvars(s.lvars), dag(*s.dag), last_condition(sem::NOP) { }
	int process(const sem::PathIter& seminsts);
	// int process(sem::inst inst);
	inline void set(const OperandVar& var, const Operand* expr, bool set_updated = true) { s.set(var, expr, set_updated); }
	inline void setMem(Constant addr, const Operand* expr) { s.setMem(addr, expr); }
	int store(OperandVar addr, const Operand* b);
	void scratch(const OperandVar& var);
	static bool affectsRegister(t::uint16 op);
	static bool affectsMemory(t::uint16 op);
	inline const Operand* getPtr(t::int32 var_id) const { return s.getPtr(var_id); }
	inline const sem::inst& lastCond() const { return last_condition; }
	inline const Operand* new_top() const { return vm.new_top(); }

private:
	State& s;
	VarMaker& vm;
	const LocalVariables& lvars;
	DAG& dag;
	sem::inst last_condition;
};

#endif