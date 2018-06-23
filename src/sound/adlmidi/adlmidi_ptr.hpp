/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLMIDI_PTR_HPP_THING
#define ADLMIDI_PTR_HPP_THING

#include <algorithm>  // swap
#include <stddef.h>
#include <stdlib.h>

/*
  Generic deleters for smart pointers
 */
template <class T>
struct ADLMIDI_DefaultDelete
{
    void operator()(T *x) { delete x; }
};
template <class T>
struct ADLMIDI_DefaultArrayDelete
{
    void operator()(T *x) { delete[] x; }
};
struct ADLMIDI_CDelete
{
    void operator()(void *x) { free(x); }
};

/*
    Safe unique pointer for C++98, non-copyable but swappable.
*/
template< class T, class Deleter = ADLMIDI_DefaultDelete<T> >
class AdlMIDI_UPtr
{
    T *m_p;
public:
    explicit AdlMIDI_UPtr(T *p)
        : m_p(p) {}
    ~AdlMIDI_UPtr()
    {
        reset();
    }

    void reset(T *p = NULL)
    {
        if(p != m_p) {
            if(m_p) {
                Deleter del;
                del(m_p);
            }
            m_p = p;
        }
    }

    void swap(AdlMIDI_UPtr &other)
    {
        std::swap(m_p, other.m_p);
    }

    T *get() const
    {
        return m_p;
    }
    T &operator*() const
    {
        return *m_p;
    }
    T *operator->() const
    {
        return m_p;
    }
    T &operator[](size_t index) const
    {
        return m_p[index];
    }
private:
    AdlMIDI_UPtr(const AdlMIDI_UPtr &);
    AdlMIDI_UPtr &operator=(const AdlMIDI_UPtr &);
};

template <class T>
void swap(AdlMIDI_UPtr<T> &a, AdlMIDI_UPtr<T> &b)
{
    a.swap(b);
}

/**
   Unique pointer for arrays.
 */
template<class T>
class AdlMIDI_UPtrArray :
    public AdlMIDI_UPtr< T, ADLMIDI_DefaultArrayDelete<T> >
{
public:
    explicit AdlMIDI_UPtrArray(T *p = NULL)
        : AdlMIDI_UPtr< T, ADLMIDI_DefaultArrayDelete<T> >(p) {}
};

/**
   Unique pointer for C memory.
 */
template<class T>
class AdlMIDI_CPtr :
    public AdlMIDI_UPtr< T, ADLMIDI_CDelete >
{
public:
    explicit AdlMIDI_CPtr(T *p = NULL)
        : AdlMIDI_UPtr< T, ADLMIDI_CDelete >(p) {}
};

/*
    Shared pointer with non-atomic counter
    FAQ: Why not std::shared_ptr? Because of Android NDK now doesn't supports it
*/
template< class T, class Deleter = ADLMIDI_DefaultDelete<T> >
class AdlMIDI_SPtr
{
    T *m_p;
    size_t *m_counter;
public:
    explicit AdlMIDI_SPtr(T *p = NULL)
        : m_p(p), m_counter(p ? new size_t(1) : NULL) {}
    ~AdlMIDI_SPtr()
    {
        reset(NULL);
    }

    AdlMIDI_SPtr(const AdlMIDI_SPtr &other)
        : m_p(other.m_p), m_counter(other.m_counter)
    {
        if(m_counter)
            ++*m_counter;
    }

    AdlMIDI_SPtr &operator=(const AdlMIDI_SPtr &other)
    {
        if(this == &other)
            return *this;
        reset();
        m_p = other.m_p;
        m_counter = other.m_counter;
        if(m_counter)
            ++*m_counter;
        return *this;
    }

    void reset(T *p = NULL)
    {
        if(p != m_p) {
            if(m_p && --*m_counter == 0) {
                Deleter del;
                del(m_p);
                if(!p) {
                    delete m_counter;
                    m_counter = NULL;
                }
            }
            m_p = p;
            if(p) {
                if(!m_counter)
                    m_counter = new size_t;
                *m_counter = 1;
            }
        }
    }

    T *get() const
    {
        return m_p;
    }
    T &operator*() const
    {
        return *m_p;
    }
    T *operator->() const
    {
        return m_p;
    }
    T &operator[](size_t index) const
    {
        return m_p[index];
    }
};

/**
   Shared pointer for arrays.
 */
template<class T>
class AdlMIDI_SPtrArray :
    public AdlMIDI_SPtr< T, ADLMIDI_DefaultArrayDelete<T> >
{
public:
    explicit AdlMIDI_SPtrArray(T *p = NULL)
        : AdlMIDI_SPtr< T, ADLMIDI_DefaultArrayDelete<T> >(p) {}
};

#endif //ADLMIDI_PTR_HPP_THING
