#ifndef _GLIBSTUBS_H
#define _GLIBSTUBS_H

#ifdef WIN32
#include <Windows.h>
#include <assert.h>
#include <stdio.h>

/* Miscellaneous stubs */
#define GLIB_CHECK_VERSION(x, y, z) 0 /* Evaluate to 0 to get FluidSynth to use the "old" thread API */
#define GLIB_MAJOR_VERSION 2
#define GLIB_MINOR_VERSION 29

typedef struct
{
    int code;
    const char *message;
} GError;
typedef void *gpointer;

#define g_new(s, c) FLUID_ARRAY(s, c)
#define g_free(p) FLUID_FREE(p)
#define g_strfreev FLUID_FREE
#define g_newa(_type, _len) (_type *)_alloca(sizeof(_type) * (_len))
#define g_assert(a) assert(a)
#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)
#endif

#define g_vsnprintf(b, c, f, a) vsnprintf(b, c, f, a)
#define g_snprintf(b, c, f, ...) snprintf(b, c, f, __VA_ARGS__)

#define g_return_val_if_fail(expr, val) if (expr) {} else { return val; }
#define g_clear_error(err) do {} while (0)

#define G_FILE_TEST_EXISTS 1
#define G_FILE_TEST_IS_REGULAR 2

#define g_file_test fluid_g_file_test
#define g_shell_parse_argv fluid_g_shell_parse_argv
BOOL fluid_g_file_test(const char *path, int flags);
BOOL fluid_g_shell_parse_argv(const char *command_line, int *argcp, char ***argvp, void *dummy);

#define g_get_monotonic_time fluid_g_get_monotonic_time
double fluid_g_get_monotonic_time(void);

/* Byte ordering */
#ifdef __BYTE_ORDER__
#define G_BYTE_ORDER __BYTE_ORDER__
#define G_BIG_ENDIAN __ORDER_BIG_ENDIAN__
#else
// If __BYTE_ORDER__ isn't defined, assume little endian
#define G_BYTE_ORDER 1234
#define G_BIG_ENDIAN 4321
#endif

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define GINT16_FROM_LE(x) (int16_t)(((uint16_t)(x) >> 8) | ((uint16_t)(x) << 8))
#define GINT32_FROM_LE(x) (int32_t)((FLUID_LE16TOH(x) << 16) | (FLUID16_LE16TOH(x >> 16)))
#else
#define GINT32_FROM_LE(x) (x)
#define GINT16_FROM_LE(x) (x)

/* Thread support */
#define g_thread_supported() 1
#define g_thread_init(_) do {} while (0)
#define g_usleep(usecs) Sleep((usecs) / 1000)

typedef gpointer (*GThreadFunc)(void *data);
typedef struct
{
    GThreadFunc func;
    void *data;
    HANDLE handle;
} GThread;

#define g_thread_create fluid_g_thread_create
#define g_thread_join fluid_g_thread_join
GThread *fluid_g_thread_create(GThreadFunc func, void *data, BOOL joinable, GError **error);
void fluid_g_thread_join(GThread *thread);

/* Regular mutex */
typedef SRWLOCK GStaticMutex;
#define G_STATIC_MUTEX_INIT SRWLOCK_INIT
#define g_static_mutex_init(_m) InitializeSRWLock(_m)
#define g_static_mutex_free(_m) do {} while (0)
#define g_static_mutex_lock(_m) AcquireSRWLockExclusive(_m)
#define g_static_mutex_unlock(_m) ReleaseSRWLockExclusive(_m)

/* Recursive lock capable mutex */
typedef CRITICAL_SECTION GStaticRecMutex;
#define g_static_rec_mutex_init(_m) InitializeCriticalSection(_m)
#define g_static_rec_mutex_free(_m) DeleteCriticalSection(_m)
#define g_static_rec_mutex_lock(_m) EnterCriticalSection(_m)
#define g_static_rec_mutex_unlock(_m) LeaveCriticalSection(_m)

/* Dynamically allocated mutex suitable for fluid_cond_t use */
typedef SRWLOCK GMutex;
#define g_mutex_free(m) do { if (m != NULL) g_free(m); } while(0)
#define g_mutex_lock(m) AcquireSRWLockExclusive(m)
#define g_mutex_unlock(m) ReleaseSRWLockExclusive(m)

static inline GMutex *g_mutex_new(void)
{
    GMutex *mutex = g_new(GMutex, 1);
    InitializeSRWLock(mutex);
    return mutex;
}

/* Thread condition signaling */
typedef CONDITION_VARIABLE GCond;
#define g_cond_free(cond) do { if (cond != NULL) g_free(cond); } while (0)
#define g_cond_signal(cond) WakeConditionVariable(cond)
#define g_cond_broadcast(cond) WakeAllConditionVariable(cond)
#define g_cond_wait(cond, mutex) SleepConditionVariableSRW(cond, mutex, INFINITE, 0)

static inline GCond *g_cond_new(void)
{
    GCond *cond = g_new(GCond, 1);
    InitializeConditionVariable(cond);
    return cond;
}

/* Thread private data */
typedef DWORD GStaticPrivate;
#define g_static_private_init(_priv) do { *_priv = TlsAlloc(); } while (0)
#define g_static_private_get(_priv) TlsGetValue(*_priv)
#define g_static_private_set(_priv, _data, _) TlsSetValue(*_priv, _data)
#define g_static_private_free(_priv) TlsFree(*_priv)

/* Atomic operations */
#define g_atomic_int_inc(_pi) InterlockedIncrement(_pi)
#define g_atomic_int_get(_pi) (MemoryBarrier(), *_pi)
#define g_atomic_int_set(_pi, _val) do { MemoryBarrier(); *_pi = _val; } while (0)
#define g_atomic_int_dec_and_test(_pi) (InterlockedDecrement(_pi) == 0)
#define g_atomic_int_compare_and_exchange(_pi, _old, _new) (InterlockedCompareExchange(_pi, _new, _old) == _old)
#define g_atomic_int_exchange_and_add(_pi, _add) InterlockedExchangeAdd(_pi, _add)

#endif

#endif
