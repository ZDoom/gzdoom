// (C) Beneficii.  Released under Creative Commons license.

#ifndef BENEFICII_RANGE_MAP_RANGE_MAP_HPP
#define	BENEFICII_RANGE_MAP_RANGE_MAP_HPP

#include "range_map/internal/rm_iter.h"
#include <algorithm>
#include <stack>
#include <initializer_list>
#include <memory>
#include <utility>

namespace beneficii {
    
    template<class _kty, class _ty, class _compare = std::less<_kty>,
            class _alloc_type = std::allocator<range_map_item<_kty, _ty>>,
										bool _allow_encaps_matching_sets = false>
    class range_map {
            
            typedef range_map<_kty, _ty, _compare, _alloc_type> _myt;
            typedef _range_node<_kty, _ty> _node;
            typedef typename _alloc_type::template rebind<_node>::other _node_allocator;
            typedef typename _node_allocator::pointer _nodeptr;
        
        public:
            
            typedef _kty key_type;
            typedef _ty mapped_type;
            typedef _compare key_compare;
            typedef _alloc_type allocator_type;
            typedef range_map_item<_kty, _ty> range_type;
            typedef typename _node::_point value_type;
            typedef typename allocator_type::size_type size_type;
            typedef typename allocator_type::difference_type difference_type;
            typedef value_type* pointer;
            typedef const value_type* const_pointer;
            typedef value_type& reference;
            typedef const value_type& const_reference;
            typedef _rm_iterator<_myt> iterator;
            typedef _rm_const_iterator<_myt> const_iterator;
            typedef std::reverse_iterator<iterator> reverse_iterator;
            typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
            
            //destructor
            ~range_map() {
                _clear_container();
                delete _root;
            }
            
            //default constructor
            range_map() : _comp(), _alloc(),
                    _root(new _nodeptr(nullptr)), _cur_size(0) {}
           
            //constructor that takes comparator object
            //COMP = comparator object
            explicit range_map(const key_compare& _comp) : _comp(_comp), _alloc(),
                    _root(new _nodeptr(nullptr)), _cur_size(0) {}
            
            //constructor that takes allocator object
            //ALLOC = allocator object
            explicit range_map(const allocator_type& _alloc) : _comp(), _alloc(_node_allocator(_alloc)),
                    _root(new _nodeptr(nullptr)), _cur_size(0) {}
            
            //constructor that takes comparator and allocator object
            //COMP = comparator object
            //ALLOC = allocator object
            range_map(const key_compare& _comp, allocator_type _alloc) : _comp(_comp),
                    _alloc(_node_allocator(_alloc)), _root(new _nodeptr(nullptr), _cur_size(0)) {}
            
            //copy constructor
            //RGT = container to be copied
            range_map(const _myt& _rgt) : _comp(_rgt._comp),
                    _alloc(std::allocator_traits<_node_allocator>::
                    select_on_container_copy_construction(_rgt._alloc)),
                    _root(new _nodeptr(nullptr)), _cur_size(0) {
                _do_copy_container(_rgt);
            }
            
            //copy assignment operator
            // RGT = container to be copied
            // RETURN VALUE = reference to this container
            _myt& operator=(const _myt& _rgt) {
                if(this != &_rgt) {
                    _clear_container();
                    _comp = _rgt._comp;
                    _copy_assign(_rgt, 
                          typename std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment());
                    _do_copy_container(_rgt);
                }
                return *this;
            }
            
            //move constructor
            //RGT = container to be moved
            range_map(_myt&& _rgt) {
                _comp = std::move(_rgt._comp);
                _root = new _nodeptr(nullptr);
                _move_assign(std::forward<_myt>(_rgt), std::true_type());
            }
            
            //move constructor that takes allocator object
            //RGT = container to be moved
            //ALLOC = allocator to copied
            range_map(_myt&& _rgt, const allocator_type& _all) {
                _comp = std::move(_rgt._comp);
                _alloc = _node_allocator(_all);
                _root = new _nodeptr(nullptr);
                _cur_size = 0;
                _move_assign(std::forward<_myt>(_rgt), std::false_type());
            }
            
            //move assignment operator
            //RGT = container to be moved
            //RETURN VALUE = reference to this container
            _myt& operator=(_myt&& _rgt) {
                if(this != &_rgt) {
                    _clear_container();
                    _comp = std::move(_rgt._comp);
                    _move_assign(std::forward<_myt>(_rgt), 
                            typename std::allocator_traits<allocator_type>::propagate_on_container_move_assignment());
                }
                return *this;
            }
            
            //initialization construtor
            //LIST = initializer list
            range_map(std::initializer_list<range_type> _list) : _comp(), _alloc(),
                        _root(new _nodeptr(nullptr)), _cur_size(0) {
                for(auto& _i : _list) {
                    insert(true, _i);
                }
            }
            
            //initialization constructor that takes comparator object
            //LIST = initializer list
            //COMP = comparator object
            range_map(std::initializer_list<range_type> _list, const key_compare& _comp) : 
                        _comp(_comp), _alloc(), _root(new _nodeptr(nullptr)), _cur_size(0) {
                for(auto& _i : _list) {
                    insert(true, _i);
                }
            }
            
            //initialization constructor that takes allocator object
            //list = initializer list
            //comp = comparator object
            range_map(std::initializer_list<range_type> _list, const allocator_type& _alloc) : 
                        _comp(), _alloc(_node_allocator(_alloc)), _root(new _nodeptr(nullptr)), _cur_size(0) {
                for(auto& _i : _list) {
                    insert(true, _i);
                }
            }
            
            //initialization constructor that takes comparator and allocator objects
            //LIST = initializer list
            //COMP = comparator object
            //ALLOC = allocator object
            range_map(std::initializer_list<range_type> _list, const key_compare& _comp, const allocator_type& _alloc) : 
                        _comp(_comp), _alloc(_node_allocator(_alloc)), _root(new _nodeptr(nullptr)), _cur_size(0) {
                for(auto& _i : _list) {
                    insert(true, _i);
                }
            }
            
            //initialization assignment operator (clears contents of this container)
            //LIST = initializer list
            //RETURN VALUE = reference to this container
            _myt& operator=(std::initializer_list<range_type> _list) {
                _clear_container();
                for(auto& _i: _list) {
                    insert(true, _i);
                }
                return *this;
            }
            
            //initialization addition operator (without clearing contents of this container)
            //LIST = initializer list
            //RETURN VALUE = reference to this container
            _myt& operator+=(std::initializer_list<range_type> _list) {
                for(auto& _i: _list) {
                    insert(true, _i);
                }
                return *this;
            }
            
            //C++11 swap
            //RGT = container to be swapped with
            void swap(_myt& _rgt) {
                _swap2(_rgt, 
                  typename std::allocator_traits<allocator_type>::propagate_on_container_swap());
            }
            
            //returns copy of allocator object
            //RETURN VALUE = copy of allocator object
            allocator_type get_allocator() const {
                return allocator_type(_alloc);
            }
            
            //returns copy of comparator object
            //RETURN VALUE = copy of comaprator object
            key_compare key_comp() const {
                return _comp;
            }
            
            //returns maximum number of items as specified by allocator
            //RETURN VALUE = maximum possible size of container (as determined by allocator))
            size_type max_size() const {
                return _alloc.max_size();
            }
            
            //returns current size of container
            //RETURN VALUE = current size of container, in terms of number of items
            size_type size() const {
                return _cur_size;
            }
            
            //tests whether container is empty
            //RETURN VALUE = true if container is empty, false if not
            bool empty() const {
                return *_root == nullptr;
            }
            
            //returns iterator to first item in sequence
            //RETURN VALUE = iterator to first item in container
            iterator begin() {
                return empty() ? end() : 
                    iterator(_node::_min(*_root), false);
            }
            
            //returns const_iterator to first item in sequence
            //RETURN VALUE = const_iterator to first item in container
            const_iterator begin() const {
                return empty() ? end() : 
                    const_iterator(_node::_min(*_root), false);
            }
            
            //returns const_iterator to first item in sequence
            //RETURN VALUE = const_iterator to first item in container
            const_iterator cbegin() const {
                return empty() ? cend() : 
                    const_iterator(_node::_min(*_root), false);
            }
            
            //returns reverse_iterator to last item in sequence
            //RETURN VALUE = reverse_iterator to first item in container
            reverse_iterator rbegin() {
                return empty() ? rend() : 
                    reverse_iterator(iterator(_node::_max(*_root), true));
            }
            
            //returns const_reverse_iterator to last item in sequence
            //RETURN VALUE = const_reverse_iterator to first item in container
            const_reverse_iterator rbegin() const {
                return empty() ? rend() : 
                    const_reverse_iterator(const_iterator(_node::_max(*_root), true));
            }
            
            //returns const_reverse_iterator to last item in sequence
            //RETURN VALUE = const_reverse_iterator to first item in container
            const_reverse_iterator crbegin() const {
                return empty() ? crend() : 
                    const_reverse_iterator(const_iterator(_node::_max(*_root), true));
            }
            
            //returns past-end iterator
            //RETURN VALUE = iterator to past-end point
            iterator end() {
                return iterator(nullptr, false);
            }
            
            //returns past-end const_iterator
            //RETURN VALUE = const_iterator to past-end point
            const_iterator end() const {
                return const_iterator(nullptr, false);
            }
            
            //returns past-end const_iterator
            //RETURN VALUE = const_iterator to past-end point
            const_iterator cend() const {
                return const_iterator(nullptr, false);
            }
            
            //returns past-begin reverse_iterator
            //RETURN VALUE = reverse_iterator to past-begin point
            reverse_iterator rend() {
                return reverse_iterator(iterator(nullptr, false));
            }
            
            //returns past-begin const_reverse_iterator
            //RETURN VALUE = const_reverse_iterator to past-begin point
            const_reverse_iterator rend() const {
                return const_reverse_iterator(const_iterator(nullptr, false));
            }
            
            //returns past-begin const_reverse_iterator
            //RETURN VALUE = const_reverse_iterator to past-begin point
            const_reverse_iterator crend() const {
                return const_reverse_iterator(const_iterator(nullptr, false));
            }
            
            // returns iterator to opposite end point of current iterator
            iterator get_opposite_end(iterator _cpy) {
              return _cpy == end() ? end() : iterator(*_cpy, !_cpy->is_rgt_pt());
            }
            
            // returns const_iterator to opposite end point of current const_iterator
            const_iterator get_opposite_end(const_iterator _cpy) const {
              return _cpy == end() ? end() : const_iterator(*_cpy, !_cpy->is_rgt_pt());
            }
            
            //inserts item; _encaps specifies what is to be done in cases where the
            //inserted range would encompass the ranges that are already present
            //true specifies that they would become part of the subtree of the
            //inserted range, while false specifies that they would simply be deleted
            //this inserts item by copying it
            //ENCAPS = whether to encapsulate pre-existing items encompassed by range
            //VAL = range_map_item to be copied
            //RETURN VALUE = pair of iterator to item inserted if successful or if not,
            //    then past-end iterator if overlaps pre-existing items, or iterator to
            //    pre-existing item with exact same range; to bool (true if successful,
            //    false if not)
            std::pair<iterator, bool> insert(bool _encaps, const range_type& _val) {
                _nodeptr _ins = nullptr;
                std::pair<iterator, bool> _ret;
                _ret.second = _insert1(&_ins, _encaps, _val);
                _ret.first = _ins == nullptr ? end() : iterator(_ins, false);
                return _ret;
            }
            
            //inserts item; _encaps specifies what is to be done in cases where the
            //inserted range would encompass the ranges that are already present
            //true specifies that they would become part of the subtree of the
            //inserted range, while false specifies that they would simply be deleted
            //this inserts item by moving it
            //ENCAPS = whether to encapsulate pre-existing items encompassed by range
            //VAL = range_map_item to be moved
            //RETURN VALUE = pair of iterator to item inserted if successful or if not,
            //    then past-end iterator if overlaps pre-existing items, or iterator to
            //    pre-existing item with exact same range; to bool (true if successful,
            //    false if not)
            std::pair<iterator, bool> insert(bool _encaps, range_type&& _val) {
                _nodeptr _ins = nullptr;
                std::pair<iterator, bool> _ret;
                _ret.second = _insert1(&_ins, _encaps, std::forward<range_type>(_val));
                _ret.first = _ins == nullptr ? end() : iterator(_ins, false);
                return _ret;
            }
            
            //emplaces item; _encaps specifies what is to be done in cases where the
            //inserted range would encompass the ranges that are already present
            //true specifies that they would become part of the subtree of the
            //inserted range, while false specifies that they would simply be deleted
            //this inserts item by moving it
            //ENCAPS = whether to encapsulate pre-existing items encompassed by range
            //ARGS = set of arguments to construct range_map_item in place
            //RETURN VALUE = pair of iterator to item inserted if successful or if not,
            //    then past-end iterator if overlaps pre-existing items, or iterator to
            //    pre-existing item with exact same range; to bool (true if successful,
            //    false if not)
            //(std::piecewise_construct may be used to have arguments for each member
            //of range_map_item placed in tuples, using std::forward_as_tuple)
            template<class ..._args>
            std::pair<iterator, bool> emplace(bool _encaps, _args&&... _arg) {
                _nodeptr _ins = nullptr;
                std::pair<iterator, bool> _ret;
                _ret.second = _insert1(&_ins, _encaps, std::forward<_args>(_arg)...);
                _ret.first = _ins == nullptr ? end() : iterator(_ins, false);
                return _ret;
            }
            
            //erases item as specified by const_iterator
            //_decaps specifies what to do with subtree of item to be erased
            //true specifies that the items in the subtree be "decapsulated," moved
            //out of subtree, while false specifies they be erased with the item
            //ITER = iterator to start or end point of item to be erased
            //DECAPS = Whether to delete all child subtrees of item to be erased or
            //    to "decapsulate" them into the subtree encompassing the item to be
            //    erased
            iterator erase(const_iterator _iter, bool _decaps) {
                _nodeptr _n = *_iter;
                _n = _delete2(_n, _root, _decaps);
                iterator _ret = _n == nullptr ? end() : iterator(_n, false);
                return _ret;
            }
            
            //clears all items
            void clear() {
                _clear_container();
            }
            
            //gets first point (start or end) >= _val
            //VAL = key to find lower_bound of
            //RETURN VALUE = iterator to item if found or past-end point
            iterator lower_bound(const key_type& _val) {
                _nodeptr _n;
                bool _is_rgt_pt = _lbound(_val, &_n);
                return iterator(_n, _is_rgt_pt);
            }
            
            //gets first point (start or end) >= _val
            //VAL = key to find lower_bound of
            //RETURN VALUE = const_iterator to item if found or past-end point
            const_iterator lower_bound(const key_type& _val) const {
                _nodeptr _n;
                bool _is_rgt_pt = _lbound(_val, &_n);
                return const_iterator(_n, _is_rgt_pt);
            }
            
            //gets first point (start or end) > _val
            //VAL = key to find upper_bound of
            //RETURN VALUE = iterator to item if found or past-end point
            iterator upper_bound(const key_type& _val) {
                _nodeptr _n;
                bool _is_rgt_pt = _ubound(_val, &_n);
                return iterator(_n, _is_rgt_pt);
            }
            
            //gets first point (start or end) > _val
            //VAL = key to find upper_bound of
            //RETURN VALUE = const_iterator to item if found or past-end point
            const_iterator upper_bound(const key_type& _val) const {
                _nodeptr _n;
                bool _is_rgt_pt = _ubound(_val, &_n);
                return const_iterator(_n, _is_rgt_pt);
            }
            
            //finds all ranges that intersect _val and puts them on stack
            //the deepest range is at the top
            //VAL = key to find ranges that intersect it
            //RETURN VALUE = stack of iterators (deepest at top) to starting point
            //   of ranges that intersect val (iterator)
            std::stack<iterator> find_ranges(const key_type& _val) {
                if(*_root == nullptr) return std::stack<iterator>();
                std::stack<iterator> _ret;
                _nodeptr _n = *_root;
                while(1) {
                    if(_comp(_val, _n->_range.get_left())) {
                        _n = (_nodeptr) _n->_left;
                        if(_n == nullptr) break;
                        else continue;
                    }
                    if(!_comp(_val, _n->_range.get_right())) {
                        _n = (_nodeptr) _n->_right;
                        if(_n == nullptr) break;
                        else continue;
                    }
                    _ret.push(iterator(_n, false));
                    _n = (_nodeptr) _n->_subtree;
                    if(_n == nullptr) break;
                }
                return _ret;
            }
            
            //finds all ranges that intersect _val and puts them on stack
            //the deepest range is at the top
            //VAL = key to find ranges that intersect it
            //RETURN VALUE = stack of iterators (deepest at top) to starting point
            //    of ranges that intersect val (const_iterator)
            std::stack<const_iterator> find_ranges(const key_type& _val) const {
                if(*_root == nullptr) return std::stack<const_iterator>();
                std::stack<const_iterator> _ret;
                _nodeptr _n = *_root;
                while(1) {
                    if(_comp(_val, _n->_range.get_left())) {
                        _n = (_nodeptr) _n->_left;
                        if(_n == nullptr) break;
                        else continue;
                    }
                    if(!_comp(_val, _n->_range.get_right())) {
                        _n = (_nodeptr) _n->_right;
                        if(_n == nullptr) break;
                        else continue;
                    }
                    _ret.push(const_iterator(_n, false));
                    _n = (_nodeptr) _n->_subtree;
                    if(_n == nullptr) break;
                }
                return _ret;
            }
            
            // returns iterator to next start point (i.e. next range) if it exists
            // if iterator is at end() go to leftmost point of the range map
            iterator next_range(iterator _i) {
              if(_i == end()) return begin();
              do ++_i; while(_i != end() && _i->is_rgt_pt());
              return _i;
            }
            
            const_iterator next_range(const_iterator _i) const {
              if(_i == end()) return begin();
              do ++_i; while(_i != end() && _i->is_rgt_pt());
              return _i;
            }
            
            static bool next_range(iterator& _start, iterator _stop) {
              if(_start == _stop) return false;
              do ++_start; while(_start != _stop && _start->is_rgt_pt());
              if(_start == _stop) return false;
              return true;
            }
            
            static bool next_range(const_iterator& _start, const_iterator _stop) {
              if(_start == _stop) return false;
              do ++_start; while(_start != _stop && _start->is_rgt_pt());
              if(_start == _stop) return false;
              return true;
            }
            
            //return iterator to previous end pt (i.e. prev range) if it exists
            //if not, return end(); if currently end(), return rightmost point of
            //the range map
            iterator prev_range(iterator _i) {
              if(_i == end()) return iterator(_node::_max(*_root), true);
              if(_i == begin()) return end();
              do --_i; while(_i != begin() && !_i->is_rgt_pt());
              return _i == begin() ? end() : _i;
            }
            
            const_iterator prev_range(const_iterator _i) const {
              if(_i == end()) return iterator(_node::_max(*_root), true);
              if(_i == begin()) return end();
              do --_i; while(_i != begin() && !_i->is_rgt_pt());
              return _i == begin() ? end() : _i;
            }
            
            //returns if iterators a and b are at opposite ends of one another
            //on the same range
            bool on_same_range(const_iterator _a, const_iterator _b) const {
                if(_a == end() || _b == end()) return false;
                return !(_comp(_a->start_pt(), _b->start_pt()) || _comp(_b->start_pt(), _a->start_pt()) ||
                         _comp(_a->end_pt(), _b->end_pt()) || _comp(_b->end_pt(), _a->end_pt()));
            }
            
        private:
            
            // if allocator is set to propagate on copy assign, then propagate it
            void _copy_assign(const _myt& _rgt, std::true_type) {
                _alloc = _rgt._alloc;
            }
            
            // if allocator is not set to propagate on copy assign, then do nothing
            void _copy_assign(const _myt& _rgt, std::false_type) {}
            
            // if allocator is set to propagate on move assign, then propagate it
            // and since the way the nodes are allocated are in line with the 
            // allocator being propagated, simply move the root pointer and other
            // relevant data
            void _move_assign(_myt&& _rgt, std::true_type) {
                _alloc = std::move(_rgt._alloc);
                *_root = *_rgt._root;
                *_rgt._root = nullptr;
                _cur_size = _rgt._cur_size;
                _rgt._cur_size = 0;
            }
            
            // if allocator is not set to propagate on move assign, then things are
            // more complicated.  if the allocators are equivalent, then just like
            // with propagation set, the way the two containers store things are
            // compatible and you can simply move the root pointer and other relevant
            // data.
            // if not, then each item in the container must be move assigned
            void _move_assign(_myt&& _rgt, std::false_type) {
                if(_alloc == _rgt._alloc) {
                    *_root = *_rgt._root;
                    *_rgt._root = nullptr;
                    _cur_size = _rgt._cur_size;
                    _rgt._cur_size = 0;
                } else {
                    _do_move_container(std::move(_rgt));
                }
            }
            
            // if allocator is set to propagate on swap, then you can simply move
            // the basic variables around without expensive operations.
            void _swap2(_myt& _rgt, std::true_type) {
                if(this != &_rgt) {
                    _myt _temp;
                    _temp._comp = std::move(_comp);
                    _temp._alloc = std::move(_alloc);
                    *_temp._root = *_root;
                    _temp._cur_size = _cur_size;
                    
                    _comp = std::move(_rgt._comp);
                    _alloc = std::move(_rgt._alloc);
                    *_root = *_rgt._root;
                    _cur_size = _rgt._cur_size;
                    
                    _rgt._comp = std::move(_temp._comp);
                    _rgt._alloc = std::move(_temp._alloc);
                    *_rgt._root = *_temp._root;
                    _rgt._cur_size = _temp._cur_size;
                    
                    *_temp._root = nullptr;
                    _temp._cur_size = 0;
                }
            }
            
            // if allocator is not set to propagate on swap but the two containers
            // have equivalent allocators, then do just like if propagation is set
            // otherwise, do expensive (in terms of time) move ops
            void _swap2(_myt& _rgt, std::false_type) {
                if(this != &_rgt) {
                    if(_alloc == _rgt._alloc) {
                        _myt _temp;
                        _temp._comp = std::move(_comp);
                        *_temp._root = *_root;
                        _temp._cur_size = _cur_size;

                        _comp = std::move(_rgt._comp);
                        *_root = *_rgt._root;
                        _cur_size = _rgt._cur_size;

                        _rgt._comp = std::move(_temp._comp);
                        *_rgt._root = *_temp._root;
                        _rgt._cur_size = _temp._cur_size;

                        *_temp._root = nullptr;
                        _temp._cur_size = 0;
                    } else {
                        //if allocators don't propagate and the allocators are not
                        //equal, then move the containers around item by item
                        //naturally, this will take the longest out of all swaps.
                        //also swap the comparators.
                        _myt _temp;
                        _temp._comp = std::move(_rgt._comp);
                        _rgt._comp = std::move(this->_comp);
                        this->_comp = std::move(_temp._comp);
                        _temp._do_move_container(std::move(_rgt));
                        _rgt._do_move_container(std::move(*this));
                        this->_do_move_container(std::move(_temp));
                    }
                }
            }
            
            // for internal use, to construct/destroy a range
            allocator_type& _range_alloc() {
                return *(allocator_type*)(&_alloc);
            }
            
            // creates node, setting up everything except range
            // uses _node_allocator type to allocate memory for _node
            // object and return its pointer.
            _nodeptr _create_node() {
                _nodeptr _ret;
                try {
                    _ret = _alloc.allocate(1);
                }
                catch(std::bad_alloc&) {
                    return nullptr;
                }
                if(_ret != nullptr) {
                    //initializing all members except range, which is constructed
                    //in the next couple functions.
                    _reset_node(_ret);
                    _ret->_subtree = nullptr;
                    _ret->_end = (void*) _alloc.address(*_ret);
                    _ret->_start = (void*) _alloc.address(*_ret);
                }
                return _ret;
            }

            // constructs range in node by copy constructor
            // uses allocator_type (allocator for range_type) in order
            // to properly construct the range itself.
            // uses pointer to range_type object, not node object, to construct
            // the range in the right place in memory.
            void _construct_range(_nodeptr _n, const range_type& _arg) {
                _range_alloc().construct(&_n->_range, _arg);
            }
            
            // constructs range in node by any other set of arguments (incl. move construct)
            // uses allocator_type (allocator for range_type) in order
            // to properly construct the range itself.
            // uses pointer to range_type object, not node object, to construct
            // the range in the right place in memory.
            template<class ..._args> 
            void _construct_range(_nodeptr _n, _args&&... _arg) {
                _range_alloc().construct(&_n->_range, std::forward<_args>(_arg)...);
            }
            
            // copies container range by range
            void _do_copy_container(const _myt& _rgt) {
                for(const_iterator _i = _rgt.begin(); _i != _rgt.end(); _i = next_range(_i)) {
                    _nodeptr _n = _create_node();
                    if(_n == nullptr) return;
                    _construct_range(_n, _i->range());
                    _insert2(&_n, _root, true);
                }
            }
            
            // moves the contents of another container over item by item 
            void _do_move_container(_myt&& _rgt) {
                while(!_rgt.empty()) {
                    _nodeptr _n = _create_node();
                    if(_n == nullptr) return;
                    _construct_range(_n, std::move((*_rgt._root)->_range));
                    _insert2(&_n, _root, true);
                    _rgt._delete2(*_rgt._root, _rgt._root, true);
                }
            }
            
            // deletes all contents of container
            void _clear_container() {
                while(*_root != nullptr) {
                    _delete2(*_root, _root, true);
                }
            }
            
            // destroys the range_type object in the node object and deallocates
            // the node object's memory
            void _delete_node(_nodeptr _n) {
                _range_alloc().destroy(&_n->_range);
                _alloc.deallocate(_n, 1);
            }
            
            // prepares node to be inserted in tree; if node was already in a
            // tree, "freshens" it up to be inserted in another tree
            void _reset_node(_nodeptr _n) {
                _n->_color = _node::_red;
                _n->_left = nullptr;
                _n->_parent = nullptr;
                _n->_right = nullptr;
            }
            
            // see lower_bound for details
            bool _lbound(const key_type& _val, _nodeptr* _nod) const {
                if(*_root == nullptr) return false;
                _nodeptr _n = *_root;
                _nodeptr _last = _n;
                bool _ret = false;
                while(1) {
                    if(_comp(_n->_range.get_right(), _val)) {
                        if(_n->_right == nullptr) {
                            *_nod = _node::_next(_n);
                            if(_n == nullptr) break;
                            if(_n == _last) {
                                _ret = true;
                            } else {
                                _ret = false;
                            }
                            break;
                        }
                        _n = (_nodeptr) _n->_right;
                        continue;
                    }
                    if(!_comp(_n->_range.get_left(), _val)) {
                        if(_n->_left == nullptr) {
                            *_nod = _n;
                            _ret = false;
                            break;
                        }
                        _n = (_nodeptr) _n->_left;
                        continue;
                    }
                    if(_n->_subtree == nullptr) {
                        *_nod = _n;
                        _ret = true;
                        break;
                    }
                    _last = _n;
                    _n = (_nodeptr) _n->_subtree;
                }
                
                return _ret;
            }
            
            bool _ubound(const key_type& _val, _nodeptr* _nod) const {
                if(*_root == nullptr) return false;
                _nodeptr _n = *_root;
                _nodeptr _last = _n;
                bool _ret = false;
                while(1) {
                    if(!_comp(_val, _n->_range.get_right())) {
                        if(_n->_right == nullptr) {
                            *_nod = _node::_next(_n);
                            if(_n == nullptr) break;
                            if(_n == _last) {
                                _ret = true;
                            } else {
                                _ret = false;
                            }
                            break;
                        }
                        _n = (_nodeptr) _n->_right;
                        continue;
                    }
                    if(_comp(_val, _n->_range.get_left())) {
                        if(_n->_left == nullptr) {
                            *_nod = _n;
                            _ret = false;
                            break;
                        }
                        _n = (_nodeptr) _n->_left;
                        continue;
                    }
                    if(_n->_subtree == nullptr) {
                        *_nod = _n;
                        _ret = true;
                        break;
                    }
                    _last = _n;
                    _n = (_nodeptr) _n->_subtree;
                }
                
                return _ret;
            }
            
            // replaces node in tree with child node
            void _node_replace(_nodeptr _old, _nodeptr _new) {
                if(_old->_parent == nullptr) {
                    *_root = _new;
                } else {
                    if(_old == (_nodeptr) _node::_get_parent(_old)->_left) {
                        ((_nodeptr) _old->_parent)->_left = (void*) _new;
                    } else if(_old == (_nodeptr) _node::_get_parent(_old)->_right) {
                        ((_nodeptr) _old->_parent)->_right = (void*) _new;
                    } else {
                        ((_nodeptr) _old->_parent)->_subtree = (void*) _new;
                    }
                }
                if(_new != nullptr) {
                    _new->_parent = _old->_parent;
                }
            }
            
            // rotates nodes left around a pivot _n
            void _rotate_left(_nodeptr _n) {
                _nodeptr _z = (_nodeptr) _n->_right;
                _node_replace(_n, _z);
                _n->_right = _z->_left;
                if(_n->_right != nullptr)
                    ((_nodeptr) _n->_right)->_parent = (void*) _n;
                _z->_left = (void*) _n;
                _n->_parent = (void*) _z;
            }
            
            // rotates nodes right around a pivot _n
            void _rotate_right(_nodeptr _n) {
                _nodeptr _z = (_nodeptr) _n->_left;
                _node_replace(_n, _z);
                _n->_left = _z->_right;
                if(_n->_left != nullptr)
                    ((_nodeptr) _n->_left)->_parent = (void*) _n;
                _z->_right = (void*) _n;
                _n->_parent = (void*) _z;
            }
            
            // subroutines to keep current rb-tree balanced with each insertion
            
            //if root, make color black (root of each tree always black)
            //otherwise, continue.
            void _inscase1(_nodeptr _n) {
                if(_node::_is_root(_n))
                    _n->_color = _node::_black;
                else
                    _inscase2(_n);
            }
            
            //if parent is red, then that means there is grandparent
            void _inscase2(_nodeptr _n) {
                if(_node::_get_color(_node::_get_parent(_n)) == _node::_red)
                    _inscase3(_n);
            }
            
            //if aunt is red, then that means there is aunt and tree at this
            //point is balanced; just set colors and start back over with g'pa.
            //otherwise, continue.
            void _inscase3(_nodeptr _n) {
                if(_node::_get_color(_node::_aunt(_n)) == _node::_red) {
                    _node::_get_parent(_n)->_color = _node::_black;
                    _node::_aunt(_n)->_color = _node::_black;
                    _node::_grandparent(_n)->_color = _node::_red;
                    _inscase1(_node::_grandparent(_n));
                } else
                    _inscase4(_n);
            }
            
            //if path from n to ole g'ma is crooked, then straighten it out by making
            //n's parent into n's child and n's grandparent into n's child, maintraining
            //n and n's new child on same side of g'ma.  Switch focus to n's former
            //parent and now new child.  Otherwise, do nothing and keep focus on n.
            //Either way continue to next case.
            void _inscase4(_nodeptr _n) {
                if(_n == (_nodeptr) _node::_get_parent(_n)->_right &&
                    _node::_get_parent(_n) == (_nodeptr) _node::_grandparent(_n)->_left) {
                    _rotate_left(_node::_get_parent(_n));
                    _n = (_nodeptr) _n->_left;
                } else if(_n == (_nodeptr) _node::_get_parent(_n)->_left &&
                    _node::_get_parent(_n) == (_nodeptr) _node::_grandparent(_n)->_right) {
                    _rotate_right(_node::_get_parent(_n));
                    _n = (_nodeptr) _n->_right;
                }
                
                _inscase5(_n);
            }
            
            //n is red here, as well as its parent.  g'ma's color unknown.
            //make n's parent black and its g'ma red (so now known).
            //if n is now old n's child and has a straight path leading to its g'ma,
            //then make n's g'ma its sibling and n's parent the parent of both.
            //resulting in new parent being black and its children red.
            void _inscase5(_nodeptr _n) {
                _node::_get_parent(_n)->_color = _node::_black;
                _node::_grandparent(_n)->_color = _node::_red;
                if(_n == (_nodeptr) _node::_get_parent(_n)->_left 
                   && _node::_get_parent(_n) == (_nodeptr) _node::_grandparent(_n)->_left) {
                    _rotate_right(_node::_grandparent(_n));
                } else if(_n == (_nodeptr) _node::_get_parent(_n)->_right 
                   && _node::_get_parent(_n) == (_nodeptr) _node::_grandparent(_n)->_right) {
                    _rotate_left(_node::_grandparent(_n));
                }
            }
            
            // subroutines to keep current rb-tree balanced with each removal
            
            //only continue if n is not root.  n would be root only if it has at most
            //one child, so if one child, simply replace root with child.
            //n may be either black or red.
            void _delcase1(_nodeptr _n) {
                if(!_node::_is_root(_n))
                    _delcase2(_n);
            }
            
            //we now know n is not root and has a parent.  if sibling is red,
            //then it exists.  Make parent red and sibling black, then rotate
            //parent such that n's black sibling becomes its black g'ma and red
            //parent forming the middle part of a straight line from n to its new
            //g'ma.  n may get a new sibling, one of the old sibling's former
            //children (which was replaced by n's parent).  Otherwise, sibling does
            //exist because n was originally black (and not the root) before being set to its child's
            //color; if it had no sibling, then n would be red and these cases
            //would not have been called.  (If original sibling was red, its children
            //were black, so here n's new sibling will also be black.)
            void _delcase2(_nodeptr _n) {
                if(_node::_get_color(_node::_sibling(_n)) == _node::_red) {
                    _node::_get_parent(_n)->_color = _node::_red;
                    _node::_sibling(_n)->_color = _node::_black;
                    if(_n == (_nodeptr) _node::_get_parent(_n)->_left)
                        _rotate_left(_node::_get_parent(_n));
                    else
                        _rotate_right(_node::_get_parent(_n));
                } 
                
                _delcase3(_n);
            }
            
            //parent and sibling exist, though the sibling's children may not.
            //if parent, sibling, and sibling's children are black (or both
            //don't exist or 1 is black and the other doesn't exist), 
            //then set sibling to red and restart process with n's parent.
            //otherwise continue.
            void _delcase3(_nodeptr _n) {
                if(_node::_get_color(_node::_get_parent(_n)) == _node::_black &&
                        _node::_get_color(_node::_sibling(_n)) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_left) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_right) == _node::_black) {
                    _node::_sibling(_n)->_color = _node::_red;
                    _delcase1(_node::_get_parent(_n));
                } else
                    _delcase4(_n);
            }
            
            //if parent is red and other requirements, make it black and the sibling red.
            //Sibling will have 2 black children as specified in requirements and we're done.
            //Otheriwse/continue.
            void _delcase4(_nodeptr _n) {
                if(_node::_get_color(_node::_get_parent(_n)) == _node::_red &&
                        _node::_get_color(_node::_sibling(_n)) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_left) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_right) == _node::_black) {
                    _node::_sibling(_n)->_color = _node::_red;
                    _node::_get_parent(_n)->_color = _node::_black;
                } else
                    _delcase5(_n);
            }
            
            //
            void _delcase5(_nodeptr _n) {
                if(_n == (_nodeptr) _node::_get_parent(_n)->_left &&
                        _node::_get_color(_node::_sibling(_n)) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_left) == _node::_red &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_right) == _node::_black) {
                    _node::_sibling(_n)->_color = _node::_red;
                    ((_nodeptr) _node::_sibling(_n)->_left)->_color = _node::_black;
                    _rotate_right(_node::_sibling(_n));
                } else if(_n == (_nodeptr) _node::_get_parent(_n)->_right &&
                        _node::_get_color(_node::_sibling(_n)) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_left) == _node::_black &&
                        _node::_get_color((_nodeptr) _node::_sibling(_n)->_right) == _node::_red) {
                    _node::_sibling(_n)->_color = _node::_red;
                    ((_nodeptr) _node::_sibling(_n)->_right)->_color = _node::_black;
                    _rotate_left(_node::_sibling(_n));
                }
                
                _delcase6(_n);
            }
            
            //
            void _delcase6(_nodeptr _n) {
                _node::_sibling(_n)->_color = _node::_get_parent(_n)->_color;
                _node::_get_parent(_n)->_color = _node::_black;
                if(_n == (_nodeptr) _node::_get_parent(_n)->_left) {
                    ((_nodeptr) _node::_sibling(_n)->_right)->_color = _node::_black;
                    _rotate_left(_node::_get_parent(_n));
                } else {
                    ((_nodeptr) _node::_sibling(_n)->_left)->_color = _node::_black;
                    _rotate_right(_node::_get_parent(_n));
                }
            }
            
            // deeper function that calls function to allocate node and construct
            // its contents (range by copy construction) and to pass to even deeper
            // general insertion function
            bool _insert1(_nodeptr* _ins, bool _encaps, const range_type& _val) {
                //first check if left_pt < right_pt
                if(!_comp(_val.get_left(), _val.get_right())) return false;
                *_ins = _create_node();
                if(*_ins == nullptr) return false;
                _construct_range(*_ins, _val);
                return _insert2(_ins, _root, _encaps);
            }
            
            // deeper function that calls function to allocate node and construct
            // its contents (range by any set of arguments other than copy constrution)
            // and to attempt insertion if node is acceptable (left_pt < right_pt)
            // by deeper general insertion function
            template<class ..._args>
            bool _insert1(_nodeptr* _ins, bool _encaps, _args&&... _arg) {
                *_ins = _create_node();
                if(*_ins == nullptr) return false;
                _construct_range(*_ins, std::forward<_args>(_arg)...);
                if(!_comp((*_ins)->_range.get_left(), (*_ins)->_range.get_right())) {
                    _delete_node(*_ins);
                    *_ins = nullptr;
                    return false;
                }
                return _insert2(_ins, _root, _encaps);
            }
            
            // general insertion function; the node to be inserted is passed here
            // different things happen whether the node's range to be inserted intersects
            // the ranges of any other nodes; if not, then the node is simply inserted
            // as in a basic rb-tree (like map); if so, then if the node's range fits
            // entirely within the bounds of the range of a node already in the tree,
            // then the process of insertion will be repeated to attempt to insert it
            // in the latter's subtree; if the node's range encompasses the bounds of
            // one or more ranges already in the tree, then the deepest insertion
            // method to resolve that issue is called; if the node only partially intersects
            // an existing node without its being a perfect superset of that existing node,
            // then insertion cannot occur, neither if the node's range is coterminous with
            // that of a node already in the range_map.
            bool _insert2(_nodeptr* _ins, _nodeptr* _start, bool _encaps) {
                if(*_start == nullptr) {
                    *_start = *_ins;
                } else {
                    _nodeptr _n = *_start;
                    while(1) {
                        //if left point of existing range is greater than or equal to
                        //the right point of the range to be inserted, then we need
                        //to move to the existing range's left child and continue loop
                        //(or make the range to be inserted the left child of the
                        //existing range if existing range has no left child)
                        if(!_comp(_n->_range.get_left(), (*_ins)->_range.get_right())) {
                            if(_n->_left == nullptr) {
                                _n->_left = (void*) *_ins;
                                (*_ins)->_parent = (void*) _n;
                                break;
                            }
                            _n = (_nodeptr) _n->_left;
                            continue;
                        }
                        //if left point of range to be inserted is greater than or 
                        //equal to the left point of the existing, then we need
                        //to move to the existing range's right child and continue 
                        //loop
                        //(or make the range to be inserted the right child of the
                        //existing range if existing range has no right child)
                        if(!_comp((*_ins)->_range.get_left(), _n->_range.get_right())) {
                            if(_n->_right == nullptr) {
                                _n->_right = (void*) *_ins;
                                (*_ins)->_parent = (void*) _n;
                                break;
                            }
                            _n = (_nodeptr) _n->_right;
                            continue;
                        }
                        //if we find that the range to be inserted is partly intersected
                        //by existing range, then call the deepest insertion function to find
                        //out if it is a perfect set of all the existing ranges it is partly
                        //intersected by.  If so, then do insertion and encapsulate or delete
                        //sub-ranges as specified by _encaps; if not, then set *_ins to nullptr
                        //and return notice of failure.  _insert3 will call _insert2 again to do
                        //insertion, but in this case, with all the sub-ranges of the range to be
                        //inserted removed (encapsulated or deleted), _insert3 will not be called
                        //a second time, preventing lots of recurring function calls.
                        if(_comp((*_ins)->_range.get_left(), _n->_range.get_left()) 
                                || _comp(_n->_range.get_right(), (*_ins)->_range.get_right())) {
                            return _insert3(_ins, _n, _encaps);
                        }

												//if range is coterminous with existing range, then return it along with
                        //notice of failure.
                        if(!_comp(_n->_range.get_left(), (*_ins)->_range.get_left()) 
                                && !_comp((*_ins)->_range.get_right(), _n->_range.get_right())) {
													// We allow matching sets if encapsulate is set; if not, fail
													if(!_allow_encaps_matching_sets || !_encaps || !(!_comp((*_ins)->_range.get_left(), _n->_range.get_left())
														 && !_comp(_n->_range.get_right(), (*_ins)->_range.get_right()))) {

														_delete_node(*_ins);
														*_ins = _n;
														return false;
													}
                        }
                        //with everything ruled out, we know that the range to be inserted is
                        //a sub-range of the existing range; if existing range does not
                        //already have one or more sub-ranges, then go ahead and put the 
                        //range to be inserted as the subtree's root, otherwise set the
                        //existing range to the subtree's root, and continue loop there to
                        //determine proper placement of range to be inserted.
                        if(_n->_subtree == nullptr) {
                            _n->_subtree = (void*) *_ins;
                            (*_ins)->_parent = (void*) _n;
                            break;
                        }
                        _n = (_nodeptr) _n->_subtree;
                    }
                }
                
                ++_cur_size;              //increment container size
                _inscase1(*_ins);         //keep current rb-tree balanced
                return true;
            }
            
            // for _insert3, to find the leftmost and rightmost pointers that are
            // encompassed by the range to be inserted
            // if the rules for insertion are violated (viz., that the range to 
            // be inserted is not a perfect superset of the ranges partly intersected
            // by it) then false is returned to indicate failure of insertion
            bool _getset(const range_type& _val, _nodeptr* _lft, _nodeptr* _rgt) {
                _nodeptr _temp;
                while((_temp = _node::_prev_s(*_lft)) != nullptr &&
                        _comp(_val.get_left(), _temp->_range.get_right())) {
                    *_lft = _temp;
                }
                if(_comp((*_lft)->_range.get_left(), _val.get_left())) {
                    return false;
                }
                while((_temp = _node::_next_s(*_rgt)) != nullptr &&
                        _comp(_temp->_range.get_left(), _val.get_right())) {
                    *_rgt = _temp;
                }
                if(_comp(_val.get_right(), (*_rgt)->_range.get_right())) {
                    return false;
                }
                return true;
            }
            
            // the deepest insertion function; to resolve cases where the node to
            // be inserted encompasses the other nodes; if _encaps is true, then
            // the nodes so encompassed are removed from their tree and placed in
            // _ins's subtree; if _encaps is false, then those nodes are deleted
            bool _insert3(_nodeptr* _ins, _nodeptr _n, bool _encaps) {
                _nodeptr _lft = _n, _rgt = _n;
                if(!_getset((*_ins)->_range, &_lft, &_rgt)) {
                    //(*_ins)->_range was partly intersected by one or more ranges
                    //that the former was not a perfect superset of, so insertion
                    //fails
                    _delete_node(*_ins);
                    *_ins = nullptr;
                    return false;
                }
                //take care of all nodes that represent sub-ranges; if _encaps is
                //true, then remove each from the subtree and put them in _lft's
                //subtree; otherwise, delete them all with prejudice.
                _nodeptr _endpt = _node::_next_s(_rgt);
                while(_lft != nullptr && _lft != _endpt) {
                    _nodeptr _next = _node::_next_s(_lft);
                    if(_encaps) {
                        _remove_from_subtree(_lft);
                        _lft->_parent = (void*) *_ins;
                        _insert2(&_lft, _ins, false);
                    } else 
                        _delete2(_lft, _root, false);
                    _lft = _next;
                }
                
                return _insert2(_ins, _root, false);
            }

            //subroutine to swap the position of the node pointers instead of swapping 
            //the range and subtree between them; keeps iterator to sub_range's point valid
            //when encapsulating or decapsulating (unless it's deleted later in the process)
            void _nodeptr_swap(_nodeptr _a, _nodeptr _b) {
                //swap what the parents (or root) link to first
                if(_a->_parent != nullptr) {
                    if(_a == (_nodeptr) _node::_get_parent(_a)->_left)
                      _node::_get_parent(_a)->_left = (void*) _b;
                    else if(_a == (_nodeptr) _node::_get_parent(_a)->_right)
                      _node::_get_parent(_a)->_right = (void*) _b;
                    else
                      _node::_get_parent(_a)->_subtree = (void*) _b;
                 } else *_root = _b;
                
                if(_b->_parent != nullptr) {
                    if(_b == (_nodeptr) _node::_get_parent(_b)->_left)
                      _node::_get_parent(_b)->_left = (void*) _a;
                    else if(_b == (_nodeptr) _node::_get_parent(_b)->_right)
                      _node::_get_parent(_b)->_right = (void*) _a;
                    else
                      _node::_get_parent(_b)->_subtree = (void*) _a;
                 } else *_root = _a;
                
                //then swap the node positional values themselves
                std::swap(_a->_color, _b->_color);
                std::swap(_a->_left, _b->_left);
                std::swap(_a->_right, _b->_right);
                std::swap(_a->_parent, _b->_parent);
                
                //then swap what the children's parents are
                if(_a->_left != nullptr) 
                  ((_nodeptr) _a->_left)->_parent = (void*) _a;
                if(_a->_right != nullptr) 
                  ((_nodeptr) _a->_right)->_parent = (void*) _a;
                
                if(_b->_left != nullptr) 
                  ((_nodeptr) _b->_left)->_parent = (void*) _b;
                if(_b->_right != nullptr) 
                  ((_nodeptr) _b->_right)->_parent = (void*) _b;
            }

            
            // the general deletion function; if _decaps is true, then the nodes in
            // the subtree of _n are removed, following the removal of _n, and placed
            // back into the range_map independently (decapsulated); if false, then
            // the nodes in the subtree of _n are deleted, using recursion written in
            // such a way as not to require the use of recursive function calling, not
            // to require stacking parent trees, and not to leave the nodes in the
            // subtree dangling, by setting _decaps to true in the next function call
            // (_decaps = true prevents recursive calling), deleting each node in the
            // subtree and decapsulating the nodes in its subtree; that brings back to
            // the original delete function; this continues until all the nodes in the
            // subtree (and all subsequent subtrees) of the original node to be deleted
            // are decapsulated and deleted.
            _nodeptr _delete2(_nodeptr _n, _nodeptr* _start, bool _decaps) {
               if(_n == nullptr) return nullptr;
               _nodeptr _ret = _node::_next(_n);
               if(_n->_left != nullptr && _n->_right != nullptr) {
                   //swap positions of _nodeptrs
                   _nodeptr _pred = _node::_max((_nodeptr) _n->_left);
                   _nodeptr_swap(_n, _pred);
               }
               _nodeptr _child = _n->_right == nullptr ? (_nodeptr) _n->_left : (_nodeptr) _n->_right;
               if(_node::_get_color(_n) == _node::_black) {
                   _n->_color = _node::_get_color(_child);
                   //keep rb-tree balanced
                   _delcase1(_n);
               }
               if(_node::_is_root(_n) && _child != nullptr) {
                   _child->_color = _node::_black;
               }
               
               //with this, _n is detached from the whole data structure and is
               //now orphaned.
               _node_replace(_n, _child);
               
               //keeping going till every sub-range in _n is dealt with
               while(_n->_subtree != nullptr) {
                   if(_decaps) {
                       //detaching each first-order sub-range from _n; as the superset
                       //_n is gone and the range covered by _n no longer has any nodes
                       //that falls within _n's range in the main data structure (so the
                       //sub-ranges when inserted will never be perfect supersets), this
                       //prevents _insert2 from calling _insert3, so no recurring
                       //function calls that could overload the stack.
                       _nodeptr _temp = (_nodeptr) _n->_subtree;
                       _remove_from_subtree(_temp);
                       if(*_start != nullptr && _start != _root) //keep parent null if
                                                           //starting from root!
                                _temp->_parent = (void*) *_start;
                       _insert2(&_temp, _start, false);
                   } else {
                       //prevents recurrent function calling by calling _delete2
                       //with _decaps set to true; puts the sub-ranges of each sub-range
                       //(if they exist) directly into _n's subtree; with the first-order
                       //sub_range removed, when the sub-range's sub-ranges are inserted
                       //they will not intersect any existing sub-ranges, so when _insert2
                       //is called, it will never call _insert3, preventing recurrence.
                       _delete2((_nodeptr) _n->_subtree, &_n, true);
                   }
               }
               --_cur_size;                //we now have one less node in our main data structure
               _delete_node(_n);           //destroy range_type object and deallocate _n
               return _ret;
            }
                        
            // removes node to be decapsulated or encapsulated from the subtree in
            // question, but it does not delete the node; it simply gets it ready to
            // be inserted in another subtree.
            // use of _nodeptr_swap keeps iterator to a point on _n valid.
            void _remove_from_subtree(_nodeptr _n) {
               if(_n == nullptr) return;
               //if _n has both left and right children.
               if(_n->_left != nullptr && _n->_right != nullptr) {
                   //swap positions of _nodeptrs
                   _nodeptr _pred = _node::_max((_nodeptr) _n->_left);
                   _nodeptr_swap(_n, _pred);
               }
               _nodeptr _child = _n->_right == nullptr ? (_nodeptr) _n->_left : (_nodeptr) _n->_right;
               if(_node::_get_color(_n) == _node::_black) {
                   _n->_color = _node::_get_color(_child);
                   //keep rb-tree balanced
                   _delcase1(_n);
               }
               if(_node::_is_root(_n) && _child != nullptr) {
                   _child->_color = _node::_black;
               }
               
               //with this, _n is detached from the whole data structure and is
               //now orphaned.
               _node_replace(_n, _child);
               
               --_cur_size;    //we do have one less item in the data structure;
                               //nevertheless, it will be inserted again, so the
                               //size will be incremented again.
               _reset_node(_n);  //"freshen" up orphaned node to be inserted again
                                 //in another rb-tree.
            }
            
            key_compare _comp;
            _node_allocator _alloc;
            _nodeptr* _root;
            size_type _cur_size;
    };
    
}

template<class _kty, class _ty, class _compare, class _alloc_type>
void swap(beneficii::range_map<_kty, _ty, _compare, _alloc_type>& A, beneficii::range_map<_kty, _ty, _compare, _alloc_type>& B) {
    A.swap(B);
}

#endif	/* RANGE_MAP_HPP */
