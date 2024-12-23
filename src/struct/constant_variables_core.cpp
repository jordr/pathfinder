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
 
#include "constant_variables_core.h"
#include "../debug.h"

/**
 * @class ConstantVariablesCore
 * @author Jordy Ruiz 
 * @brief Core class (incomplete methods) of ConstantVariables.
 * Contains accessible information on constant variables of the abstract interpretation analysis
 */
/**
 * @class ConstantVariablesCore::LabelledValue
 * @author Jordy Ruiz 
 * @brief Constant value labelled by a list of edges and an updated flag
 */
ConstantVariablesCore::ConstantVariablesCore() : tempvars(NULL), registers(NULL), _max_tempvars(0), _max_registers(0) { }

ConstantVariablesCore::ConstantVariablesCore(short max_tempvars, short max_registers)
	: _max_tempvars(max_tempvars), _max_registers(max_registers)
{
	tempvars = new LabelledValue[max_tempvars];
	registers = new LabelledValue[max_registers];
}

ConstantVariablesCore::ConstantVariablesCore(const ConstantVariablesCore& cv)
	: _max_tempvars(cv.maxTempVars()), _max_registers(cv.maxRegisters())
{
	tempvars = new LabelledValue[cv.maxTempVars()];
	registers = new LabelledValue[cv.maxRegisters()];
	*this = cv;
}

ConstantVariablesCore::~ConstantVariablesCore()
{
	delete[] tempvars;
	delete[] registers;
}

ConstantVariablesCore::LabelledValue& ConstantVariablesCore::LabelledValue::operator=(const LabelledValue& lv)
{
	_val = lv._val;
	_updated = lv._updated;
	_exists = lv._exists;
	delete _labels;
	if(lv._labels)
	{
		ASSERTP(_labels != lv._labels, "LabelledValue::operator= used with labels with same pointer! maybe handle this case");
		_labels = new Set<Edge*>(*lv._labels);
	}
	else
		_labels = NULL;
	return *this;
}

bool ConstantVariablesCore::LabelledValue::operator==(const LabelledValue& lv) const
{
	if(!_exists)
		return !lv._exists;
	return _val == lv._val
		&& _updated == lv._updated
		&& *_labels == *lv._labels; // Set<> has functional operator== and operator!=
}

void ConstantVariablesCore::clear()
{
	for(short i = 0; i < _max_tempvars; i++)
		tempvars[i].clear();
	for(short i = 0; i < _max_registers; i++)
		registers[i].clear();
}

ConstantVariablesCore::LabelledValue& ConstantVariablesCore::getCell(t::int32 var_id) const
{
	if(var_id < 0)
		return tempvars[-1-var_id]; // tempvars id start at 1 and are negative
	else
		return registers[var_id]; // registers ids start at 0 and are positive
}

ConstantVariablesCore& ConstantVariablesCore::operator=(const ConstantVariablesCore& cv)
{
	if(this->isValid())
	{
		// the two ConstantVariables must have the same size!
		ASSERT(_max_tempvars == cv.maxTempVars() && _max_registers == cv.maxRegisters());
		array::copy(tempvars, cv.tempvars, _max_tempvars);
		array::copy(registers, cv.registers, _max_registers);
	}
	else if(cv.isValid())
	{
		_max_tempvars = cv.maxTempVars();
		_max_registers = cv.maxRegisters();
		tempvars = new LabelledValue[cv.maxTempVars()];
		registers = new LabelledValue[cv.maxRegisters()];
		array::copy(tempvars, cv.tempvars, _max_tempvars);
		array::copy(registers, cv.registers, _max_registers);
	}
	return *this;
}

bool ConstantVariablesCore::operator==(const ConstantVariablesCore& cv) const
{
	return _max_tempvars == cv._max_tempvars
		&& _max_registers == cv._max_registers
		&& array::cmp(tempvars, cv.tempvars, _max_tempvars) == 0
		&& array::cmp(registers, cv.registers, _max_registers) == 0;
}

bool ConstantVariablesCore::sameValuesAs(const ConstantVariablesCore& cv) const
{
	if(_max_tempvars != cv._max_tempvars || _max_registers != cv._max_registers)
		return false; // sizes do not match
	for(short i = 0; i < _max_tempvars; i++)
	{
		if(tempvars[i].exists() != cv.tempvars[i].exists())
			return false;
		if(tempvars[i].exists() && tempvars[i].val() != cv.tempvars[i].val())
			return false;
	}
	for(short i = 0; i < _max_registers; i++)
	{
		if(registers[i].exists() != cv.registers[i].exists())
			return false;
		if(registers[i].exists() && registers[i].val() != cv.registers[i].val())
			return false;
	}
	return true;
}

void ConstantVariablesCore::label(Edge* label)
{
	for(short i = 0; i < _max_tempvars; i++)
		if(tempvars[i] && tempvars[i].isUpdated())
		{
			tempvars[i].addLabel(label);
			tempvars[i].setUpdatedFlag(false);
		}
	for(short i = 0; i < _max_registers; i++)
		if(registers[i] && registers[i].isUpdated())
		{
			registers[i].addLabel(label);
			registers[i].setUpdatedFlag(false);
		}
}

// Printing functions

/**
 * @fn elm::String ConstantVariablesCore::printChanges() const;
 * @brief returns a one-line string with pretty printing of changes that occurred in the constants during the current block (updated flag)
 */
elm::String ConstantVariablesCore::printChanges() const
{
	elm::String rtn = "[";
	bool first = true;
	/*for(short i = 0; i < _max_tempvars; i++)
	{
		if(tempvars[i] && (*tempvars[i]).isUpdated())
		{
			if(!first)
				rtn = rtn.concat(elm::String(", "));
			else first = false;
			rtn = rtn.concat(_ << "t" << i+1 << "=" << (*tempvars[i]).val());
		}
	}*/
	for(short i = 0; i < _max_registers; i++)
	{
		if(registers[i] && registers[i].isUpdated()) 
		{
			if(!first)
				rtn = rtn.concat(elm::String(", "));
			else first = false;
			rtn = rtn.concat(_ << "?" << i << "=" << registers[i].val());
		}
	}
	return rtn.concat(elm::String("]"));
}

io::Output& ConstantVariablesCore::LabelledValue::print(io::Output& out) const
{
	ASSERT(_exists);
	out << _val;
	if(_labels)
	{
		out << "(";
		bool first_time = true;
		for(Set<Edge*>::Iter iter(*_labels); iter; iter++)
		{
			if(first_time)
				first_time = false;
			else
				out << ", ";
#ifdef v1
			out << (*iter)->source()->number() << "->" << (*iter)->target()->number();
#else
			out << (*iter)->source()->index() << "->" << (*iter)->target()->index(); // v2
#endif
		}
		out << ")";
	}
	if(_updated)
		out << " [u]";
	return out;
}

io::Output& ConstantVariablesCore::print(io::Output& out) const
{
	out << "[" << endl;
	for(short i = 0; i < _max_tempvars; i++)
		if(tempvars[i])
			out << "\tt" << i+1 << " = " << tempvars[i] << endl;
	for(short i = 0; i < _max_registers; i++)
		if(registers[i])
			out << "\t?" << i << " = " << registers[i] << endl;
	return (out << "]");
}
