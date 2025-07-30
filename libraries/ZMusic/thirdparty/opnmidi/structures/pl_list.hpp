//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef PL_LIST_HPP
#define PL_LIST_HPP

#include <iterator>
#include <cstddef>

/*
  pl_cell: the linked list cell
 */
template <class T>
struct pl_cell;

template <class T>
struct pl_basic_cell
{
    pl_cell<T> *prev, *next;
};

template <class T>
struct pl_cell : pl_basic_cell<T>
{
    T value;
};

/*
  pl_iterator: the linked list iterator
 */
template <class Cell>
class pl_iterator
{
public:
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef Cell value_type;
    typedef Cell &reference;
    typedef Cell *pointer;
    typedef std::ptrdiff_t difference_type;

    pl_iterator(Cell *cell = NULL);
    bool is_end() const;
    Cell &operator*() const;
    Cell *operator->() const;
    bool operator==(const pl_iterator &i) const;
    bool operator!=(const pl_iterator &i) const;
    pl_iterator &operator++();
    pl_iterator operator++(int);
    pl_iterator &operator--();
    pl_iterator operator--(int);

private:
    Cell *cell_;
};

/*
  pl_list: the preallocated linked list
 */
template <class T>
class pl_list
{
public:
    typedef pl_cell<T> value_type;
    typedef value_type *pointer;
    typedef value_type &reference;
    typedef const value_type *const_pointer;
    typedef const value_type &const_reference;
    typedef pl_iterator< pl_cell<T> > iterator;
    typedef pl_iterator< const pl_cell<T> > const_iterator;

    pl_list(std::size_t capacity = 0);
    ~pl_list();

    struct external_storage_policy {};
    pl_list(pl_cell<T> *cells, std::size_t ncells, external_storage_policy);

    pl_list(const pl_list &other);
    pl_list &operator=(const pl_list &other);

    std::size_t size() const;
    std::size_t capacity() const;
    bool empty() const;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

    void clear();

    pl_cell<T> &front();
    const pl_cell<T> &front() const;
    pl_cell<T> &back();
    const pl_cell<T> &back() const;

    iterator insert(iterator pos, const T &x);
    iterator erase(iterator pos);
    void push_front(const T &x);
    void push_back(const T &x);
    void pop_front();
    void pop_back();

    iterator find(const T &x);
    const_iterator find(const T &x) const;
    template <class Pred> iterator find_if(const Pred &p);
    template <class Pred> const_iterator find_if(const Pred &p) const;

private:
    // number of cells in the list
    std::size_t size_;
    // number of cells allocated
    std::size_t capacity_;
    // array of cells allocated
    pl_cell<T> *cells_;
    // pointer to the head cell
    pl_cell<T> *first_;
    // pointer to the next free cell
    pl_cell<T> *free_;
    // value-less cell which terminates the linked list
    pl_basic_cell<T> endcell_;
    // whether cell storage is allocated
    bool cells_allocd_;

    void initialize(std::size_t capacity, pl_cell<T> *extcells = NULL);
    pl_cell<T> *allocate(pl_cell<T> *pos);
    void deallocate(pl_cell<T> *cell);
};

#include "pl_list.tcc"

#endif // PL_LIST_HPP
