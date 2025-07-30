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

#ifndef DOSBOX_NO_MUTEX
#   if defined(USE_LIBOGC_MUTEX)
#       include <ogc/mutex.h>
typedef mutex_t MutexNativeObject;
#   elif defined(USE_WUT_MUTEX)
#       if __cplusplus < 201103L || (defined(_MSC_VER) && _MSC_VER < 1900)
#           define static_assert(x, y)
#       endif
#       include <coreinit/mutex.h>
typedef OSMutex MutexNativeObject;
#   elif !defined(_WIN32)
#       include <pthread.h>
typedef pthread_mutex_t MutexNativeObject;
#   else
#       include <windows.h>
typedef CRITICAL_SECTION MutexNativeObject;
#   endif
#endif


class Mutex
{
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();
private:
#if !defined(DOSBOX_NO_MUTEX)
    MutexNativeObject m;
#endif
    Mutex(const Mutex &);
    Mutex &operator=(const Mutex &);
};

class MutexHolder
{
public:
    explicit MutexHolder(Mutex &m) : m(m) { m.lock(); }
    ~MutexHolder() { m.unlock(); }
private:
    Mutex &m;
    MutexHolder(const MutexHolder &);
    MutexHolder &operator=(const MutexHolder &);
};

#if defined(DOSBOX_NO_MUTEX) // No mutex, just a dummy

inline Mutex::Mutex()
{}

inline Mutex::~Mutex()
{}

inline void Mutex::lock()
{}

inline void Mutex::unlock()
{}

#elif defined(USE_WUT_MUTEX)

inline Mutex::Mutex()
{
    OSInitMutex(&m);
}

inline Mutex::~Mutex()
{}

inline void Mutex::lock()
{
    OSLockMutex(&m);
}

inline void Mutex::unlock()
{
    OSUnlockMutex(&m);
}

#elif defined(USE_LIBOGC_MUTEX)

inline Mutex::Mutex()
{
    m = LWP_MUTEX_NULL;
    LWP_MutexInit(&m, 0);
}

inline Mutex::~Mutex()
{
    LWP_MutexDestroy(m);
}

inline void Mutex::lock()
{
    LWP_MutexLock(m);
}

inline void Mutex::unlock()
{
    LWP_MutexUnlock(m);
}

#elif !defined(_WIN32) // pthread

inline Mutex::Mutex()
{
    pthread_mutex_init(&m, NULL);
}

inline Mutex::~Mutex()
{
    pthread_mutex_destroy(&m);
}

inline void Mutex::lock()
{
    pthread_mutex_lock(&m);
}

inline void Mutex::unlock()
{
    pthread_mutex_unlock(&m);
}

#else // Win32

inline Mutex::Mutex()
{
    InitializeCriticalSection(&m);
}

inline Mutex::~Mutex()
{
    DeleteCriticalSection(&m);
}

inline void Mutex::lock()
{
    EnterCriticalSection(&m);
}

inline void Mutex::unlock()
{
    LeaveCriticalSection(&m);
}
#endif
