#ifndef _RE2C_IR_REGEXP_EMPTY_CLASS_POLICY_
#define _RE2C_IR_REGEXP_EMPTY_CLASS_POLICY_

namespace re2c {

enum empty_class_policy_t
{
	EMPTY_CLASS_MATCH_EMPTY, // match on empty input
	EMPTY_CLASS_MATCH_NONE, // fail to match on any input
	EMPTY_CLASS_ERROR // compilation error
};

} // namespace re2c

#endif // _RE2C_IR_REGEXP_EMPTY_CLASS_POLICY_
