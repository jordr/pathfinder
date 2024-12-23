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
 
#ifndef _ORACLE_H
#define _ORACLE_H

#include <elm/data/Vector.h>
#include <otawa/cfg/Edge.h>
#include "analysis.h"
#include "detailed_path.h"

class DefaultAnalysis : public Analysis
{
public:
	DefaultAnalysis() : Analysis() { }

protected:
	LockPtr<States> join(const Vector<Edge*>& edges) const;
	LockPtr<States> merge(LockPtr<States>, Block* b) const;
	bool inD_ip(const otawa::Edge* e) const;
	IPStats ipcheck(States& ss, Vector<DetailedPath>& infeasible_paths) const;

	LockPtr<States> vectorOfS(const Vector<Edge*>& ins) const;
};

#endif