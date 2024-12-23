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
 * SMT Job for multithreading
 */

#ifndef SMT_JOB_H
#define SMT_JOB_H

#include <elm/sys/Thread.h>

template<class SMT> class SMTJob : public elm::sys::Runnable {
	typedef Pair<const Analysis::State*, Option<Analysis::Path*> > pair_t;
	typedef Vector<pair_t > data_t;

public:
	SMTJob(int flags) : flags(flags) { }

	void run() {
		for(data_t::Iter iter(data); iter; iter++) {
			SMT smt(flags);
			const Analysis::State* s = (*iter).fst;
			data[iter].snd = smt.seekInfeasiblePaths(*s); // TODO! make it work with v2
		}
	}

	void addState(const Analysis::State* s)
		{ data.push(pair_t(s, elm::none)); }

	const data_t& getResults() const
		{ return data; }

	typedef data_t::Iter Iter;

private:
	int flags;
	data_t data;
};

#endif
