#include <stddef.h>
#include "src/util/c99_stdint.h"
#include <set>
#include <utility>
#include <vector>

#include "src/codegen/go.h"
#include "src/codegen/label.h"
#include "src/ir/adfa/adfa.h"

namespace re2c
{

void Cases::used_labels (std::set<label_t> & used)
{
	for (uint32_t i = 0; i < cases_size; ++i)
	{
		used.insert (cases[i].to->label);
	}
}

void Binary::used_labels (std::set<label_t> & used)
{
	thn->used_labels (used);
	els->used_labels (used);
}

void Linear::used_labels (std::set<label_t> & used)
{
	for (uint32_t i = 0; i < branches.size (); ++i)
	{
		used.insert (branches[i].second->label);
	}
}

void If::used_labels (std::set<label_t> & used)
{
	switch (type)
	{
		case BINARY:
			info.binary->used_labels (used);
			break;
		case LINEAR:
			info.linear->used_labels (used);
			break;
	}
}

void SwitchIf::used_labels (std::set<label_t> & used)
{
	switch (type)
	{
		case SWITCH:
			info.cases->used_labels (used);
			break;
		case IF:
			info.ifs->used_labels (used);
			break;
	}
}

void GoBitmap::used_labels (std::set<label_t> & used)
{
	if (hgo != NULL)
	{
		hgo->used_labels (used);
	}
	used.insert (bitmap_state->label);
	if (lgo != NULL)
	{
		lgo->used_labels (used);
	}
}

void CpgotoTable::used_labels (std::set<label_t> & used)
{
	for (uint32_t i = 0; i < TABLE_SIZE; ++i)
	{
		used.insert (table[i]->label);
	}
}

void Cpgoto::used_labels (std::set<label_t> & used)
{
	if (hgo != NULL)
	{
		hgo->used_labels (used);
	}
	table->used_labels (used);
}

void Go::used_labels (std::set<label_t> & used)
{
	switch (type)
	{
		case EMPTY:
		case DOT:
			break;
		case SWITCH_IF:
			info.switchif->used_labels (used);
			break;
		case BITMAP:
			info.bitmap->used_labels (used);
			break;
		case CPGOTO:
			info.cpgoto->used_labels (used);
			break;
	}
}

} // namespace re2c
