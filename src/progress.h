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
 
#ifndef _PROGRESS_H
#define _PROGRESS_H

#include <ctime> // clock
#include <elm/data/HashTable.h>
#include <elm/string/String.h>
#include <otawa/cfg/CFG.h>
#include "analysis.h"
#include "debug.h"
#include "cfg_features.h"

using elm::data::HashTable;
using otawa::Block;

// solver progress bar
/* example: [■■       ] 4 states */
class Analysis::SolverProgress {
public:
	SolverProgress(int state_count) : i(0), n(state_count) { if(n) { onSolving(false); } }
	~SolverProgress() { onFinish(); }
	inline void onSolving(bool ip) { elm::cout << "\e[1A\n\e[K" << percentBar(i++*10/n,9,ip) << " " << n << " states"; }

private:
	inline void onFinish() { elm::cout << "\e[1A\n\e[K"; }
	inline elm::String percentBar(int x, int n, bool ip = false) const {
		elm::String rtn = ip ? "["+color::IRed() : "[";
		int i = 0;
		for(; i < x; i++)
			rtn = rtn + "■";
		for(; i < n; i++)
			rtn = rtn + " ";
		return rtn + (ip ? color::RCol() + "]" : "]");
	}

	int i;
	const int n;
};


class Analysis::Progress
{
public:
	virtual void onBlock(Block* b) { }
	virtual void enter(CFG* cfg) { }
	virtual void exit(CFG* cfg, int states_count, int tops_count, int ip_count) { }
	virtual ~Progress() { }
protected:
	Progress() { }
	static inline void moveUp(int n)
		{ elm::cout << "\e[" << n << "A"; }
	static inline void moveDown(int n)
		{ elm::cout << "\e[" << n << "B"; }
	static inline void moveRight(int n)
		{ elm::cout << "\e[" << n << "C"; }
	static inline void newLines(int n)
		{ for(int i = 0; i < n; i++) elm::cout << endl; }
	static inline void eraseAboveLines(int n)
		{ moveUp(n); for(int i = 0; i < n; i++) moveDown(1); }
};

/**
  * @class Progress
  * @author Jordy Ruiz 
  * @brief call tree with progress.
  * example:
  *	getbit         :   5/9   (55 %)
  *	des            :  18/40  (45 %)
  *	main           :   1/3   (33 %)
*/
class Analysis::Progressv1 : public Analysis::Progress
{
private:
	struct Stats
	{
		Stats() : curr(0), tot(0) { }
		Stats(int curr, int tot) : curr(curr), tot(tot) { }
		Stats operator+(const Stats& s) const { return Stats(curr+s.curr, tot+s.tot); }

		int curr; // current count of parsed BBs
		int tot; // total count of BBs in CFG
	};

public:
	Progressv1(CFG* main, bool v2 = false) : lines(0), maxlines(-1), v2(v2) { onEntry(main); }
	void onBlock(Block* b) {
		bool all_leave = true; // enable when on sequential level
		for(LoopHeaderIter lh(b); lh; lh++) {
			if(loopStatus(lh) != LEAVE)	{
				all_leave = false;
				break;
			}
		}
		if(!all_leave)
			return;

		if(b->isEntry())
			onEntry(b->cfg());
		else if(b->isCall()) {
			incrementStat(b->cfg());
			onExit(b->toSynth()->callee());
			// b = b->toSynth()->callee()->exit();
		}
		else if(b->isBasic())
			onBasic(b->toBasic());
		else if(b->isExit()) {
			onFinish();
			return;
		}
		// else unknown/virtual
		print(b->cfg());
	}

private:
	inline void onEntry(CFG* cfg) { ASSERT(!cfgs.contains(cfg)); cfgs.push(cfg); stats.push(Stats(0, cfg->count()-2)); }
	inline void onExit (CFG* cfg) { if(cfgs.pop() != cfg && !v2) crash(); }
	void onBasic(BasicBlock* bb) { 
		bool all_leave = true;
		for(LoopHeaderIter lh(bb); lh; lh++)
			if(loopStatus(lh) != LEAVE)	{
				all_leave = false;
				break;
			}
		if(all_leave){
			if(!cfgs.contains(bb->cfg()))
			{
				if(!v2)
					crash();
				else
				{
					cfgs.push(bb->cfg());
					stats.push(Stats(0, bb->cfg()->count()-1));
				}
			}
			incrementStat(bb->cfg());
		}
	}
	inline void onFinish() { moveUp(maxlines-1); elm::cout << StringFormat(" ").width(15+2+3+1+3+2+3+2+1) << endl; moveUp(2); maxlines = 0; }
	void print(CFG* cfg) {
		// count number n of lines required
		int n = 0;
		if(!v2)
		{
			for(Block* b = cfg->entry(); b; b = getCaller(b, NULL))
				n++;
		}
		else
			n = cfgs.length();
		if(n > maxlines)
			newLines(n-maxlines);
		eraseAboveLines(lines);
		moveUp(n);
		lines = 0;
		if(!v2)
		{
			for(Block* b = cfg->entry(); b; b = getCaller(b, NULL))
			{
				CFG* k = b->cfg();
				ASSERTP(cfgs.contains(k), "parsing callee after caller has exited!")
				int i = cfgs.indexOf(k);
				// elm::cout << StringFormat(k->name()).width(15)
				// 	<< ": " << IntFormat((*tab.get(k)).curr).width(3).right() << "/" << IntFormat((*tab.get(k)).tot).width(3)
				// 	<< " (" << IntFormat((*tab.get(k)).curr * 100/ (*tab.get(k)).tot).width(3) << "%)" << endl;
				elm::cout << StringFormat(k->name()).width(15)
					<< ": " << IntFormat((stats[i]).curr).width(3).right() << "/" << IntFormat((stats[i]).tot).width(3)
					<< " (" << IntFormat((stats[i]).curr * 100/ (stats[i]).tot).width(3) << "%)" << endl;
				lines++;
			}
		}
		else
		{
			for(int i = 0; i < cfgs.length(); i++)
			{
				elm::cout << StringFormat(cfgs[i]->name()).width(15)
					<< ": " << IntFormat((stats[i]).curr).width(3).right() << "/" << IntFormat((stats[i]).tot).width(3)
					<< " (" << IntFormat((stats[i]).curr * 100/ (stats[i]).tot).width(3) << "%)" << endl;
				lines++;
			}
		}
		maxlines = max(maxlines, lines);	
	}
	static inline elm::String indent(int n)
		{ String str; for(int i = 0; i < n; i++) str= _ << str << "\t"; return str; }
	inline void incrementStat(CFG* cfg)
		{ stats[cfgs.indexOf(cfg)] = stats[cfgs.indexOf(cfg)] + Stats(1, 0); }

	// HashTable<CFG*, Stats> tab;
	Vector<CFG*> cfgs;
	Vector<Stats> stats;
	int lines, maxlines;
	bool v2;
};

class Analysis::Progressv2 : public Analysis::Progress
{
private:
	struct Stats
	{
		Stats() : cfg(NULL), x(0) { }
		Stats(CFG* cfg, int x = 0) : cfg(cfg), x(x), timestamp(clock()) { }
		Stats& operator++(void) { this->x++; return *this; }
		Stats& operator++(int)  { this->x++; return *this; }
		friend io::Output& operator<<(io::Output& out, const Stats& s){ out << s.cfg->name() << "-" << s.x; return out; }

		CFG* cfg;
		int x; // how far it is
		std::time_t timestamp;
	};

public:
	Progressv2(void) : depth(0) { }
	void enter(CFG* cfg) {
		flush();
		prefix();
		depth++;
		elm::cout << cfg->name() << endl;

		funs.push(Stats(cfg));
		increaseFuns();
	};
	void exit(CFG* cfg, int states_count, int tops_count, int ip_count) {
		flush();
		depth--;

		int x = 0;
		float t;
		for(Vector<Stats>::Iter i(funs); i; i++)
			if(funs[i].cfg == cfg)
			{
				x = funs[i].x;
				t = (clock()-funs[i].timestamp)*1.f/CLOCKS_PER_SEC;
				funs.remove(i);
				break;
			}
		moveUp(x);
		moveRight(25);
		cout << ":" << IntFormat(states_count).right().width(5) << " states,"
					<< IntFormat(ip_count).right().width(4) << " IPs,"
					<< IntFormat(tops_count).right().width(5)  << " tops. ("
					<< FloatFormat(t).decimal().width(0,3) << "s)" << endl;
		moveDown(x);
	}
	void onBlock(Block* b) {
		flush();
		cout << b;
	}

private:
	void prefix(void) { if(depth) { indent(elm::cout, depth-1); elm::cout << "└▶"; } }
	static inline void flush()
		{ elm::cout << "\e[1A\n\e[K"; }
	static inline void indent(io::Output& out, int n)
		{ for(int i = 0; i < n; i++) out << " "; }
	inline void increaseFuns()
		{ for(Vector<Stats>::Iter i(funs); i; i++) funs[i]++; }

	int depth;
	Vector<Stats> funs;
};

#endif
