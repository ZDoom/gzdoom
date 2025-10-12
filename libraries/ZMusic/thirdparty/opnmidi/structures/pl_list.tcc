//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "pl_list.hpp"

template <class Cell>
pl_iterator<Cell>::pl_iterator(Cell *cell)
    : cell_(cell)
{
}

template <class Cell>
bool pl_iterator<Cell>::is_end() const
{
    return cell_->next == NULL;
}

template <class Cell>
Cell &pl_iterator<Cell>::operator*() const
{
    return *cell_;
}

template <class Cell>
Cell *pl_iterator<Cell>::operator->() const
{
    return cell_;
}

template <class T>
bool pl_iterator<T>::operator==(const pl_iterator &i) const
{
    return cell_ == i.cell_;
}

template <class T>
bool pl_iterator<T>::operator!=(const pl_iterator &i) const
{
    return cell_ != i.cell_;
}

template <class T>
pl_iterator<T> &pl_iterator<T>::operator++()
{
    cell_ = cell_->next;
    return *this;
}

template <class T>
pl_iterator<T> pl_iterator<T>::operator++(int)
{
    pl_iterator i(cell_);
    cell_ = cell_->next;
    return i;
}

template <class T>
pl_iterator<T> &pl_iterator<T>::operator--()
{
    cell_ = cell_->prev;
    return *this;
}

template <class T>
pl_iterator<T> pl_iterator<T>::operator--(int)
{
    pl_iterator i(cell_);
    cell_ = cell_->prev;
    return i;
}

template <class T>
pl_list<T>::pl_list(std::size_t capacity)
{
    initialize(capacity);
}

template <class T>
pl_list<T>::~pl_list()
{
    if (cells_allocd_)
        delete[] cells_;
}

template <class T>
pl_list<T>::pl_list(pl_cell<T> *cells, std::size_t ncells, external_storage_policy)
{
    initialize(ncells, cells);
}

template <class T>
pl_list<T>::pl_list(const pl_list &other)
{
    initialize(other.capacity());
    for(const_iterator i = other.end(), b = other.begin(); i-- != b;)
        push_front(i->value);
}

template <class T>
pl_list<T> &pl_list<T>::operator=(const pl_list &other)
{
    if(this != &other)
    {
        std::size_t size = other.size();
        if(size > capacity())
        {
            pl_cell<T> *oldcells = cells_;
            bool allocd = cells_allocd_;
            initialize(other.capacity());
            if (allocd)
                delete[] oldcells;
        }
        clear();
        for(const_iterator i = other.end(), b = other.begin(); i-- != b;)
            push_front(i->value);
    }
    return *this;
}

template <class T>
std::size_t pl_list<T>::size() const
{
    return size_;
}

template <class T>
std::size_t pl_list<T>::capacity() const
{
    return capacity_;
}

template <class T>
bool pl_list<T>::empty() const
{
    return size_ == 0;
}

template <class T>
typename pl_list<T>::iterator pl_list<T>::begin()
{
    return iterator(first_);
}

template <class T>
typename pl_list<T>::iterator pl_list<T>::end()
{
    return iterator(reinterpret_cast<pl_cell<T> *>(&endcell_));
}

template <class T>
typename pl_list<T>::const_iterator pl_list<T>::begin() const
{
    return const_iterator(first_);
}

template <class T>
typename pl_list<T>::const_iterator pl_list<T>::end() const
{
    return const_iterator(reinterpret_cast<const pl_cell<T> *>(&endcell_));
}

template <class T>
void pl_list<T>::clear()
{
    std::size_t capacity = capacity_;
    pl_cell<T> *cells = cells_;
    pl_cell<T> *endcell = &*end();
    size_ = 0;
    first_ = endcell;
    free_ = cells;
    endcell->prev = NULL;
    for(std::size_t i = 0; i < capacity; ++i)
    {
        cells[i].prev = (i > 0) ? &cells[i - 1] : NULL;
        cells[i].next = (i + 1 < capacity) ? &cells[i + 1] : NULL;
        cells[i].value = T();
    }
}

template <class T>
pl_cell<T> &pl_list<T>::front()
{
    return *first_;
}

template <class T>
const pl_cell<T> &pl_list<T>::front() const
{
    return *first_;
}

template <class T>
pl_cell<T> &pl_list<T>::back()
{
    iterator i = end();
    return *--i;
}

template <class T>
const pl_cell<T> &pl_list<T>::back() const
{
    const_iterator i = end();
    return *--i;
}

template <class T>
typename pl_list<T>::iterator pl_list<T>::insert(iterator pos, const T &x)
{
    pl_cell<T> *cell = allocate(&*pos);
    if (!cell)
        throw std::bad_alloc();
    cell->value = x;
    return iterator(cell);
}

template <class T>
typename pl_list<T>::iterator pl_list<T>::erase(iterator pos)
{
    deallocate(&*(pos++));
    return pos;
}

template <class T>
void pl_list<T>::push_front(const T &x)
{
    insert(begin(), x);
}

template <class T>
void pl_list<T>::push_back(const T &x)
{
    insert(end(), x);
}

template <class T>
void pl_list<T>::pop_front()
{
    deallocate(first_);
}

template <class T>
void pl_list<T>::pop_back()
{
    iterator i(&*end());
    deallocate(&*--i);
}

template <class T>
typename pl_list<T>::iterator pl_list<T>::find(const T &x)
{
    const_iterator i = const_cast<const pl_list<T> *>(this)->find(x);
    return iterator(&const_cast<reference>(*i));
}

template <class T>
typename pl_list<T>::const_iterator pl_list<T>::find(const T &x) const
{
    const_iterator e = end();
    for (const_iterator i = begin(); i != e; ++i)
    {
        if(i->value == x)
            return i;
    }
    return e;
}

template <class T>
template <class Pred>
typename pl_list<T>::iterator pl_list<T>::find_if(const Pred &p)
{
    const_iterator i = const_cast<const pl_list<T> *>(this)->find_if(p);
    return iterator(&const_cast<reference>(*i));
}

template <class T>
template <class Pred>
typename pl_list<T>::const_iterator pl_list<T>::find_if(const Pred &p) const
{
    const_iterator e = end();
    for (const_iterator i = begin(); i != e; ++i)
    {
        if(p(i->value))
            return i;
    }
    return e;
}

template <class T>
void pl_list<T>::initialize(std::size_t capacity, pl_cell<T> *extcells)
{
    cells_ = extcells ? extcells : new pl_cell<T>[capacity];
    cells_allocd_ = extcells ? false : true;
    capacity_ = capacity;
    endcell_.next = NULL;
    clear();
}

template <class T>
pl_cell<T> *pl_list<T>::allocate(pl_cell<T> *pos)
{
    // remove free cells front
    pl_cell<T> *cell = free_;
    if(!cell)
        return NULL;
    free_ = cell->next;
    if(free_)
        free_->prev = NULL;

    // insert at position
    if (pos == first_)
        first_ = cell;
    cell->prev = pos->prev;
    if (cell->prev)
        cell->prev->next = cell;
    cell->next = pos;
    pos->prev = cell;

    ++size_;
    return cell;
}

template <class T>
void pl_list<T>::deallocate(pl_cell<T> *cell)
{
    if(cell->prev)
        cell->prev->next = cell->next;
    if(cell->next)
        cell->next->prev = cell->prev;
    if(cell == first_)
        first_ = cell->next;
    cell->prev = NULL;
    cell->next = free_;
    cell->value = T();
    free_ = cell;
    --size_;
}
