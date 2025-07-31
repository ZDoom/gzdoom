/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
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

#ifndef MY_PTR_HPP_THING
#define MY_PTR_HPP_THING

#include <stddef.h>
#include <stdlib.h>

/*
  Generic deleters for smart pointers
 */
template <class T>
struct My_DefaultDelete
{
    void operator()(T *x) { delete x; }
};

template <class T>
struct My_DefaultArrayDelete
{
    void operator()(T *x) { delete[] x; }
};

struct My_CDelete
{
    void operator()(void *x) { free(x); }
};

/*
    Safe unique pointer for C++98, non-copyable but swappable.
*/
template< class T, class Deleter = My_DefaultDelete<T> >
class My_UPtr
{
    T *m_p;
public:
    explicit My_UPtr(T *p = NULL)
        : m_p(p) {}
    ~My_UPtr()
    {
        reset();
    }

    void reset(T *p = NULL)
    {
        if(p != m_p)
        {
            if(m_p)
            {
                Deleter del;
                del(m_p);
            }
            m_p = p;
        }
    }

    T *get() const
    {
        return m_p;
    }

    T *release()
    {
        T *ret = m_p;
        m_p = NULL;
        return ret;
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
    My_UPtr(const My_UPtr &);
    My_UPtr &operator=(const My_UPtr &);
};

#endif // MY_PTR_HPP_THING
