/*
C89 compatible threads. Choice of public domain or MIT-0. See license statements at the end of this file.

David Reid - mackron@gmail.com
*/

/*
NOTE: Condition variables are not current supported on Win32 builds. If this is needed, you'll need
      to use pthread.

Introduction
============
This library aims to implement an equivalent to the C11 threading library. Not everything is implemented:

  * Condition variables are not supported on the Win32 build. If your compiler supports pthread, you
    can use that instead by putting `#define C89THREAD_USE_PTHREAD` before including this file.
  * Thread-specific storage (TSS/TLS) is not yet implemented.

The API should be compatible with the main C11 API, but all apis have been namespaced with `c89`:

    +----------+----------------+
    | C11 Type | c89thread Type |
    +----------+----------------+
    | thrd_t   | c89thrd_t      |
    | mtx_t    | c89mtx_t       |
    | cnd_t    | c89cnd_t       |
    +----------+----------------+

In addition to types defined by the C11 standard, c89thread also implements the following primitives:

    * Semaphores (`c89sem_t`)
    * Events (`c89evnt_t`)

The C11 threading library uses the timespec function for specifying times, however this is not well
supported on older compilers. Therefore, c89thread implements some helper functions for working with
the timespec object. For known compilers that do not support the timespec struct, c89thread will
define it.

Sometimes c89thread will need to allocate memory internally. You can set a custom allocator at the
global level with `c89thread_set_allocation_callbacks()`. This is not thread safe, but can be called
from any thread so long as you do your own synchronization.

This is still work-in-progress and not much testing has been done. Use at your own risk.
*/

#ifndef c89thread_h
#define c89thread_h

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
    typedef long long c89thread_intptr;
#else
    typedef int c89thread_intptr;
#endif
typedef void* c89thread_handle;

#if defined(_WIN32) && !defined(C89THREAD_USE_PTHREAD)
    /* Win32. Do *not* include windows.h here. It will be included in the implementation section. */
    #define C89THREAD_WIN32
#else
    /* Using pthread */
    #define C89THREAD_POSIX
#endif

#if defined(C89THREAD_POSIX)
    #ifndef C89THREAD_USE_PTHREAD
    #define C89THREAD_USE_PTHREAD
    #endif
    #include <pthread.h>
#endif

#include <time.h>   /* For timespec. */

#ifndef TIME_UTC
#define TIME_UTC    1
#endif

#if (defined(_MSC_VER) && _MSC_VER < 1900) || defined(__DMC__)  /* 1900 = Visual Studio 2015 */
struct timespec
{
    time_t tv_sec;
    long tv_nsec;
};
#endif

enum
{
    c89thrd_success  =  0,
    c89thrd_signal   = -1,  /* Not one of the standard results specified by C11, but -1 is used to indicate a signal in some APIs (thrd_sleep(), for example). */
    c89thrd_nomem    = -2,
    c89thrd_timedout = -3,
    c89thrd_busy     = -4,
    c89thrd_error    = -5
};


/* Memory Management. */
typedef enum
{
    c89thread_allocation_type_general = 0
} c89thread_allocation_type;

typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, c89thread_allocation_type type, void* pUserData);
    void  (* onFree)(void* p, c89thread_allocation_type type, void* pUserData);
} c89thread_allocation_callbacks;

void c89thread_set_allocation_callbacks(const c89thread_allocation_callbacks* pCallbacks);
void* c89thread_malloc(size_t sz, c89thread_allocation_type type, const c89thread_allocation_callbacks* pCallbacks);
void  c89thread_free(void* p, c89thread_allocation_type type, const c89thread_allocation_callbacks* pCallbacks);


/* thrd_t */
#if defined(C89THREAD_WIN32)
typedef c89thread_handle    c89thrd_t;  /* HANDLE, CreateThread() */
#else
typedef pthread_t           c89thrd_t;
#endif

typedef int (* c89thrd_start_t)(void*);

int c89thrd_create(c89thrd_t* thr, c89thrd_start_t func, void* arg);
int c89thrd_equal(c89thrd_t lhs, c89thrd_t rhs);
c89thrd_t c89thrd_current(void);
int c89thrd_sleep(const struct timespec* duration, struct timespec* remaining);
void c89thrd_yield(void);
void c89thrd_exit(int res);
int c89thrd_detach(c89thrd_t thr);
int c89thrd_join(c89thrd_t thr, int* res);


/* mtx_t */
#if defined(C89THREAD_WIN32)
typedef struct
{
    c89thread_handle handle;    /* HANDLE, CreateMutex(), CreateEvent() */
    int type;
} c89mtx_t;
#else
typedef pthread_mutex_t c89mtx_t;
#endif

enum
{
    c89mtx_plain     = 0x00000000,
    c89mtx_timed     = 0x00000001,
    c89mtx_recursive = 0x00000002
};

int c89mtx_init(c89mtx_t* mutex, int type);
void c89mtx_destroy(c89mtx_t* mutex);
int c89mtx_lock(c89mtx_t* mutex);
int c89mtx_timedlock(c89mtx_t* mutex, const struct timespec* time_point);
int c89mtx_trylock(c89mtx_t* mutex);
int c89mtx_unlock(c89mtx_t* mutex);


/* cnd_t */
#if defined(C89THREAD_WIN32)
/* Not implemented. */
typedef void*           c89cnd_t;
#else
typedef pthread_cond_t  c89cnd_t;
#endif

int c89cnd_init(c89cnd_t* cnd);
void c89cnd_destroy(c89cnd_t* cnd);
int c89cnd_signal(c89cnd_t* cnd);
int c89cnd_broadcast(c89cnd_t* cnd);
int c89cnd_wait(c89cnd_t* cnd, c89mtx_t* mtx);
int c89cnd_timedwait(c89cnd_t* cnd, c89mtx_t* mtx, const struct timespec* time_point);


/* c89sem_t (not part of C11) */
#if defined(C89THREAD_WIN32)
typedef c89thread_handle c89sem_t;
#else
typedef struct
{
    int value;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} c89sem_t;
#endif

int c89sem_init(c89sem_t* sem, int value);
void c89sem_destroy(c89sem_t* sem);
int c89sem_wait(c89sem_t* sem);
int c89sem_timedwait(c89sem_t* sem, const struct timespec* time_point);
int c89sem_post(c89sem_t* sem);


/* c89evnt_t (not part of C11) */
#if defined(C89THREAD_WIN32)
typedef c89thread_handle c89evnt_t;
#else
typedef struct
{
    int value;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} c89evnt_t;
#endif

int c89evnt_init(c89evnt_t* evnt);
void c89evnt_destroy(c89evnt_t* evnt);
int c89evnt_wait(c89evnt_t* evnt);
int c89evnt_timedwait(c89evnt_t* evnt, const struct timespec* time_point);
int c89evnt_signal(c89evnt_t* evnt);


/* Timing Helpers */
void c89timespec_get(struct timespec* ts, int base);
struct timespec c89timespec_now();
struct timespec c89timespec_milliseconds(time_t milliseconds);
struct timespec c89timespec_seconds(time_t seconds);
struct timespec c89timespec_diff(struct timespec lhs, struct timespec rhs);
struct timespec c89timespec_add(struct timespec tsA, struct timespec tsB);

/* Thread Helpers. */
int c89thrd_sleep_timespec(struct timespec ts);
int c89thrd_sleep_milliseconds(int milliseconds);


#if defined(__cplusplus)
}
#endif
#endif  /* c89thread_h */


/**************************************************************************************************

Implementation

**************************************************************************************************/
#if defined(C89THREAD_IMPLEMENTATION)

/* Win32 */
#if defined(C89THREAD_WIN32)
#include <windows.h>
#include <limits.h> /* For LONG_MAX */

#ifndef C89THREAD_MALLOC
#define C89THREAD_MALLOC(sz)    HeapAlloc(GetProcessHeap(), 0, (sz))
#endif

#ifndef C89THREAD_FREE
#define C89THREAD_FREE(p)       HeapFree(GetProcessHeap(), 0, (p))
#endif

static int c89thrd_result_from_GetLastError(DWORD error)
{
    switch (error)
    {
        case ERROR_SUCCESS:             return c89thrd_success;
        case ERROR_NOT_ENOUGH_MEMORY:   return c89thrd_nomem;
        case ERROR_SEM_TIMEOUT:         return c89thrd_timedout;
        case ERROR_BUSY:                return c89thrd_busy;
        default: break;
    }

    return c89thrd_error;
}


static time_t c89timespec_to_milliseconds(const struct timespec ts)
{
    LONGLONG milliseconds;

    milliseconds = ((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
    if ((ts.tv_nsec % 1000000) != 0) {
        milliseconds += 1; /* We truncated a sub-millisecond amount of time. Add an extra millisecond to meet the minimum duration requirement. */
    }

    return (time_t)milliseconds;
}

static time_t c89timespec_diff_milliseconds(const struct timespec tsA, const struct timespec tsB)
{
    return (unsigned int)c89timespec_to_milliseconds(c89timespec_diff(tsA, tsB));
}


typedef struct
{
    c89thrd_start_t func;
    void* arg;
} c89thrd_start_data_win32;

static unsigned long WINAPI c89thrd_start_win32(void* pUserData)
{
    c89thrd_start_data_win32* pStartData = (c89thrd_start_data_win32*)pUserData;
    c89thrd_start_t func;
    void* arg;

    /* Make sure we make a copy of the start data here. That way we can free pStartData straight away (it was allocated in c89thrd_create()). */
    func = pStartData->func;
    arg  = pStartData->arg;

    /* We should free the data pointer before entering into the start function. That way when c89thrd_exit() is called we don't leak. */
    c89thread_free(pStartData, c89thread_allocation_type_general, NULL);

    return (unsigned long)func(arg);
}

int c89thrd_create(c89thrd_t* thr, c89thrd_start_t func, void* arg)
{
    HANDLE hThread;
    c89thrd_start_data_win32* pData;    /* <-- Needs to be allocated on the heap to ensure the data doesn't get trashed before the thread is entered. */

    if (thr == NULL) {
        return c89thrd_error;
    }

    *thr = NULL;    /* Safety. */

    if (func == NULL) {
        return c89thrd_error;
    }

    pData = c89thread_malloc(sizeof(*pData), c89thread_allocation_type_general, NULL);   /* <-- This will be freed when c89thrd_start_win32() is entered. */
    if (pData == NULL) {
        return c89thrd_nomem;
    }

    pData->func = func;
    pData->arg  = arg;

    hThread = CreateThread(NULL, 0, c89thrd_start_win32, pData, 0, NULL);
    if (hThread == NULL) {
        c89thread_free(pData, c89thread_allocation_type_general, NULL);
        return c89thrd_result_from_GetLastError(GetLastError());
    }

    *thr = (c89thrd_t)hThread;

    return c89thrd_success;
} 

int c89thrd_equal(c89thrd_t lhs, c89thrd_t rhs)
{
    /*
    Annoyingly, GetThreadId() is not defined for Windows XP. Need to conditionally enable this. I'm
    not sure how to do this any other way, so I'm falling back to a simple handle comparison. I don't
    think this is right, though. If anybody has any suggestions, let me know.
    */
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0502
    return GetThreadId((HANDLE)lhs) == GetThreadId((HANDLE)rhs);
#else
    return lhs == rhs;
#endif
}

c89thrd_t c89thrd_current(void)
{
    return (c89thrd_t)GetCurrentThread();
}

int c89thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    /*
    Sleeping is annoyingly complicated in C11. Nothing crazy or anything, but it's not just a simple
    millisecond sleep. These are the rules:
    
        * On success, return 0
        * When the sleep is interupted due to a signal, return -1
        * When any other error occurs, return some other negative value.
        * When the sleep is interupted, the `remaining` output parameter needs to be filled out with
          the remaining time.

    In order to detect a signal, we can use SleepEx(). This only has a resolution of 1 millisecond,
    however (this is true for everything on Windows). SleepEx() will return WAIT_IO_COMPLETION if
    some I/O completion event occurs. This is the best we'll get on Windows, I think.

    In order to calculate the value to place into `remaining`, we need to get the time before sleeping
    and then get the time after the sleeping. We'll then have enough information to calculate the
    difference which will be our remining. This is only required when the `remaining` parameter is not
    NULL. Unfortunately we cannot use timespec_get() here because it doesn't have good support with
    MinGW. We'll instead use Windows' high resolution performance counter which is supported back to
    Windows 2000.
    */
    static LARGE_INTEGER frequency;
    LARGE_INTEGER start;
    DWORD sleepResult;
    DWORD sleepMilliseconds;

    if (duration == NULL) {
        return c89thrd_error;
    }

    start.QuadPart = 0;

    if (remaining != NULL) {
        if (frequency.QuadPart == 0) {
            if (QueryPerformanceFrequency(&frequency) == FALSE) {
                frequency.QuadPart = 0; /* Just to be sure... */
                return c89thrd_error;
            }
        }

        if (QueryPerformanceCounter(&start) == FALSE) {
            return c89thrd_error;   /* Failed to retrieve the start time. */
        }
    }

    sleepMilliseconds = (DWORD)((duration->tv_sec * 1000) + (duration->tv_nsec / 1000000));

    /*
    A small, but important detail here. The C11 spec states that thrd_sleep() should sleep for a
    *minium* of the specified duration. In the above calculation we converted nanoseconds to
    milliseconds, however this requires a division which may truncate a non-zero sub-millisecond
    amount of time. We need to add an extra millisecond to meet the minimum duration requirement if
    indeed we truncated.
    */
    if ((duration->tv_nsec % 1000000) != 0) {
        sleepMilliseconds += 1; /* We truncated a sub-millisecond amount of time. Add an extra millisecond to meet the minimum duration requirement. */
    }
    
    sleepResult = SleepEx(sleepMilliseconds, TRUE); /* <-- Make this sleep alertable so we can detect WAIT_IO_COMPLETION and return -1. */
    if (sleepResult == 0) {
        if (remaining != NULL) {
            remaining->tv_sec  = 0;
            remaining->tv_nsec = 0;
        }

        return c89thrd_success;
    }

    /*
    Getting here means we didn't sleep for the specified amount of time. We need to fill `remaining`.
    To do this, we need to find out out much time has elapsed and then offset that will the requested
    duration. This is the hard part of the process because we need to convert to and from timespec.
    */
    if (remaining != NULL) {
        LARGE_INTEGER end;
        if (QueryPerformanceCounter(&end)) {
            LARGE_INTEGER elapsed;
            elapsed.QuadPart = end.QuadPart - start.QuadPart;

            /*
            The remaining amount of time is the requested duration, minus the elapsed time. This section warrents an explanation.

            The section below is converting between our performance counters and timespec structures. Just above we calculated the
            amount of the time that has elapsed since sleeping. By subtracting the requested duration from the elapsed duration,
            we'll be left with the remaining duration.

            The first thing we do is convert the requested duration to a LARGE_INTEGER which will be based on the performance counter
            frequency we retrieved earlier. The Windows high performance counters are based on seconds, so a counter divided by the
            frequency will give you the representation in seconds. By multiplying the counter by 1000 before the division by the
            frequency you'll have a result in milliseconds, etc.

            Once the remainder has be calculated based on the high performance counters, it's converted to the timespec structure
            which is just the reverse.
            */
            {
                LARGE_INTEGER durationCounter;
                LARGE_INTEGER remainingCounter;

                durationCounter.QuadPart = ((duration->tv_sec * frequency.QuadPart) + ((duration->tv_nsec * frequency.QuadPart) / 1000000000));
                if (durationCounter.QuadPart > elapsed.QuadPart) {
                    remainingCounter.QuadPart = durationCounter.QuadPart - elapsed.QuadPart;
                } else {
                    remainingCounter.QuadPart = 0;   /* For safety. Ensures we don't go negative. */
                }

                remaining->tv_sec  = (time_t)((remainingCounter.QuadPart * 1)          / frequency.QuadPart);
                remaining->tv_nsec =  (long)(((remainingCounter.QuadPart * 1000000000) / frequency.QuadPart) - (remaining->tv_sec * (LONGLONG)1000000000));
            }
        } else {
            remaining->tv_sec  = 0; /* Just for safety. */
            remaining->tv_nsec = 0;
        }
    }

    if (sleepResult == WAIT_IO_COMPLETION) {
        return c89thrd_signal;  /* -1 */
    } else {
        return c89thrd_error;   /* "other negative value if an error occurred." */
    }
}

void c89thrd_yield(void)
{
    Sleep(0);
}

void c89thrd_exit(int res)
{
    ExitThread((DWORD)res);
}

int c89thrd_detach(c89thrd_t thr)
{
    /*
    The documentation for thrd_detach() says explicitly that any error should return thrd_error.
    We'll do the same, so make sure c89thrd_result_from_GetLastError() is not used here.
    */
    BOOL result;

    result = CloseHandle((HANDLE)thr);
    if (!result) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89thrd_join(c89thrd_t thr, int* res)
{
    /*
    Like thrd_detach(), the documentation for thrd_join() says to return thrd_success or thrd_error.
    Therefore, make sure c89thrd_result_from_GetLastError() is not used here.

    In Win32, waiting for the thread to complete and retrieving the result is done as two separate
    steps.
    */

    /* Wait for the thread. */
    if (WaitForSingleObject((HANDLE)thr, INFINITE) == WAIT_FAILED) {
        return c89thrd_error;   /* Wait failed. */
    }

    /* Retrieve the result code if required. */
    if (res != NULL) {
        DWORD exitCode;
        if (GetExitCodeThread((HANDLE)thr, &exitCode) == FALSE) {
            return c89thrd_error;
        }

        *res = (int)exitCode;
    }

    /*
    It's not entirely clear from the documentation for thrd_join() as to whether or not the thread
    handle should be closed at this point. I think it makes sense to close it here, as I don't recall
    ever seeing a pattern or joining a thread, and then explicitly closing the thread handle. I think
    joining should be an implicit detach.
    */
    return c89thrd_detach(thr);
}


int c89mtx_init(c89mtx_t* mutex, int type)
{
    HANDLE hMutex;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    /* Initialize the object to zero for safety. */
    mutex->handle = NULL;
    mutex->type   = 0;

    /*
    CreateMutex() will create a thread-aware mutex (allowing recursiveness), whereas an auto-reset
    event (CreateEvent()) is not thread-aware and will deadlock (will not allow recursiveness). In
    Win32 I'm making all mutex's timeable.
    */
    if ((type & c89mtx_recursive) != 0) {
        hMutex = CreateMutex(NULL, FALSE, NULL);
    } else {
        hMutex = CreateEvent(NULL, FALSE, TRUE, NULL);
    }

    if (hMutex == NULL) {
        return c89thrd_result_from_GetLastError(GetLastError());
    }

    mutex->handle = (c89thread_handle)hMutex;
    mutex->type   = type;

    return c89thrd_success;
}

void c89mtx_destroy(c89mtx_t* mutex)
{
    if (mutex == NULL) {
        return;
    }

    CloseHandle((HANDLE)mutex->handle);
}

int c89mtx_lock(c89mtx_t* mutex)
{
    DWORD result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89mtx_timedlock(c89mtx_t* mutex, const struct timespec* time_point)
{
    DWORD result;

    if (mutex == NULL || time_point == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, (DWORD)c89timespec_diff_milliseconds(*time_point, c89timespec_now()));
    if (result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89mtx_trylock(c89mtx_t* mutex)
{
    DWORD result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)mutex->handle, 0);
    if (result != WAIT_OBJECT_0) {
        return c89thrd_busy;
    }

    return c89thrd_success;
}

int c89mtx_unlock(c89mtx_t* mutex)
{
    BOOL result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    if ((mutex->type & c89mtx_recursive) != 0) {
        result = ReleaseMutex((HANDLE)mutex->handle);
    } else {
        result = SetEvent((HANDLE)mutex->handle);
    }

    if (!result) {
        return c89thrd_error;
    }

    return c89thrd_success;
}



int c89cnd_init(c89cnd_t* cnd)
{
    if (cnd == NULL) {
        return c89thrd_error;
    }

    /* Not supporting condition variables on Win32. */
    return c89thrd_error;
}

void c89cnd_destroy(c89cnd_t* cnd)
{
    if (cnd == NULL) {
        return;
    }

    /* Not supporting condition variables on Win32. */
}

int c89cnd_signal(c89cnd_t* cnd)
{
    if (cnd == NULL) {
        return c89thrd_error;
    }

    /* Not supporting condition variables on Win32. */
    return c89thrd_error;
}

int c89cnd_broadcast(c89cnd_t* cnd)
{
    if (cnd == NULL) {
        return c89thrd_error;
    }

    /* Not supporting condition variables on Win32. */
    return c89thrd_error;
}

int c89cnd_wait(c89cnd_t* cnd, c89mtx_t* mtx)
{
    if (cnd == NULL) {
        return c89thrd_error;
    }

    (void)mtx;

    /* Not supporting condition variables on Win32. */
    return c89thrd_error;
}

int c89cnd_timedwait(c89cnd_t* cnd, c89mtx_t* mtx, const struct timespec* time_point)
{
    if (cnd == NULL) {
        return c89thrd_error;
    }

    (void)mtx;
    (void)time_point;

    /* Not supporting condition variables on Win32. */
    return c89thrd_error;
}



int c89sem_init(c89sem_t* sem, int value)
{
    HANDLE hSemaphore;

    if (sem == NULL) {
        return c89thrd_error;
    }

    *sem = NULL;

    hSemaphore = CreateSemaphore(NULL, value, LONG_MAX, NULL);
    if (hSemaphore == NULL) {
        return c89thrd_error;
    }

    *sem = hSemaphore;

    return c89thrd_success;
}

void c89sem_destroy(c89sem_t* sem)
{
    if (sem == NULL) {
        return;
    }

    CloseHandle((HANDLE)*sem);
}

int c89sem_wait(c89sem_t* sem)
{
    DWORD result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)*sem, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89sem_timedwait(c89sem_t* sem, const struct timespec* time_point)
{
    DWORD result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)*sem, (DWORD)c89timespec_diff_milliseconds(*time_point, c89timespec_now()));
    if (result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89sem_post(c89sem_t* sem)
{
    BOOL result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = ReleaseSemaphore((HANDLE)*sem, 1, NULL);
    if (!result) {
        return c89thrd_error;
    }

    return c89thrd_success;
}



int c89evnt_init(c89evnt_t* evnt)
{
    HANDLE hEvent;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    *evnt = NULL;

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL) {
        return c89thrd_error;
    }

    *evnt = hEvent;

    return c89thrd_success;
}

void c89evnt_destroy(c89evnt_t* evnt)
{
    if (evnt == NULL) {
        return;
    }

    CloseHandle((HANDLE)*evnt);
}

int c89evnt_wait(c89evnt_t* evnt)
{
    DWORD result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)*evnt, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89evnt_timedwait(c89evnt_t* evnt, const struct timespec* time_point)
{
    DWORD result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = WaitForSingleObject((HANDLE)*evnt, (DWORD)c89timespec_diff_milliseconds(*time_point, c89timespec_now()));
    if (result != WAIT_OBJECT_0) {
        if (result == WAIT_TIMEOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89evnt_signal(c89evnt_t* evnt)
{
    BOOL result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = SetEvent((HANDLE)*evnt);
    if (!result) {
        return c89thrd_error;
    }

    return c89thrd_success;
}
#endif

/* POSIX */
#if defined(C89THREAD_POSIX)
#include <stdlib.h> /* For malloc(), free(). */
#include <errno.h>  /* For errno_t. */

#ifndef C89THREAD_MALLOC
#define C89THREAD_MALLOC(sz)    malloc(sz)
#endif

#ifndef C89THREAD_FREE
#define C89THREAD_FREE(p)       free(p)
#endif


static int c89thrd_result_from_errno(int e)
{
    switch (e)
    {
        case 0:         return c89thrd_success;
        case ENOMEM:    return c89thrd_nomem;
        case ETIME:     return c89thrd_timedout;
        case ETIMEDOUT: return c89thrd_timedout;
        case EBUSY:     return c89thrd_busy;
    }

    return c89thrd_error;
}


typedef struct
{
    c89thrd_start_t func;
    void* arg;
} c89thrd_start_data_posix;

static void* c89thrd_start_posix(void* pUserData)
{
    c89thrd_start_data_posix* pStartData = (c89thrd_start_data_posix*)pUserData;
    c89thrd_start_t func;
    void* arg;

    /* Make sure we make a copy of the start data here. That way we can free pStartData straight away (it was allocated in c89thrd_create()). */
    func = pStartData->func;
    arg  = pStartData->arg;

    /* We should free the data pointer before entering into the start function. That way when c89thrd_exit() is called we don't leak. */
    c89thread_free(pStartData, c89thread_allocation_type_general, NULL);

    return (void*)(c89thread_intptr)func(arg);
}

int c89thrd_create(c89thrd_t* thr, c89thrd_start_t func, void* arg)
{
    int result;
    c89thrd_start_data_posix* pData;
    pthread_t thread;

    if (thr == NULL) {
        return c89thrd_error;
    }

    *thr = 0;   /* Safety. */

    if (func == NULL) {
        return c89thrd_error;
    }

    pData = c89thread_malloc(sizeof(*pData), c89thread_allocation_type_general, NULL);   /* <-- This will be freed when c89thrd_start_posix() is entered. */
    if (pData == NULL) {
        return c89thrd_nomem;
    }

    pData->func = func;
    pData->arg  = arg;

    result = pthread_create(&thread, NULL, c89thrd_start_posix, pData);
    if (result != 0) {
        c89thread_free(pData, c89thread_allocation_type_general, NULL);
        return c89thrd_result_from_errno(errno);
    }

    *thr = thread;

    return c89thrd_success;
}

int c89thrd_equal(c89thrd_t lhs, c89thrd_t rhs)
{
    return pthread_equal(lhs, rhs);
}

c89thrd_t c89thrd_current(void)
{
    return pthread_self();
}

int c89thrd_sleep(const struct timespec* duration, struct timespec* remaining)
{
    /*
    The documentation for thrd_sleep() mentions nanosleep(), so we'll go ahead and use that. We need
    to keep in mind the requirement to handle signal interrupts.
    */
    int result = nanosleep(duration, remaining);
    if (result != 0) {
        if (result == EINTR) {
            return c89thrd_signal;
        }

        return c89thrd_error;    
    }
    
    return c89thrd_success;
}

void c89thrd_yield(void)
{
    sched_yield();
}

void c89thrd_exit(int res)
{
    pthread_exit((void*)(c89thread_intptr)res);
}

int c89thrd_detach(c89thrd_t thr)
{
    /*
    The documentation for thrd_detach() explicitly says c89thrd_success if successful or c89thrd_error
    for any other error. Don't use c89thrd_result_from_errno() here.
    */
    int result = pthread_detach(thr);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89thrd_join(c89thrd_t thr, int* res)
{
    /* Same rules apply here as thrd_detach() with respect to the return value. */
    void* retval;
    int result = pthread_join(thr, &retval);
    if (result != 0) {
        return c89thrd_error;   
    }

    if (res != NULL) {
        *res = (int)(c89thread_intptr)retval;
    }

    return c89thrd_success;
}



int c89mtx_init(c89mtx_t* mutex, int type)
{
    int result;
    pthread_mutexattr_t attr;   /* For specifying whether or not the mutex is recursive. */

    if (mutex == NULL) {
        return c89thrd_error;
    }

    pthread_mutexattr_init(&attr);
    if ((type & c89mtx_recursive) != 0) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    } else {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);     /* Will deadlock. Consistent with Win32. */
    }

    result = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

void c89mtx_destroy(c89mtx_t* mutex)
{
    if (mutex == NULL) {
        return;
    }

    pthread_mutex_destroy(mutex);
}

int c89mtx_lock(c89mtx_t* mutex)
{
    int result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_lock(mutex);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89mtx_timedlock(c89mtx_t* mutex, const struct timespec* time_point)
{
    int result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_timedlock(mutex, time_point);
    if (result != 0) {
        if (result == ETIMEDOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89mtx_trylock(c89mtx_t* mutex)
{
    int result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_trylock(mutex);
    if (result != 0) {
        if (result == EBUSY) {
            return c89thrd_busy;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89mtx_unlock(c89mtx_t* mutex)
{
    int result;

    if (mutex == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_unlock(mutex);
    if (result == 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}



int c89cnd_init(c89cnd_t* cnd)
{
    int result;

    if (cnd == NULL) {
        return c89thrd_error;
    }

    result = pthread_cond_init(cnd, NULL);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

void c89cnd_destroy(c89cnd_t* cnd)
{
    if (cnd == NULL) {
        return;
    }

    pthread_cond_destroy(cnd);
}

int c89cnd_signal(c89cnd_t* cnd)
{
    int result;

    if (cnd == NULL) {
        return c89thrd_error;
    }

    result = pthread_cond_signal(cnd);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89cnd_broadcast(c89cnd_t* cnd)
{
    int result;

    if (cnd == NULL) {
        return c89thrd_error;
    }

    result = pthread_cond_broadcast(cnd);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89cnd_wait(c89cnd_t* cnd, c89mtx_t* mtx)
{
    int result;

    if (cnd == NULL) {
        return c89thrd_error;
    }

    result = pthread_cond_wait(cnd, mtx);
    if (result != 0) {
        return c89thrd_error;
    }

    return c89thrd_success;
}

int c89cnd_timedwait(c89cnd_t* cnd, c89mtx_t* mtx, const struct timespec* time_point)
{
    int result;

    if (cnd == NULL) {
        return c89thrd_error;
    }

    result = pthread_cond_timedwait(cnd, mtx, time_point);
    if (result != 0) {
        if (result == ETIMEDOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    return c89thrd_success;
}



int c89sem_init(c89sem_t* sem, int value)
{
    int result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    sem->value = value;

    result = pthread_mutex_init(&sem->lock, NULL);
    if (result != 0) {
        return c89thrd_result_from_errno(result);  /* Failed to create mutex. */
    }

    result = pthread_cond_init(&sem->cond, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&sem->lock);
        return c89thrd_result_from_errno(result);  /* Failed to create condition variable. */
    }

    return c89thrd_success;
}

void c89sem_destroy(c89sem_t* sem)
{
    if (sem == NULL) {
        return;
    }

    pthread_cond_destroy(&sem->cond);
    pthread_mutex_destroy(&sem->lock);
}

int c89sem_wait(c89sem_t* sem)
{
    int result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_lock(&sem->lock);
    if (result != 0) {
        return c89thrd_error;
    }

    /* We need to wait on a condition variable before escaping. We can't return from this function until the semaphore has been signaled. */
    while (sem->value == 0) {
        pthread_cond_wait(&sem->cond, &sem->lock);
    }

    sem->value -= 1;
    pthread_mutex_unlock(&sem->lock);

    return c89thrd_success;
}

int c89sem_timedwait(c89sem_t* sem, const struct timespec* time_point)
{
    int result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_timedlock(&sem->lock, time_point);
    if (result != 0) {
        if (result == ETIMEDOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    /* We need to wait on a condition variable before escaping. We can't return from this function until the semaphore has been signaled. */
    while (sem->value == 0) {
        result = pthread_cond_timedwait(&sem->cond, &sem->lock, time_point);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&sem->lock);
            return c89thrd_timedout;
        }
    }

    sem->value -= 1;

    pthread_mutex_unlock(&sem->lock);
    return c89thrd_success;
}

int c89sem_post(c89sem_t* sem)
{
    int result;

    if (sem == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_lock(&sem->lock);
    if (result != 0) {
        return c89thrd_error;
    }

    sem->value += 1;
    pthread_cond_signal(&sem->cond);

    pthread_mutex_unlock(&sem->lock);
    return c89thrd_success;
}



int c89evnt_init(c89evnt_t* evnt)
{
    int result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    evnt->value = 0;

    result = pthread_mutex_init(&evnt->lock, NULL);
    if (result != 0) {
        return c89thrd_result_from_errno(result);  /* Failed to create mutex. */
    }

    result = pthread_cond_init(&evnt->cond, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&evnt->lock);
        return c89thrd_result_from_errno(result);  /* Failed to create condition variable. */
    }

    return c89thrd_success;
}

void c89evnt_destroy(c89evnt_t* evnt)
{
    if (evnt == NULL) {
        return;
    }

    pthread_cond_destroy(&evnt->cond);
    pthread_mutex_destroy(&evnt->lock);
}

int c89evnt_wait(c89evnt_t* evnt)
{
    int result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_lock(&evnt->lock);
    if (result != 0) {
        return c89thrd_error;
    }

    while (evnt->value == 0) {
        pthread_cond_wait(&evnt->cond, &evnt->lock);
    }
    evnt->value = 0;  /* Auto-reset. */

    pthread_mutex_unlock(&evnt->lock);
    return c89thrd_success;
}

int c89evnt_timedwait(c89evnt_t* evnt, const struct timespec* time_point)
{
    int result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_timedlock(&evnt->lock, time_point);
    if (result != 0) {
        if (result == ETIMEDOUT) {
            return c89thrd_timedout;
        }

        return c89thrd_error;
    }

    while (evnt->value == 0) {
        result = pthread_cond_timedwait(&evnt->cond, &evnt->lock, time_point);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&evnt->lock);
            return c89thrd_timedout;
        }
    }
    evnt->value = 0;  /* Auto-reset. */

    pthread_mutex_unlock(&evnt->lock);
    return c89thrd_success;
}

int c89evnt_signal(c89evnt_t* evnt)
{
    int result;

    if (evnt == NULL) {
        return c89thrd_error;
    }

    result = pthread_mutex_lock(&evnt->lock);
    if (result != 0) {
        return c89thrd_error;
    }

    evnt->value = 1;
    pthread_cond_signal(&evnt->cond);

    pthread_mutex_unlock(&evnt->lock);
    return c89thrd_success;
}
#endif


#if defined(_WIN32)

/* We'll need windows.h for a few timing things here. Sorry. */
#if !defined(C89THREAD_WIN32)
#include <windows.h>
#endif

void c89timespec_get(struct timespec* ts, int base)
{
    FILETIME ft;
    LONGLONG currentMilliseconds;

    if (ts == NULL) {
        return;
    }

    ts->tv_sec  = 0;
    ts->tv_nsec = 0;

    /* Currently only supporting UTC. */
    if (base != TIME_UTC) {
        return;
    }

    GetSystemTimeAsFileTime(&ft);
    currentMilliseconds = (((LONGLONG)ft.dwHighDateTime << 32) | (LONGLONG)ft.dwLowDateTime) / 10000;
    currentMilliseconds = currentMilliseconds - ((LONGLONG)116444736 * 100000); /* Windows to Unix epoch. Normal value is 11644473600000LL, but VC6 doesn't like 64-bit constants. */

    ts->tv_sec  = (time_t)(currentMilliseconds / 1000);
    ts->tv_nsec =  (long)((currentMilliseconds - (ts->tv_sec * 1000)) * 1000000);
}
#else
void c89timespec_get(struct timespec* ts, int base)
{
    return timespec_get(ts, base);
}
#endif


struct timespec c89timespec_now()
{
    struct timespec ts;

    c89timespec_get(&ts, TIME_UTC);

    return ts;
}

struct timespec c89timespec_seconds(time_t seconds)
{
    struct timespec ts;

    ts.tv_sec  = seconds;
    ts.tv_nsec = 0;

    return ts;
}

struct timespec c89timespec_milliseconds(time_t milliseconds)
{
    struct timespec ts;

    ts.tv_sec  = milliseconds / 1000;
    ts.tv_nsec = (long)((milliseconds - (ts.tv_sec * 1000)) * 1000000);

    return ts;
}

struct timespec c89timespec_diff(struct timespec lhs, struct timespec rhs)
{
    struct timespec diff;

    diff.tv_sec = lhs.tv_sec - rhs.tv_sec;

    if (lhs.tv_nsec > rhs.tv_nsec) {
        diff.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;
    } else {
        diff.tv_nsec = lhs.tv_nsec + 1000000000 - rhs.tv_nsec;
        diff.tv_sec -= 1;
    }

    return diff;
}

struct timespec c89timespec_add(struct timespec tsA, struct timespec tsB)
{
    struct timespec ts;

    ts.tv_sec  = tsA.tv_sec  + tsB.tv_sec;
    ts.tv_nsec = tsA.tv_nsec + tsB.tv_nsec;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec  += 1;
    }
    
    return ts;
}


int c89thrd_sleep_timespec(struct timespec ts)
{
    return c89thrd_sleep(&ts, NULL);
}

int c89thrd_sleep_milliseconds(int milliseconds)
{
    if (milliseconds < 0) {
        milliseconds = 0;
    }

    return c89thrd_sleep_timespec(c89timespec_milliseconds(milliseconds));
}


/*
Memory Management
*/
static c89thread_allocation_callbacks g_c89thread_AllocationCallbacks;

void c89thread_set_allocation_callbacks(const c89thread_allocation_callbacks* pCallbacks)
{
    if (pCallbacks == NULL) {
        g_c89thread_AllocationCallbacks.pUserData = NULL;
        g_c89thread_AllocationCallbacks.onMalloc  = NULL;
        g_c89thread_AllocationCallbacks.onFree    = NULL;
    } else {
        g_c89thread_AllocationCallbacks = *pCallbacks;
    }
}

void* c89thread_malloc(size_t sz, c89thread_allocation_type type, const c89thread_allocation_callbacks* pCallbacks)
{
    if (pCallbacks != NULL) {
        if (pCallbacks->onMalloc != NULL) {
            return pCallbacks->onMalloc(sz, type, pCallbacks->pUserData);
        } else {
            return C89THREAD_MALLOC(sz);
        }
    } else {
        if (g_c89thread_AllocationCallbacks.onMalloc != NULL) {
            return g_c89thread_AllocationCallbacks.onMalloc(sz, type, g_c89thread_AllocationCallbacks.pUserData);
        } else {
            return C89THREAD_MALLOC(sz);
        }
    }
}

void c89thread_free(void* p, c89thread_allocation_type type, const c89thread_allocation_callbacks* pCallbacks)
{
    if (p == NULL) {
        return;
    }

    if (pCallbacks != NULL) {
        if (pCallbacks->onFree != NULL) {
            pCallbacks->onFree(p, type, pCallbacks->pUserData);
        } else {
            C89THREAD_FREE(p);
        }
    } else {
        if (g_c89thread_AllocationCallbacks.onFree != NULL) {
            g_c89thread_AllocationCallbacks.onFree(p, type, g_c89thread_AllocationCallbacks.pUserData);
        } else {
            C89THREAD_FREE(p);
        }
    }
}
#endif /* C89THREAD_IMPLEMENTATION */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
