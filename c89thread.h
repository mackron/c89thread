/*
C89 compatible threads. Choice of public domain or MIT-0. See license statements at the end of this file.

David Reid - mackron@gmail.com
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

enum
{
    c89thrd_success  =  0,
    c89thrd_signal   = -1,  /* Not one of the standard results specified by C11, but -1 is used to indicate a signal in some APIs (thrd_sleep(), for example). */
    c89thrd_nomem    = -2,
    c89thrd_timedout = -3,
    c89thrd_busy     = -4,
    c89thrd_error    = -5
};


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
typedef c89thread_handle    c89mtx_t;   /* HANDLE, CreateMutex(), CreateEvent() */
#else
typedef pthread_mutex_t     c89mtx_t;
#endif


/* cnd_t */
#if defined(C89THREAD_WIN32)
typedef struct { void* p; } c89cnd_t;   /* CONDITION_VARIABLE, InitializeConditionVariable() */
#else
typedef pthread_cond_t      c89cnd_t;
#endif


#if defined(__cplusplus)
}
#endif
#endif  /* c89thread_h */


/**************************************************************************************************

Implementation

**************************************************************************************************/
#if defined(C89THREAD_IMPLEMENTATION)

static void* c89thread_malloc(size_t sz);
static void  c89thread_free(void* p);

/* Win32 */
#if defined(C89THREAD_WIN32)
#include <windows.h>

static void* c89thread_malloc(size_t sz)
{
    return HeapAlloc(GetProcessHeap(), 0, (sz));
}

static void c89thread_free(void* p)
{
    HeapFree(GetProcessHeap(), 0, (p));
}


int c89thrd_result_from_GetLastError(DWORD error)
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
    c89thread_free(pStartData);

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

    pData = c89thread_malloc(sizeof(*pData));   /* <-- This will be freed when c89thrd_start_win32() returns. */
    if (pData == NULL) {
        return c89thrd_nomem;
    }

    pData->func = func;
    pData->arg  = arg;

    hThread = CreateThread(NULL, 0, c89thrd_start_win32, pData, 0, NULL);
    if (hThread == NULL) {
        c89thread_free(pData);
        return c89thrd_result_from_GetLastError(GetLastError());
    }

    *thr = (c89thrd_t)hThread;

    return c89thrd_success;
} 

int c89thrd_equal(c89thrd_t lhs, c89thrd_t rhs)
{
    return GetThreadId((HANDLE)lhs) == GetThreadId((HANDLE)rhs);
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

                remaining->tv_sec  =  (remainingCounter.QuadPart * 1)          / frequency.QuadPart;
                remaining->tv_nsec = ((remainingCounter.QuadPart * 1000000000) / frequency.QuadPart) - (remaining->tv_sec * (LONGLONG)1000000000);
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
    if (CloseHandle((HANDLE)thr) != 0) {
        return c89thrd_success;
    } else {
        return c89thrd_error;
    }
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
#endif

/* POSIX */
#if defined(C89THREAD_POSIX)
#include <stdlib.h> /* For malloc(), free(). */
#include <errno.h>  /* For errno_t. */

static void* c89thread_malloc(size_t sz)
{
    return malloc(sz);
}

static void c89thread_free(void* p)
{
    return free(p);
}


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
    c89thread_free(pStartData);

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

    pData = c89thread_malloc(sizeof(*pData));   /* <-- This will be freed when c89thrd_start_win32() returns. */
    if (pData == NULL) {
        return c89thrd_nomem;
    }

    pData->func = func;
    pData->arg  = arg;

    result = pthread_create(&thread, NULL, c89thrd_start_posix, pData);
    if (result != 0) {
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
    if (result == 0) {
        return c89thrd_success;
    }

    if (result == EINTR) {
        return c89thrd_signal;
    }

    return c89thrd_error;
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
    if (result == 0) {
        return c89thrd_success;
    } else {
        return c89thrd_error;
    }
}

int c89thrd_join(c89thrd_t thr, int* res)
{
    /* Same rules apply here as thrd_detach() with respect to the return value. */
    void* retval;
    int result = pthread_join(thr, &retval);
    if (result == 0) {
        if (res != NULL) {
            *res = (int)(c89thread_intptr)retval;
        }

        return c89thrd_success;
    } else {
        return c89thrd_error;
    }
}
#endif



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
