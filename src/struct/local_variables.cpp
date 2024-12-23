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
 
/**
 * Structure to represent local variables (a table for registers & tempvars)
 */

#include "local_variables.h"


/**
* @fn inline bool LocalVariables::isValid() const
 * @brief      Determines if valid.
 *
 * @return     True if valid, False otherwise.
 */

/**
* @fn inline short LocalVariables::maxRegisters() const
 * @brief      Gets the count of possible registers.
 *
 * @return     The registers count.
 */

/**
 * @brief      Determines if a variable is identified to be equal to a constant.
 *
 * @fn inline bool LocalVariables::isConst(OperandVar var) const
 * @param[in]  var   The variable
 *
 * @return     True if constant, False otherwise.
 */

/**
 * @brief      Gets the labels associated to a variable.
 *
 * @fn inline const labels_t& LocalVariables::labels(OperandVar var) const
 * @param[in]  var   The variable
 *
 * @return     The labels.
 */

/**
 * @brief      Associate a new label to a variable.
 * @fn inline void LocalVariables::label(OperandVar var, Edge* e)
 *
 * @param[in]  var   The variable to associate a label to
 * @param      e     The edge to label with
 */

/**
* @fn inline void LocalVariables::clearLabels(OperandVar var)
 * @brief      Clear all labels associated to a variable.
 *
 * @param[in]  var   The variable
 */

/**
 * @brief      Determines if updated.
 *
 * @fn inline bool LocalVariables::isUpdated(OperandVar var)
 * @param[in]  var   The variable
 *
 * @return     True if updated, False otherwise.
 */

/**
* @fn inline void LocalVariables::markAsUpdated(OperandVar var)
 * @brief      Mark a variable as updated
 *
 * @param[in]  var   The variable
 */

/**
 * @fn inline void LocalVariables::resetUpdatedMarks()
 * @brief      Reset all updated flags.
 */

/**
 * @brief      Label all variables that are marked as updated with an edge (and remove mark)
 *
 * @param      e     Edge to label with
 */
void LocalVariables::onEdge(Edge* e)
{
	for(int i = 0; i < size; i++)
		if(u[i])
			label(i, e);
	resetUpdatedMarks();
}

/**
 * @fn void LocalVariables::resetTempVars();
 * @brief      Resets temporary variables; aka clears [thresold; size[
 */

/**
 * @brief      Merge current LocalVariables with a provided one
 *
 * @param[in]  lv    LocalVariables to merge with
 */
void LocalVariables::merge(const LocalVariables& lv) // this = this ∩ lv
{
	for(int i = 0; i < size; i++)
	{
		if(o[i] == lv.o[i])
			u.set(i, u[i] | lv.u[i]);
		else
		{
			o[i] = Top; // TODO!! handle tops so that they are not being manipulated (T-T is not 0!!)
			u.set(i, false);
		}
		// clear labels
		delete l[i];
		l[i] = NULL;
	}
}

LocalVariables& LocalVariables::operator=(const LocalVariables& lv)
{
	if(lv.isValid())
	{
		if(isValid())
		{
			ASSERTP(size == lv.size && thresold == lv.thresold, "sizes or thresolds do not match ("
			  << size << "/" << lv.size << ", " << thresold << "/" << lv.thresold << ")");
			copy(lv);
		}
		else
		{
			size = lv.size;
			thresold = lv.thresold;
			o = new Operand const*[size];
			l = new labels_t*[size];
			array::clear(l, size);
			u.resize(size);
			// u = BitVector(size);
			copy(lv);
		}
	}
	else // we're copying an invalid LV
	{
		delete[] o;
		if(l)
			for(int i = 0; i < size; i++)
				delete l[i];
		size = 0;
		thresold = 0;
		o = NULL;
		l = NULL;
		u = BitVector();
	}
	return *this;
}

io::Output& LocalVariables::print(io::Output& out) const
{
	if(! isValid())
		return out << "<invalid>" << endl;
	for(int i = 0; i < size; i++)
	{
		if(o[i])
		{
			out << "        " << OperandVar(getId(i)) << (u[i] ? "*" : " ") << "\t | ";
			if(*o[i] == OperandVar(getId(i)))
				out << " ✓";
			else
				out << *o[i];
			if(l[i] && !(dbg_flags&DBG_DETERMINISTIC))
			{
				out << "     \t| ";
				for(labels_t::Iter li(*l[i]); li; li++)
				// for(labels_t::Iter li(*l[i]); li; li++)
					out << IntFormat((long int)(*li)).hex() << ", ";
			}
			out << endl;
		}
	}
	return out;
}
