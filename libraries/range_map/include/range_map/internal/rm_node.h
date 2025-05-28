// (C) Beneficii.  Released under Creative Commons license.

#ifndef BENEFICII_RANGE_MAP_RM_NODE_HPP
#define	BENEFICII_RANGE_MAP_RM_NODE_HPP

#include "rm_base.h"

namespace beneficii {

// the nodes the are allocated in memory by the range_map; the range_map_item used
// is constructed in this node; start and end points are kept track of here
// nothing outside the range_map library needs to call on _range_node, except on the
// individual points (which are of value_type) through the range_map iterators
// void* are used for the connecting nodes and templates for static functions,
// because the allocator may use a wrapper class for the node pointer.

template<class _key, class _mpd>
struct _range_node {
    typedef _range_node<_key, _mpd> _myt;
    
    struct _point {
        
        _point(void* _node) : _node((_myt*) _node) {}
      
        //returns whether current point is right end point
        bool is_rgt_pt() const {
            return this == &_node->_end;
        }
        
        //returns point pointed to
        const _key& cur_pt() const {
            return !is_rgt_pt() ? _node->_range.get_left() : _node->_range.get_right();
        }
        
        //returns point other than that pointed too
        const _key& opp_pt() const {
            return is_rgt_pt() ? _node->_range.get_left() : _node->_range.get_right();
        }
        
        //returns start point
        const _key& start_pt() const {
            return _node->_range.get_left();
        }
        
        //returns end point
        const _key& end_pt() const {
            return _node->_range.get_right();
        }
        
        _key width() const {
            return _node->_range.width();
        }
        
        //returns reference to mapped value
        _mpd& mapped() {
            return _node->_range.mapped();
        }
        
        //returns const_reference to mapped value
        const _mpd& mapped() const {
            return _node->_range.mapped();
        }
        
        //returns const_reference to point on opposite end
        const _point& opposite_pt() const {
            return is_rgt_pt() ? _node->_start : _node->_end;
        }
        
        //returns const range_map_item
        const range_map_item<_key, _mpd>& range() const {
            return _node->_range;
        }
        
        private:
            _myt* _node;
    };
    
    range_map_item<_key, _mpd> _range;
    void* _parent;
    void* _left;
    void* _subtree;
    void* _right;
    int _color;
    _point _start;
    _point _end;
    
    template<class _np>
    static _np _get_parent(_np _n) {
      return (_np) _n->_parent;
    }
    
    template<class _np>
    static _np _grandparent(_np _n) {
        _n = _get_parent(_n);
        if(_n == nullptr) return nullptr;
        if(_is_root(_n)) return nullptr;
        return _get_parent(_n);
    }
    
    template<class _np>
    static _np _sibling(_np _n) {
        if(_is_root(_n)) return nullptr;
        _np _m = _get_parent(_n);
        if(_m == nullptr) return nullptr;
        return _n == (_np) _m->_left ? (_np) _m->_right : (_np) _m->_left;
    }
    
    template<class _np>
    static _np _aunt(_np _n) {
        _n = _get_parent(_n);
        if(_n == nullptr) return nullptr;
        if(_is_root(_n)) return nullptr;
        return _sibling(_n);
    }
    
    template<class _np>
    static int _get_color(_np _n) {
        return _n == nullptr ? _black : _n->_color;
    }
    
    template<class _np>
    static bool _is_root(_np _n) {
        return _n->_parent == nullptr || _n == (_np) ((_np) _n->_parent)->_subtree;
    }
    
    template<class _np>
    static _np _min(_np _n) {
        while(_n->_left != nullptr)
            _n = (_np) _n->_left;
        return _n;
    }
    
    template<class _np>
    static _np _max(_np _n) {
        while(_n->_right != nullptr)
            _n = (_np) _n->_right;
        return _n;
    }
    
    template<class _np>
    static _np _next(_np _n) {
        if(_n->_right == nullptr) {
            while(_n->_parent != nullptr && _n == ((_np) _n->_parent)->_right)
                _n = (_np) _n->_parent;
            _n = (_np) _n->_parent;
        } else {
            _n = _min((_np) _n->_right);
        }
        return _n;
    }
    
    template<class _np>
    static _np _prev(_np _n) {
        if(_n->_left == nullptr) {
            while(_n->_parent != nullptr && _n == (_np) ((_np) _n->_parent)->_left)
                _n = _get_parent(_n);
            _n = _get_parent(_n);
        } else {
            _n = _max((_np) _n->_left);
        }
        return _n;
    }
    
    template<class _np>
    static _np _next_s(_np _n) {
        if(_n->_right == nullptr) {
            while(_n->_parent != nullptr && _n == (_np) ((_np) _n->_parent)->_right)
                _n = (_np) _n->_parent;
            if(_is_root(_n))  
                _n = nullptr;
            else
                _n = _get_parent(_n);
        } else {
            _n = _min((_np) _n->_right);
        }
        return _n;
    }
    
    template<class _np>
    static _np _prev_s(_np _n) {
        if(_n->_left == nullptr) {
            while(_n->_parent != nullptr && _n == (_np) ((_np) _n->_parent)->_left)
                _n = (_np) _n->_parent;
            if(_is_root(_n)) 
                _n = nullptr;
            else
                _n = _get_parent(_n);
        } else {
            _n = _max((_np) _n->_left);
        }
        return _n;
    }
    
    //moving functions for iterators
    template<class _np>
    static bool _next(_np* _n, bool _is_rgt_pt) {
        bool _ret = _is_rgt_pt;
        if(_is_rgt_pt) {
            if((*_n)->_right == nullptr) {
                while((*_n)->_parent != nullptr && *_n == (_np) ((_np) (*_n)->_parent)->_right)
                    *_n = _get_parent(*_n);
                if((*_n)->_parent != nullptr) {
                    if(*_n == (_np) ((_np) (*_n)->_parent)->_subtree) 
                        _ret = true;
                    else
                        _ret = false;
                    *_n = _get_parent(*_n);
                } else {
                    *_n = nullptr;
                    _ret = false;
                }
            } else {
                *_n = _min((_np) (*_n)->_right);
                _ret = false;
            }
        } else {
            if((*_n)->_subtree == nullptr) {
                _ret = true;
            } else {
                *_n = _min((_np) (*_n)->_subtree);
                _ret = false;
            }
        }
        return _ret;
    }
    
    template<class _np>
    static bool _prev(_np* _n, bool _is_rgt_pt) {
        bool _ret = _is_rgt_pt;
        if(_is_rgt_pt) {
            if((*_n)->_subtree == nullptr) {
                _ret = false;
            } else {
                *_n = _max((_np) (*_n)->_subtree);
                _ret = true;
            }
        } else {
            if((*_n)->_left == nullptr) {
                while((*_n)->_parent != nullptr && *_n == (_np) ((_np) (*_n)->_parent)->_left)
                    *_n = _get_parent(*_n);
                if((*_n)->_parent != nullptr) {
                    if(*_n == (_np) ((_np) (*_n)->_parent)->_subtree) 
                        _ret = false;
                    else
                        _ret = true;
                    *_n = _get_parent(*_n);
                } else {
                    *_n = nullptr;
                    _ret = false;
                }
            } else {
                *_n = _max((_np) (*_n)->_left);
                _ret = true;
            }
        }
        return _ret;
    }
    
    static const int _black = 0;
    static const int _red = 1;
};

/*#if __cplusplus >= 201103L
template<class _key, class _mpd>
using range_map_point = _range_node<_key, _mpd>::_point;
#endif*/

}

#endif	/* RM_NODE_HPP */

