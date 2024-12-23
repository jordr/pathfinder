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
 
#ifndef _CONSTANT_VARIABLES_CORE_H
#define _CONSTANT_VARIABLES_CORE_H

#include <elm/avl/Set.h>
// #include <elm/data/List.h>
#include <otawa/cfg/Edge.h>
#include "constant.h"
#include "../debug.h"

using namespace otawa;
// using namespace elm::genstruct; 
using namespace elm::avl;

class ConstantVariablesCore {
public:
	class LabelledValue
	{
	public:
		LabelledValue()
			: _exists(false), _labels(NULL) { }
		LabelledValue(const LabelledValue& lv)
			: _val(lv._val), _updated(lv._updated), _exists(lv._exists), _labels(lv._labels ? new Set<Edge*>(*lv._labels) : NULL) { }
		LabelledValue(const Constant& val, const Set<Edge*>& labels, bool updated = true)
			: _val(val), _updated(updated), _exists(true), _labels(new Set<Edge*>(labels)) { }
		~LabelledValue()
			{ delete _labels; }
		inline Constant val() const
			{ return _val; }
		inline const Set<Edge*>& labels() const
			{ return *_labels; }
		inline bool isUpdated() const
			{ return _updated; }
		inline bool exists() const
			{ return _exists; }
		inline void setVal(Constant val)
			{ _val = val; }
		inline void addLabel(Edge* label)
			{ ASSERT(_labels); _labels->add(label); }
		// inline void clearLabels()
			// { _labels.clear(); }
		inline void setUpdatedFlag(bool updated = true)
			{ _updated = updated; }
		inline void clear()
			{ delete _labels; _labels = NULL; _exists = false; }
		LabelledValue& operator=(const LabelledValue& lv);
		operator bool() const
			{ return _exists; }
		bool operator==(const LabelledValue& lv) const;
		inline bool operator!=(const LabelledValue& lv) const
			{ return !(lv == *this); }
		friend io::Output& operator<<(io::Output& out, const LabelledValue& lv)
			{ return lv.print(out); }

	private:
		Constant _val;
		bool _updated;
		bool _exists;
		Set<Edge*>* _labels;

		io::Output& print(io::Output& out) const;
	}; // LabelledValue class

	ConstantVariablesCore(); // invalid
	ConstantVariablesCore(short max_tempvars, short max_registers);
	ConstantVariablesCore(const ConstantVariablesCore& cv);
	~ConstantVariablesCore();
	void clear();
	inline short maxTempVars() const { return _max_tempvars; }
	inline short maxRegisters() const { return _max_registers; }
	inline bool isValid() const { return _max_registers && _max_tempvars; }
	
	inline bool isConstant(t::int32 var_id) const { return getCell(var_id).exists(); }
	inline Constant getValue(t::int32 var_id) const { return getCell(var_id).val(); }
	void label(Edge* label);
	// void merge(const elm::List<ConstantVariablesCore>& cvl);
	elm::String printChanges() const;
	ConstantVariablesCore& operator=(const ConstantVariablesCore& cv);
	bool operator==(const ConstantVariablesCore& cv) const; // strict
	bool sameValuesAs(const ConstantVariablesCore& cv) const; // less strict (only values)
	inline bool operator!=(const ConstantVariablesCore& cv) const { return !(*this == cv); }
	inline Constant operator[](t::int32 var_id) const { return getValue(var_id); }
	friend io::Output& operator<<(io::Output& out, const ConstantVariablesCore& cv) { return cv.print(out); }

protected:
	LabelledValue* tempvars;
	LabelledValue* registers;
	short _max_tempvars;
	short _max_registers;

	LabelledValue& getCell(t::int32 var_id) const;
	io::Output& print(io::Output& out) const;
}; // ConstantVariablesCore class

#endif
