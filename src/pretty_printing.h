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
 
#ifndef _PRETTY_PRINTING_H
#define _PRETTY_PRINTING_H

#include <otawa/cfg.h> // Block
#include <elm/avl/Set.h>
#include <elm/data/List.h>
#include <elm/data/Vector.h>
#include <elm/io/Output.h>
#include "debug.h"
using elm::io::Output;
template<typename T>
using SLList = elm::List<T>;
using elm::_;
using elm::Vector;
using elm::avl::Set;


// pretty printing
void addIndents(Output& out, int n);
template <class I> Output& printCollection(Output& out, I items, int size);
template <class T> inline Output& operator<<(Output& out, const elm::List<T>& l)
	{ return printCollection(out, typename elm::List<T>::Iter(l), l.count()); }
template <class T> inline Output& operator<<(Output& out, const Vector<T>& v)
	{ return printCollection(out, typename Vector<T>::Iter(v), v.count()); }
Output& otawa::operator<<(Output& out, otawa::Block* b);
elm::String pathToString(const Set<otawa::Edge*>& path);

template <class T> inline const Vector<T>& nullVector(void) {
	static const Vector<T> _null;
	return _null;
}

template <class I> elm::io::Output& printCollection(elm::io::Output& out, I items, int size)
{
	if(size > 35)
	{
		out << "#" << size << "#";
		return out; // just exit if too many items
	}
	
	static int indent = 0;
	bool indented_output = (size > 5) && false; // TODO!
	
	if(indented_output)
		addIndents(out, indent++);
	out << elm::color::Bold() << "[" << elm::color::NoBold();
	if(indented_output)
		out << elm::io::endl;
	bool first = true;
	for(; items; items++)
	{
		if(first)
		{
			if(indented_output)
				addIndents(out, indent);
			out << *items;
			first = false;
		}
		else
		{
			out << ", ";
			if(indented_output)
			{
				out << elm::io::endl;
				addIndents(out, indent);
			}
			out << *items;
		}
	}
	if(indented_output)
	{
		out << elm::io::endl;
		addIndents(out, --indent);
	}	
	return (out << elm::color::Bold() << "]" << elm::color::NoBold());
}

#endif