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
 
#ifndef _FFX_H_
#define _FFX_H_

#include <elm/io/OutStream.h>
#include <elm/io/OutFileStream.h>
#include "analysis.h"
// using namespace elm;

class FFX
{
public:
	FFX(const Vector<DetailedPath>& ips);
	void output(const elm::String& filename, const elm::String& function_name, const elm::String& graph_filename = "");

private:
	typedef enum
	{
		FFX_TAG_LOOP=0,
		FFX_TAG_CALL=1,
	} ffx_tag_t;

	void sanitizeCallReturns(void);
	void outputSortedInfeasiblePaths(io::Output& FFXFile);
	void printInfeasiblePath(io::Output& FFXFile, const DetailedPath& ip);
	void writeGraph(io::Output& GFile, const Vector<DetailedPath>& ips);
	bool checkPathValidity(const DetailedPath& ip, bool critical) const;
	static bool lastIsCaller(elm::List<ffx_tag_t> open_tags);
	inline bool edgeAfter(elm::List<DetailedPath::FlowInfo>::Iter iter) const
		{ for(; iter; iter++) if(iter->isEdge()) return true; return false; }
	void printInfeasiblePathOldNomenclature(io::Output& FFXFile, const DetailedPath& ip);

	bool nextElementisCall(const DetailedPath::Iter& iter, CFG* cfg);
	elm::String indent(int indent_increase = 0);

	Vector<DetailedPath> infeasible_paths;
	int indent_level;
};

#endif
