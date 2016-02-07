#ifndef _RE2C_UTIL_COUNTER_
#define _RE2C_UTIL_COUNTER_

namespace re2c {

template <typename num_t>
class counter_t
{
	num_t num;

public:
	counter_t ()
		: num ()
	{}
	num_t next ()
	{
		num_t n = num;
		num.inc ();
		return n;
	}
	void reset ()
	{
		num = num_t ();
	}
};

} // namespace re2c

#endif // _RE2C_UTIL_COUNTER_
