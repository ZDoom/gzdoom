#include "src/util/c99_stdint.h"
#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/codegen/bitmap.h"
#include "src/codegen/emit.h"
#include "src/codegen/go.h"
#include "src/codegen/input_api.h"
#include "src/codegen/label.h"
#include "src/codegen/output.h"
#include "src/conf/opt.h"
#include "src/globals.h"
#include "src/ir/adfa/action.h"
#include "src/ir/adfa/adfa.h"
#include "src/ir/skeleton/skeleton.h"
#include "src/util/counter.h"

namespace re2c
{

static std::string genGetCondition ();
static void genCondGotoSub (OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames, uint32_t cMin, uint32_t cMax);
static void genCondTable   (OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames);
static void genCondGoto    (OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames);
static void emit_state     (OutputFile & o, uint32_t ind, const State * s, bool used_label);

std::string genGetCondition()
{
	return opts->cond_get_naked
		? opts->cond_get
		: opts->cond_get + "()";
}

void genGoTo(OutputFile & o, uint32_t ind, const State *from, const State *to, bool & readCh)
{
	if (opts->target == opt_t::DOT)
	{
		o.wlabel(from->label).ws(" -> ").wlabel(to->label).ws("\n");
		return;
	}

	if (readCh && from->next != to)
	{
		o.wstring(opts->input_api.stmt_peek (ind));
		readCh = false;
	}

	o.wind(ind).ws("goto ").wstring(opts->labelPrefix).wlabel(to->label).ws(";\n");
}

void emit_state (OutputFile & o, uint32_t ind, const State * s, bool used_label)
{
	if (opts->target != opt_t::DOT)
	{
		if (used_label)
		{
			o.wstring(opts->labelPrefix).wlabel(s->label).ws(":\n");
		}
		if (opts->dFlag && (s->action.type != Action::INITIAL))
		{
			o.wind(ind).wstring(opts->yydebug).ws("(").wlabel(s->label).ws(", ").wstring(opts->input_api.expr_peek ()).ws(");\n");
		}
	}
}

void DFA::count_used_labels (std::set<label_t> & used, label_t start, label_t initial, bool force_start) const
{
	// In '-f' mode, default state is always state 0
	if (opts->fFlag)
	{
		used.insert (label_t::first ());
	}
	if (force_start)
	{
		used.insert (start);
	}
	for (State * s = head; s; s = s->next)
	{
		s->go.used_labels (used);
	}
	for (uint32_t i = 0; i < accepts.size (); ++i)
	{
		used.insert (accepts[i]->label);
	}
	// must go last: it needs the set of used labels
	if (used.count (head->label))
	{
		used.insert (initial);
	}
}

void DFA::emit_body (OutputFile & o, uint32_t& ind, const std::set<label_t> & used_labels, label_t initial) const
{
	// If DFA has transitions to initial state, then initial state
	// has a piece of code that advances input position. Wee must
	// skip it when entering DFA.
	if (used_labels.count(head->label))
	{
		o.wind(ind).ws("goto ").wstring(opts->labelPrefix).wlabel(initial).ws(";\n");
	}

	const bool save_yyaccept = accepts.size () > 1;
	for (State * s = head; s; s = s->next)
	{
		bool readCh = false;
		emit_state (o, ind, s, used_labels.count (s->label) != 0);
		emit_action (s->action, o, ind, readCh, s, cond, skeleton, used_labels, save_yyaccept);
		s->go.emit(o, ind, readCh);
	}
}

void DFA::emit(Output & output, uint32_t& ind, bool isLastCond, bool& bPrologBrace)
{
	OutputFile & o = output.source;

	bool bProlog = (!opts->cFlag || !bWroteCondCheck);

	// start_label points to the beginning of current re2c block
	// (prior to condition dispatch in '-c' mode)
	// it can forced by configuration 're2c:startlabel = <integer>;'
	label_t start_label = o.label_counter.next ();
	// initial_label points to the beginning of DFA
	// in '-c' mode this is NOT equal to start_label
	label_t initial_label = bProlog && opts->cFlag
		? o.label_counter.next ()
		: start_label;
	for (State * s = head; s; s = s->next)
	{
		s->label = o.label_counter.next ();
	}
	std::set<label_t> used_labels;
	count_used_labels (used_labels, start_label, initial_label, o.get_force_start_label ());

	head->action.set_initial (initial_label, head->action.type == Action::SAVE);

	skeleton->warn_undefined_control_flow ();
	skeleton->warn_unreachable_rules ();
	skeleton->warn_match_empty ();

	if (opts->target == opt_t::SKELETON)
	{
		if (output.skeletons.insert (name).second)
		{
			skeleton->emit_data (o.file_name);
			skeleton->emit_start (o, max_fill, need_backup, need_backupctx, need_accept);
			uint32_t i = 2;
			emit_body (o, i, used_labels, initial_label);
			skeleton->emit_end (o, need_backup, need_backupctx);
		}
	}
	else
	{
		// Generate prolog
		if (bProlog)
		{
			o.ws("\n").wdelay_line_info ();
			if (opts->target == opt_t::DOT)
			{
				bPrologBrace = true;
				o.ws("digraph re2c {\n");
			}
			else if ((!opts->fFlag && o.get_used_yyaccept ())
			||  (!opts->fFlag && opts->bEmitYYCh)
			||  (opts->bFlag && !opts->cFlag && BitMap::first)
			||  (opts->cFlag && !bWroteCondCheck && opts->gFlag)
			||  (opts->fFlag && !bWroteGetState && opts->gFlag)
			)
			{
				bPrologBrace = true;
				o.wind(ind++).ws("{\n");
			}
			else if (ind == 0)
			{
				ind = 1;
			}
			if (!opts->fFlag && opts->target != opt_t::DOT)
			{
				if (opts->bEmitYYCh)
				{
					o.wind(ind).wstring(opts->yyctype).ws(" ").wstring(opts->yych).ws(";\n");
				}
				o.wdelay_yyaccept_init (ind);
			}
			else
			{
				o.ws("\n");
			}
		}
		if (opts->bFlag && !opts->cFlag && BitMap::first)
		{
			BitMap::gen(o, ind, lbChar, ubChar <= 256 ? ubChar : 256);
		}
		if (bProlog)
		{
			if (opts->cFlag && !bWroteCondCheck && opts->gFlag)
			{
				genCondTable(o, ind, output.types);
			}
			o.wdelay_state_goto (ind);
			if (opts->cFlag && opts->target != opt_t::DOT)
			{
				if (used_labels.count(start_label))
				{
					o.wstring(opts->labelPrefix).wlabel(start_label).ws(":\n");
				}
			}
			o.wuser_start_label ();
			if (opts->cFlag && !bWroteCondCheck)
			{
				genCondGoto(o, ind, output.types);
			}
		}
		if (opts->cFlag && !cond.empty())
		{
			if (opts->condDivider.length())
			{
				o.wstring(replaceParam(opts->condDivider, opts->condDividerParam, cond)).ws("\n");
			}
			if (opts->target == opt_t::DOT)
			{
				o.wstring(cond).ws(" -> ").wlabel(head->label).ws("\n");
			}
			else
			{
				o.wstring(opts->condPrefix).wstring(cond).ws(":\n");
			}
		}
		if (opts->cFlag && opts->bFlag && BitMap::first)
		{
			o.wind(ind++).ws("{\n");
			BitMap::gen(o, ind, lbChar, ubChar <= 256 ? ubChar : 256);
		}
		// Generate code
		emit_body (o, ind, used_labels, initial_label);
		if (opts->cFlag && opts->bFlag && BitMap::first)
		{
			o.wind(--ind).ws("}\n");
		}
		// Generate epilog
		if ((!opts->cFlag || isLastCond) && bPrologBrace)
		{
			o.wind(--ind).ws("}\n");
		}
	}

	// Cleanup
	if (BitMap::first)
	{
		delete BitMap::first;
		BitMap::first = NULL;
	}
}

void genCondTable(OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames)
{
	const size_t conds = condnames.size ();
	o.wind(ind++).ws("static void *").wstring(opts->yyctable).ws("[").wu64(conds).ws("] = {\n");
	for (size_t i = 0; i < conds; ++i)
	{
		o.wind(ind).ws("&&").wstring(opts->condPrefix).wstring(condnames[i]).ws(",\n");
	}
	o.wind(--ind).ws("};\n");
}

void genCondGotoSub(OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames, uint32_t cMin, uint32_t cMax)
{
	if (cMin == cMax)
	{
		o.wind(ind).ws("goto ").wstring(opts->condPrefix).wstring(condnames[cMin]).ws(";\n");
	}
	else
	{
		uint32_t cMid = cMin + ((cMax - cMin + 1) / 2);

		o.wind(ind).ws("if (").wstring(genGetCondition()).ws(" < ").wu32(cMid).ws(") {\n");
		genCondGotoSub(o, ind + 1, condnames, cMin, cMid - 1);
		o.wind(ind).ws("} else {\n");
		genCondGotoSub(o, ind + 1, condnames, cMid, cMax);
		o.wind(ind).ws("}\n");
	}
}

/*
 * note [condition order]
 *
 * In theory re2c makes no guarantee about the order of conditions in
 * the generated lexer. Users should define condition type 'YYCONDTYPE'
 * and use values of this type with 'YYGETCONDITION' and 'YYSETCONDITION'.
 * This way code is independent of internal re2c condition numbering.
 *
 * However, it is possible to manually hardcode condition numbers and make
 * re2c generate condition dispatch without explicit use of condition names
 * (nested 'if' statements with '-b' or computed 'goto' table with '-g').
 * This code is syntactically valid (compiles), but unsafe:
 *     - change of re2c options may break compilation
 *     - change of internal re2c condition numbering may break runtime
 *
 * re2c has to preserve the existing numbering scheme.
 *
 * re2c warns about implicit assumptions about condition order, unless:
 *     - condition type is defined with 'types:re2c' or '-t, --type-header'
 *     - dispatch is independent of condition order: either it uses
 *       explicit condition names or there's only one condition and
 *       dispatch shrinks to unconditional jump
 */
void genCondGoto(OutputFile & o, uint32_t ind, const std::vector<std::string> & condnames)
{
	const size_t conds = condnames.size ();
	if (opts->target == opt_t::DOT)
	{
		o.warn_condition_order = false; // see note [condition order]
		for (size_t i = 0; i < conds; ++i)
		{
			const std::string cond = condnames[i];
			o.ws("0 -> ").wstring(cond).ws(" [label=\"state=").wstring(cond).ws("\"]\n");
		}
	}
	else if (opts->gFlag)
	{
		o.wind(ind).ws("goto *").wstring(opts->yyctable).ws("[").wstring(genGetCondition()).ws("];\n");
	}
	else if (opts->sFlag)
	{
		if (conds == 1)
		{
			o.warn_condition_order = false; // see note [condition order]
		}
		genCondGotoSub(o, ind, condnames, 0, static_cast<uint32_t> (conds) - 1);
	}
	else
	{
		o.warn_condition_order = false; // see note [condition order]
		o.wind(ind).ws("switch (").wstring(genGetCondition()).ws(") {\n");
		for (size_t i = 0; i < conds; ++i)
		{
			const std::string & cond = condnames[i];
			o.wind(ind).ws("case ").wstring(opts->condEnumPrefix).wstring(cond).ws(": goto ").wstring(opts->condPrefix).wstring(cond).ws(";\n");
		}
		o.wind(ind).ws("}\n");
	}
	o.wdelay_warn_condition_order ();
	bWroteCondCheck = true;
}

} // end namespace re2c
