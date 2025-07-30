/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPNMIDI_BANKMAP_H
#define OPNMIDI_BANKMAP_H

#include <list>
#include <utility>
#include <stdint.h>
#include <stddef.h>

#include "opnmidi_ptr.hpp"

/**
 * A simple hash map which accepts bank numbers as keys, can be reserved to a
 * fixed size, offers O(1) search and insertion, has a hash function to
 * optimize for the worst case, and has some good cache locality properties.
 */
template <class T>
class BasicBankMap
{
public:
    typedef size_t key_type;  /* the bank identifier */
    typedef T mapped_type;
    typedef std::pair<key_type, T> value_type;

    BasicBankMap();
    void reserve(size_t capacity);

    size_t size() const
        { return m_size; }
    size_t capacity() const
        { return m_capacity; }
    bool empty() const
        { return m_size == 0; }

    class iterator;
    iterator begin() const;
    iterator end() const;

    struct do_not_expand_t {};

    iterator find(key_type key);
    void erase(iterator it);
    std::pair<iterator, bool> insert(const value_type &value);
    std::pair<iterator, bool> insert(const value_type &value, do_not_expand_t);
    void clear();

    T &operator[](key_type key);

private:
    struct Slot;
    enum { minimum_allocation = 4 };
    enum
    {
        hash_bits = 8, /* worst case # of collisions: 128^2/2^hash_bits */
        hash_buckets = 1 << hash_bits
    };

public:
    class iterator
    {
    public:
        iterator();
        value_type &operator*() const { return slot->value; }
        value_type *operator->() const { return &slot->value; }
        iterator &operator++();
        bool operator==(const iterator &o) const;
        bool operator!=(const iterator &o) const;
        void to_ptrs(void *ptrs[3]);
        static iterator from_ptrs(void *const ptrs[3]);
    private:
        Slot **buckets;
        Slot *slot;
        size_t index;
        iterator(Slot **buckets, Slot *slot, size_t index);
#ifdef _MSC_VER
        template<class _T>
        friend class BasicBankMap;
#else
        friend class BasicBankMap<T>;
#endif
    };

private:
    struct Slot {
        Slot *next, *prev;
        value_type value;
        Slot() : next(NULL), prev(NULL) {}
    };
    AdlMIDI_SPtrArray<Slot *> m_buckets;
    std::list< AdlMIDI_SPtrArray<Slot> > m_allocations;
    Slot *m_freeslots;
    size_t m_size;
    size_t m_capacity;
    static size_t hash(key_type key);
    Slot *allocate_slot();
    Slot *ensure_allocate_slot();
    void free_slot(Slot *slot);
    Slot *bucket_find(size_t index, key_type key);
    void bucket_add(size_t index, Slot *slot);
    void bucket_remove(size_t index, Slot *slot);
};

#include "opnmidi_bankmap.tcc"

#endif  // OPNMIDI_BANKMAP_H
