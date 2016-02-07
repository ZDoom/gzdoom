#include "src/util/c99_stdint.h"
#include <utility>
#include <vector>

#include "src/codegen/go.h"

namespace re2c
{

Cases::~Cases ()
{
	delete [] cases;
}

Binary::~Binary ()
{
	delete cond;
	delete thn;
	delete els;
}

Linear::~Linear ()
{
	for (uint32_t i = 0; i < branches.size (); ++i)
	{
		delete branches[i].first;
	}
}

If::~If ()
{
	switch (type)
	{
		case BINARY:
			delete info.binary;
			break;
		case LINEAR:
			delete info.linear;
			break;
	}
}

SwitchIf::~SwitchIf ()
{
	switch (type)
	{
		case SWITCH:
			delete info.cases;
			break;
		case IF:
			delete info.ifs;
			break;
	}
}

GoBitmap::~GoBitmap ()
{
	delete hgo;
	delete lgo;
}

CpgotoTable::~CpgotoTable ()
{
	delete [] table;
}

Cpgoto::~Cpgoto ()
{
	delete hgo;
	delete table;
}

Dot::~Dot ()
{
	delete cases;
}

Go::~Go ()
{
	switch (type)
	{
		case EMPTY:
			break;
		case SWITCH_IF:
			delete info.switchif;
			break;
		case BITMAP:
			delete info.bitmap;
			break;
		case CPGOTO:
			delete info.cpgoto;
			break;
		case DOT:
			delete info.dot;
			break;
	}
}

} // namespace re2c
