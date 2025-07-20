// (C) Beneficii.  Released under Creative Commons license.

#ifndef BENEFICII_RANGE_MAP_RM_ITER_HPP
#define	BENEFICII_RANGE_MAP_RM_ITER_HPP

#include "rm_node.h"

#include <iterator>

namespace beneficii {

    // base class for iterator
    template<class _range_map>
    class _rm_iter_base {
        
        typedef _rm_iter_base<_range_map> _myt;
        
        protected:    
    
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef _range_node<typename _range_map::key_type,
            typename _range_map::mapped_type> _node_type;
        typedef typename _range_map::allocator_type _alloc_type;
        typedef typename _alloc_type::template rebind<_node_type>::other _node_allocator;
        typedef typename _node_allocator::pointer _nodeptr;
        typedef typename _range_map::value_type value_type;
        typedef typename _range_map::difference_type difference_type;
    
        _rm_iter_base()
        : _node(nullptr), _is_rgt_pt(false) {}
        
        _rm_iter_base(_nodeptr _node, bool _is_rgt_pt)
        : _node(_node), _is_rgt_pt(_is_rgt_pt) {}

        _rm_iter_base(const _myt& _right) 
        : _node(_right._node), _is_rgt_pt(_right._is_rgt_pt) {}
        
        void _do_assign(const _myt* _right) {
            _node = _right->_node;
            _is_rgt_pt = _right->_is_rgt_pt;
        }
        
        bool _do_equal(const _myt* _right) {
            return _node == _right->_node && _is_rgt_pt == _right->_is_rgt_pt;
        }
        
        _nodeptr _node;
        bool _is_rgt_pt;
    };
    
    template<class _range_map>
    class _rm_const_iterator;
    
template<class _range_map>
class _rm_iterator : public _rm_iter_base<_range_map> {
    
    typedef _rm_iter_base<_range_map> _base;
    typedef _rm_iterator<_range_map> _myt;
    typedef _rm_const_iterator<_range_map> _ott;
    typedef typename _base::_node_type _node_type;
    typedef typename _base::_nodeptr _nodeptr;
    
public:
    
    typedef typename _base::iterator_category iterator_category;
    typedef typename _base::value_type value_type;
    typedef typename _base::difference_type difference_type;
    typedef value_type* pointer;
    typedef value_type& reference;
    
    _rm_iterator() : _rm_iter_base<_range_map>() {}
    _rm_iterator(_nodeptr _node, bool _is_rgt_pt)
    : _rm_iter_base<_range_map>(_node, _is_rgt_pt) {}
    
    _rm_iterator(const _myt& _right) 
    : _rm_iter_base<_range_map>(_right) {}
    _myt operator=(const _myt& _right) {
        if(this != &_right) {
            this->_do_assign(&_right);
        }
        return *this;
    }
    
    bool operator==(const _myt& _right) {
        return this->_do_equal(&_right);
    }
    bool operator!=(const _myt& _right) {
        return !operator==(_right);
    }
    
     bool operator==(const _ott& _right) {
        return this->_do_equal(&_right);
    }
    bool operator!=(const _ott& _right) {
        return !operator==(_right);
    }
    
    reference operator*() {
        return this->_is_rgt_pt ? this->_node->_end : this->_node->_start;
    }
    pointer operator->() {
        return this->_is_rgt_pt ? &this->_node->_end : &this->_node->_start;
    }
    
    _myt operator++() {
        this->_is_rgt_pt = _node_type::_next(&this->_node, this->_is_rgt_pt);
        return *this;
    }
    _myt operator++(int) {
        _myt _ret = *this;
        operator++();
        return _ret;
    }
    
    _myt operator--() {
        this->_is_rgt_pt = _node_type::_prev(&this->_node, this->_is_rgt_pt);
        return *this;
    }
    _myt operator--(int) {
        _myt _ret = *this;
        operator--();
        return _ret;
    }
};

template<class _range_map>
class _rm_const_iterator : public _rm_iter_base<_range_map> {
    
    typedef _rm_iter_base<_range_map> _base;
    typedef _rm_const_iterator<_range_map> _myt;
    typedef _rm_iterator<_range_map> _ott;
    
    typedef typename _base::_node_type _node_type;
    typedef typename _base::_nodeptr _nodeptr;
    
public:
    
    typedef typename _base::iterator_category iterator_category;
    typedef typename _base::value_type value_type;
    typedef typename _base::difference_type difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;
    
    _rm_const_iterator() : _rm_iter_base<_range_map>() {}
    _rm_const_iterator(_nodeptr _node, bool _is_rgt_pt)
    : _rm_iter_base<_range_map>(_node, _is_rgt_pt) {}
    
    _rm_const_iterator(const _myt& _right) 
    : _rm_iter_base<_range_map>(_right) {}
    _myt operator=(const _myt& _right) {
        if(this != &_right) {
            this->_do_assign(&_right);
        }
        return *this;
    }
    
    _rm_const_iterator(const _ott& _right) 
    : _rm_iter_base<_range_map>(_right) {}
    _myt operator=(const _ott& _right) {
        this->_do_assign(&_right);
        return *this;
    }
    
    bool operator==(const _myt& _right) {
        return this->_do_equal(&_right);
    }
    bool operator!=(const _myt& _right) {
        return !operator==(_right);
    }
    
     bool operator==(const _ott& _right) {
        return this->_do_equal(&_right);
    }
    bool operator!=(const _ott& _right) {
        return !operator==(_right);
    }
    
    reference operator*() {
        return this->_is_rgt_pt ? this->_node->_end : this->_node->_start;
    }
    pointer operator->() {
        return this->_is_rgt_pt ? &this->_node->_end : &this->_node->_start;
    }
    
    _myt operator++() {
        this->_is_rgt_pt = _node_type::_next(&this->_node, this->_is_rgt_pt);
        return *this;
    }
    _myt operator++(int) {
        _myt _ret = *this;
        operator++();
        return _ret;
    }
    
    _myt operator--() {
        this->_is_rgt_pt = _node_type::_prev(&this->_node, this->_is_rgt_pt);
        return *this;
    }
    _myt operator--(int) {
        _myt _ret = *this;
        operator--();
        return _ret;
    }
};

}

#endif	/* RM_ITER_HPP */
