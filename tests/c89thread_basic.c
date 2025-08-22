
/*#define C89THREAD_USE_PTHREAD*/
#define C89THREAD_IMPLEMENTATION
#include "../c89thread.h"

#include <stdio.h>

c89mtx_t g_mutex;

C89THREAD_THREAD_LOCAL int g_threadLocalValue = 1;

int thread_proc_1(void* arg)
{
    c89mtx_lock(&g_mutex);
    {
        struct timespec ts;
        ts.tv_sec  = 2;
        ts.tv_nsec = 0;

        printf("MUTEX LOCKED\n");
        c89thrd_sleep(&ts, NULL);
        printf("MUTEX UNLOCKED\n");
    }
    c89mtx_unlock(&g_mutex);

    (void)arg;

    return 0;
}

int main(int argc, char** argv)
{
    int result;
    c89thrd_t thread;
    struct timespec ts;

#if defined(C89THREAD_POSIX)
    printf("sizeof(pthread_t) = %lu\n", sizeof(pthread_t));
    printf("sizeof(pthread_mutex_t) = %lu\n", sizeof(pthread_mutex_t));
    printf("sizeof(pthread_cond_t) = %lu\n", sizeof(pthread_cond_t));
#endif


    result = c89mtx_init(&g_mutex, c89mtx_plain);
    if (result != c89thrd_success) {
        printf("Failed to create mutex: %d\n", result);
    }

    
    result = c89thrd_create(&thread, thread_proc_1, NULL);
    if (result != c89thrd_success) {
        printf("Failed to create thread: %d\n", result);
        return -1;
    }

    /* Wait a bit for the other thread to grab the lock. */
    c89thrd_sleep_milliseconds(1000);


    

    ts = c89timespec_add(c89timespec_now(), c89timespec_milliseconds(1000));

    printf("locking...\n");
    result = c89mtx_timedlock(&g_mutex, &ts);
    printf("c89mtx_timedlock() = %d\n", result);

    c89thrd_join(thread, NULL);

    printf("recursive lock...\n");
    {
        c89mtx_t recursiveLock;
        result = c89mtx_init(&recursiveLock, c89mtx_recursive);
        if (result != c89thrd_success) {
            printf("Failed to create recursive mutex: %d\n", result);
        }

        c89mtx_lock(&recursiveLock);
        {
            c89mtx_lock(&recursiveLock);
            {

            }
            c89mtx_unlock(&recursiveLock);
        }
        c89mtx_unlock(&recursiveLock);

        c89mtx_destroy(&recursiveLock);
    }
    printf("unlocked\n");



    printf("Logical CPU Count:  %d\n", c89thread_get_logical_cpu_count());


    (void)argc;
    (void)argv;

    return 0;
}
