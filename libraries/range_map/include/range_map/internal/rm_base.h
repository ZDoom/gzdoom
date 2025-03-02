// (C) Beneficii.  Released under Creative Commons license.

#ifndef BENEFICII_RANGE_MAP_RM_BASE_HPP
#define	BENEFICII_RANGE_MAP_RM_BASE_HPP

#include <cstddef>
#include <utility>
#include <tuple>

namespace beneficii {
    
// for the piecewise constructor of range_map_item; used with emplace() in 
// range_map; you would do emplace(encaps (true or false), std::piecewise_construct, 
// std::forward_as_tuple(left key args), std::forward_as_tuple(right key args),
// std::forward_as_tuple(mapped val args)
// thanks to online resource for showing how to do this!
namespace temp {

template<std::size_t...> struct index_tuple{};
 
template<std::size_t I, typename IndexTuple, typename... Types>
struct make_indices_impl;
 
template<std::size_t I, std::size_t... Indices, typename T, typename... Types>
struct make_indices_impl<I, index_tuple<Indices...>, T, Types...>
{
  typedef typename
    make_indices_impl<I + 1, 
                      index_tuple<Indices..., I>, 
                      Types...>::type type;
};
 
template<std::size_t I, std::size_t... Indices>
struct make_indices_impl<I, index_tuple<Indices...> >
{
  typedef index_tuple<Indices...> type;
};
 
template<typename... Types>
struct make_indices 
  : make_indices_impl<0, index_tuple<>, Types...>
{};

}

//indicate that left point and length rather than left and right points are
//passed to the constructor

struct length_construct_type {};

// range_map_item; basic range_type stored in range_map; here the left key and the
// right key delimiting the range [left, right) are stored, as well as the mapped
// value; the mapped value can be modified if the range_map_item is not const; this
// is contrary to the c++11 standards for the pair stored in map and multi-map
template<class kty, class ty>
class range_map_item {

  typedef range_map_item<kty, ty> _myt;

public:
        range_map_item(const kty& left, const kty& right) 
		: left(left), right(right), val() {}  
  
	range_map_item(const kty& left, const kty& right, const ty& val) 
		: left(left), right(right), val(val) {}
        
        //length constructors
        range_map_item(length_construct_type, const kty& left, const kty& length) 
		: left(left), right(left+length), val() {}  
  
        range_map_item(length_construct_type, const kty& left, const kty& length,
        const ty& val) : left(left), right(left+length), val(val) {}
        

        //c++11 constructors
        range_map_item(kty&& left, kty&& right) 
		: left(std::forward<kty>(left)), right(std::forward<kty>(right)), val() {}
        
        range_map_item(const kty& left, const kty& right, ty&& val) 
		: left(left), right(right), val(std::forward<ty>(val)) {}
        
	range_map_item(kty&& left, kty&& right, const ty& val) 
		: left(std::forward<kty>(left)), right(std::forward<kty>(right)), val(val) {}
        
	range_map_item(kty&& left, kty&& right, ty&& val) 
		: left(std::forward<kty>(left)), right(std::forward<kty>(right)), val(std::forward<ty>(val)) {}
        
        //length constructors
        range_map_item(length_construct_type, kty&& left, const kty& length) 
		: left(std::forward<kty>(left)), right(left+length), val() {}
        
        range_map_item(length_construct_type, const kty& left, const kty& length,
            ty&& val) : left(left), right(left+length), val(std::forward<ty>(val)) {}
        
        range_map_item(length_construct_type, kty&& left, const kty& length, 
            const ty& val) : left(std::forward<kty>(left)), right(left+length), val(val) {}
        
        range_map_item(length_construct_type, kty&& left, const kty& length,
            ty&& val) : left(std::forward<kty>(left)), right(left+length), 
                val(std::forward<ty>(val)) {}
        
        //piecewise constructor
        template<class... left_args, class... right_args, class... mapped_args> 
        range_map_item(std::piecewise_construct_t pwc,
                std::tuple<left_args...> left_tpl, std::tuple<right_args...> right_tpl,
                std::tuple<mapped_args...> mapped_tpl) 
        : range_map_item(left_tpl, right_tpl, mapped_tpl,
                typename temp::make_indices<left_args...>::type(),
                typename temp::make_indices<right_args...>::type(),
                typename temp::make_indices<mapped_args...>::type())
        {}
        
	const kty& get_left() const {
		return left;
	}

	const kty& get_right() const {
		return right;
	}
        
        kty width() const {
                return right - left;
        }

	ty& mapped() {
		return val;
	}
        
        const ty& mapped() const {
		return val;
	}

private:
	range_map_item() {}
        
        template<class... args1, class... args2, class... args3,
                std::size_t... indices1, std::size_t... indices2, std::size_t... indices3>
        range_map_item(std::tuple<args1...> tuple1, std::tuple<args2...> tuple2, std::tuple<args3...> tuple3,
                temp::index_tuple<indices1...>, temp::index_tuple<indices2...>, temp::index_tuple<indices3...>)
                : left(std::forward<args1>(std::get<indices1>(tuple1))...),
                right(std::forward<args2>(std::get<indices2>(tuple2))...),
                val(std::forward<args3>(std::get<indices3>(tuple3))...)
                {}

	kty left;
	kty right;
	ty val;


};

// constructs range_map_item to be forwarded
template<class kty, class ty>
range_map_item<kty, ty> make_range_map_item(kty&& left, kty&& right, ty&& val) {
    return range_map_item<kty, ty>(std::forward<kty>(left), std::forward<kty>(right), std::forward<ty>(val));
}

}

#endif	/* RM_BASE_HPP */
