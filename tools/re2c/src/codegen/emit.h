#ifndef _RE2C_CODEGEN_EMIT_
#define _RE2C_CODEGEN_EMIT_

#include "src/codegen/output.h"
#include "src/ir/adfa/adfa.h"

namespace re2c {

typedef std::vector<std::string> RegExpIndices;

void emit_action
	( const Action & action
	, OutputFile & o
	, uint32_t ind
	, bool & readCh
	, const State * const s
	, const std::string & condName
	, const Skeleton * skeleton
	, const std::set<label_t> & used_labels
	, bool save_yyaccept
	);

// helpers
void genGoTo (OutputFile & o, uint32_t ind, const State * from, const State * to, bool & readCh);

template<typename _Ty> std::string replaceParam (std::string str, const std::string & param, const _Ty & value)
{
	if (!param.empty ())
	{
		std::ostringstream strValue;
		strValue << value;
		std::string::size_type pos;
		while((pos = str.find(param)) != std::string::npos)
		{
			str.replace(pos, param.length(), strValue.str());
		}
	}
	return str;
}

} // namespace re2c

#endif // _RE2C_CODEGEN_EMIT_
