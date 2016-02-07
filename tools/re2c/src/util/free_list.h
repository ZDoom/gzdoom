#ifndef _RE2C_UTIL_FREE_LIST_
#define _RE2C_UTIL_FREE_LIST_

#include <set>

namespace re2c
{

template<class _Ty>
class free_list: protected std::set<_Ty>
{
public:
	typedef typename std::set<_Ty>::iterator   iterator;
	typedef typename std::set<_Ty>::size_type  size_type;
	typedef typename std::set<_Ty>::key_type   key_type;
	
	free_list(): in_clear(false)
	{
	}
	
	using std::set<_Ty>::insert;

	size_type erase(const key_type& key)
	{
		if (!in_clear)
		{
			return std::set<_Ty>::erase(key);
		}
		return 0;
	}
	
	void clear()
	{
		in_clear = true;

		for(iterator it = this->begin(); it != this->end(); ++it)
		{
			delete *it;
		}
		std::set<_Ty>::clear();
		
		in_clear = false;
	}

	~free_list()
	{
		clear();
	}

protected:
	bool in_clear;
};

} // end namespace re2c

#endif // _RE2C_UTIL_FREE_LIST_
