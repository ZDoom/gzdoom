#ifndef _RE2C_UTIL_LOCAL_INCREMENT_
#define _RE2C_UTIL_LOCAL_INCREMENT_

namespace re2c
{

template <typename counter_t>
struct local_increment_t
{
	counter_t & counter;
	inline explicit local_increment_t (counter_t & c)
		: counter (++c)
	{}
	inline ~local_increment_t ()
	{
		--counter;
	}
};

} // namespace re2c

#endif // _RE2C_UTIL_LOCAL_INCREMENT_
