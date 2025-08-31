#define C89THREAD_IMPLEMENTATION
#include "c89thread.h"

/* BEG c89thread_test.c */
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct c89thread_test c89thread_test;

typedef int (* c89thread_test_proc)(c89thread_test* pUserData);

struct c89thread_test
{
    const char* name;
    c89thread_test_proc proc;
    void* pUserData;
    int result;
    c89thread_test* pFirstChild;
    c89thread_test* pNextSibling;
};

void c89thread_test_init(c89thread_test* pTest, const char* name, c89thread_test_proc proc, void* pUserData, c89thread_test* pParent)
{
    if (pTest == NULL) {
        return;
    }

    memset(pTest, 0, sizeof(c89thread_test));
    pTest->name = name;
    pTest->proc = proc;
    pTest->pUserData = pUserData;
    pTest->result = c89thrd_success;
    pTest->pFirstChild = NULL;
    pTest->pNextSibling = NULL;

    if (pParent != NULL) {
        if (pParent->pFirstChild == NULL) {
            pParent->pFirstChild = pTest;
        } else {
            c89thread_test* pSibling = pParent->pFirstChild;
            while (pSibling->pNextSibling != NULL) {
                pSibling = pSibling->pNextSibling;
            }

            pSibling->pNextSibling = pTest;
        }
    }
}

void c89thread_test_count(c89thread_test* pTest, int* pCount, int* pPassed)
{
    c89thread_test* pChild;

    if (pTest == NULL) {
        return;
    }

    *pCount += 1;

    if (pTest->result == c89thrd_success) {
        *pPassed += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        c89thread_test_count(pChild, pCount, pPassed);
        pChild = pChild->pNextSibling;
    }
}

int c89thread_test_run(c89thread_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return c89thrd_error;
    }

    if (pTest->name != NULL && pTest->proc != NULL) {
        printf("Running Test: %s\n", pTest->name);
    }

    if (pTest->proc != NULL) {
        pTest->result = pTest->proc(pTest);
        if (pTest->result != c89thrd_success) {
            return pTest->result;
        }
    }

    /* Now we need to recursively execute children. If any child test fails, the parent test needs to be marked as failed as well. */
    {
        c89thread_test* pChild = pTest->pFirstChild;
        while (pChild != NULL) {
            int result = c89thread_test_run(pChild);
            if (result != c89thrd_success) {
                pTest->result = result;
            }

            pChild = pChild->pNextSibling;
        }
    }

    /* Now count the number of failed tests and report success or failure depending on the result. */
    c89thread_test_count(pTest, &testCount, &passedCount);

    return (testCount == passedCount) ? c89thrd_success : c89thrd_error;
}

void c89thread_test_print_local_result(c89thread_test* pTest, int level)
{
    if (pTest == NULL) {
        return;
    }

    printf("[%s] %*s%s\n", pTest->result == c89thrd_success ? "PASS" : "FAIL", level * 2, "", pTest->name);
}

void c89thread_test_print_child_results(c89thread_test* pTest, int level)
{
    c89thread_test* pChild;

    if (pTest == NULL) {
        return;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        c89thread_test_print_local_result(pChild, level);
        c89thread_test_print_child_results(pChild, level + 1);

        pChild = pChild->pNextSibling;
    }
}

void c89thread_test_print_result(c89thread_test* pTest, int level)
{
    c89thread_test* pChild;

    if (pTest == NULL) {
        return;
    }

    if (pTest->name != NULL) {
        printf("[%s] %*s%s\n", pTest->result == c89thrd_success ? "PASS" : "FAIL", level * 2, "", pTest->name);
        level += 1;
    }

    pChild = pTest->pFirstChild;
    while (pChild != NULL) {
        c89thread_test_print_result(pChild, level);
        pChild = pChild->pNextSibling;
    }
}

void c89thread_test_print_summary(c89thread_test* pTest)
{
    /* Start our counts at -1 to exclude the root test. */
    int testCount = -1;
    int passedCount = -1;

    if (pTest == NULL) {
        return;
    }

    /* This should only be called on a root test. */
    assert(pTest->name == NULL);

    printf("=== Test Summary ===\n");
    c89thread_test_print_result(pTest, 0);

    /* We need to count how many tests failed. */
    c89thread_test_count(pTest, &testCount, &passedCount);
    printf("---\n%s%d / %d tests passed.\n", (testCount == passedCount) ? "[PASS]: " : "[FAIL]: ", passedCount, testCount);
}
/* END c89thread_test.c */



/* BEG test_c89thrd_create */
static int c89thread_test_c89thrd_create__entry(void* pUserData)
{
    /* Do nothing. */
    (void)pUserData;
    return 0;
}

int c89thread_test_c89thrd_create(c89thread_test* pTest)
{
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_create__entry, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create failed\n", pTest->name);
        return result;
    }

    return c89thrd_success;
}
/* END test_c89thrd_create */

/* BEG test_c89thrd_join */
typedef struct
{
    int value;
} c89thread_test_c89thrd_join_data;

static int c89thread_test_c89thrd_join__entry(void* pUserData)
{
    c89thread_test_c89thrd_join_data* pData = (c89thread_test_c89thrd_join_data*)pUserData;

    pData->value = 42;  /* Set some value to return. */

    return 0;
}

int c89thread_test_c89thrd_join(c89thread_test* pTest)
{
    c89thread_test_c89thrd_join_data data;
    c89thrd_t thread;
    int result;

    data.value = 0;  /* Initialize value. */

    result = c89thrd_create(&thread, c89thread_test_c89thrd_join__entry, &data);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        return result;
    }

    if (data.value != 42) {
        printf("%s: Expected value 42, got %d\n", pTest->name, data.value);
        return c89thrd_error;
    }

    return c89thrd_success;
}
/* END test_c89thrd_join */

/* BEG test_c89thrd_detach */
static int c89thread_test_c89thrd_detach__entry(void* pUserData)
{
    (void)pUserData;
    return 0;
}

int c89thread_test_c89thrd_detach(c89thread_test* pTest)
{
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_detach__entry, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_detach(thread);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_detach() failed.\n", pTest->name);
        return result;
    }

    return c89thrd_success;
}
/* END test_c89thrd_detach */

/* BEG test_c89thrd_current */
typedef struct
{
    c89thrd_t thread;
} c89thread_test_c89thrd_current_data;

static int c89thread_test_c89thrd_current__entry(void* pUserData)
{
    c89thread_test_c89thrd_current_data* pData = (c89thread_test_c89thrd_current_data*)pUserData;

    pData->thread = c89thrd_current();  /* Get the current thread. */

    return 0;
}

int c89thread_test_c89thrd_current(c89thread_test* pTest)
{
    c89thread_test_c89thrd_current_data data;
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_current__entry, &data);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        return result;
    }

    if (!c89thrd_equal(data.thread, thread)) {
        printf("%s: c89thrd_current() failed.\n", pTest->name);
        return c89thrd_error;
    }

    return c89thrd_success;
}
/* END test_c89thrd_current */

/* BEG test_c89thrd_exit */
static int c89thread_test_c89thrd_exit__entry(void* pUserData)
{
    (void)pUserData;
    c89thrd_exit(0);
    return 0;
}

int c89thread_test_c89thrd_exit(c89thread_test* pTest)
{
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_exit__entry, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        return result;
    }

    return c89thrd_success;
}
/* END test_c89thrd_exit */

/* BEG test_c89thrd_yield */
static int c89thread_test_c89thrd_yield__entry(void* pUserData)
{
    (void)pUserData;
    c89thrd_yield();
    return 0;
}

int c89thread_test_c89thrd_yield(c89thread_test* pTest)
{
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_yield__entry, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        return result;
    }

    return c89thrd_success;
}
/* END test_c89thrd_yield */

/* BEG test_c89thrd_sleep */
static int c89thread_test_c89thrd_sleep__entry(void* pUserData)
{
    (void)pUserData;
    c89thrd_sleep_milliseconds(10);    /* <-- This calls c89thrd_sleep() internally. */
    return 0;
}

int c89thread_test_c89thrd_sleep(c89thread_test* pTest)
{
    c89thrd_t thread;
    int result;

    result = c89thrd_create(&thread, c89thread_test_c89thrd_sleep__entry, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        return result;
    }

    return c89thrd_success;
}
/* END test_c89thrd_sleep */


/* BEG test_c89mtx */
static int c89thread_test_c89mtx_basic__thread_entry(void* pUserData)
{
    c89mtx_t* pMutex = (c89mtx_t*)pUserData;
    int result;

    result = c89mtx_lock(pMutex);
    if (result != c89thrd_success) {
        printf("Thread failed to lock mutex: %d\n", result);
        return result;
    }

    c89thrd_sleep_milliseconds(10); /* Simulate some work. */

    result = c89mtx_unlock(pMutex);
    if (result != c89thrd_success) {
        printf("Thread failed to unlock mutex: %d\n", result);
        return result;
    }

    return 0;
}

int c89thread_test_c89mtx_basic(c89thread_test* pTest, int type)
{
    c89mtx_t mutex;
    c89thrd_t thread;

    int result;

    result = c89mtx_init(&mutex, type);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_init() failed.\n", pTest->name);
        return result;
    }

    result = c89thrd_create(&thread, c89thread_test_c89mtx_basic__thread_entry, &mutex);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_create() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }

    result = c89mtx_lock(&mutex);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_lock() failed.\n", pTest->name);
        c89thrd_join(thread, NULL);
        c89mtx_destroy(&mutex);
        return result;
    }

    c89thrd_sleep_milliseconds(10); /* Simulate some work. */

    result = c89mtx_unlock(&mutex);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_unlock() failed.\n", pTest->name);
        c89thrd_join(thread, NULL);
        c89mtx_destroy(&mutex);
        return result;
    }

    result = c89thrd_join(thread, NULL);
    if (result != c89thrd_success) {
        printf("%s: c89thrd_join() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }

    c89mtx_destroy(&mutex);
    return c89thrd_success;
}


typedef struct
{
    c89mtx_t* pMutex;
    int result;
} c89thread_test_c89mtx_timed_data;

static int c89thread_test_c89mtx_timed__thread_entry(void* pUserData)
{
    c89thread_test_c89mtx_timed_data* pData = (c89thread_test_c89mtx_timed_data*)pUserData;
    struct timespec timeout;
    int result;

    timeout = c89timespec_add(c89timespec_now(), c89timespec_milliseconds(10));

    /* The calling thread will be holding the lock. We expect this one to fail. */
    result = c89mtx_timedlock(pData->pMutex, &timeout);
    if (result != c89thrd_success) {
        if (result == c89thrd_timedout) {
            pData->result = c89thrd_success;
        } else {
            printf("c89mtx_timedlock() failed with unexpected error: %d\n", result);
            pData->result = result;
            return result;
        }
    } else {
        /* We successfully locked the mutex which should never have happened. This is a failure case. */
        printf("c89mtx_timedlock() succeeded unexpectedly (error case expected).\n");
        pData->result = c89thrd_error;
        c89mtx_unlock(pData->pMutex);
    }

    return 0;
}

int c89thread_test_c89mtx_timed(c89thread_test* pTest, int type)
{
    c89mtx_t mutex;
    struct timespec timeout;
    int result;

    result = c89mtx_init(&mutex, type | c89mtx_timed);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_init() failed.\n", pTest->name);
        return result;
    }

    /* A simple timed lock and unlock with no contention to start with. */
    timeout = c89timespec_add(c89timespec_now(), c89timespec_milliseconds(10));

    result = c89mtx_timedlock(&mutex, &timeout);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_timedlock() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }

    result = c89mtx_unlock(&mutex);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_unlock() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }


    /* Do a test with contention to force a timeout. */
    {
        c89thrd_t thread;
        c89thread_test_c89mtx_timed_data data;

        data.pMutex = &mutex;
        data.result = c89thrd_error;

        /* Must lock the mutex before starting the thread. */
        result = c89mtx_lock(&mutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_lock() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89thrd_create(&thread, c89thread_test_c89mtx_timed__thread_entry, &data);
        if (result != c89thrd_success) {
            printf("%s: c89thrd_create() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89thrd_join(thread, NULL);
        if (result != c89thrd_success) {
            printf("%s: c89thrd_join() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89mtx_unlock(&mutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_unlock() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        if (data.result != c89thrd_success) {
            printf("%s: c89thread_test_c89mtx_timed__thread_entry() failed with result %d.\n", pTest->name, data.result);
            c89mtx_destroy(&mutex);
            return data.result;
        }
    }

    c89mtx_destroy(&mutex);
    return c89thrd_success;
}


typedef struct
{
    c89mtx_t* pMutex;
    int result;
} c89thread_test_c89mtx_trylock_data;

static int c89thread_test_c89mtx_trylock__thread_entry(void* pUserData)
{
    c89thread_test_c89mtx_trylock_data* pData = (c89thread_test_c89mtx_trylock_data*)pUserData;
    int result;

    /* The calling thread will be holding the lock. We expect this one to fail with c89thrd_busy. */
    result = c89mtx_trylock(pData->pMutex);
    if (result != c89thrd_success) {
        if (result == c89thrd_busy) {
            pData->result = c89thrd_success;
        } else {
            printf("c89mtx_trylock() failed with unexpected error: %d\n", result);
            pData->result = result;
            return result;
        }
    } else {
        /* We successfully locked the mutex which should never have happened. This is a failure case. */
        printf("c89mtx_trylock() succeeded unexpectedly (error case expected).\n");
        pData->result = c89thrd_error;
        c89mtx_unlock(pData->pMutex);
    }

    return 0;
}

int c89thread_test_c89mtx_trylock(c89thread_test* pTest, int type)
{
    c89mtx_t mutex;
    int result;

    result = c89mtx_init(&mutex, type);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_init() failed.\n", pTest->name);
        return result;
    }

    /* Initial trylock with no contention. This should always succeed. */
    result = c89mtx_trylock(&mutex);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_trylock() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }

    result = c89mtx_unlock(&mutex);
    if (result != c89thrd_success) {
        printf("%s: c89mtx_unlock() failed.\n", pTest->name);
        c89mtx_destroy(&mutex);
        return result;
    }


    /* TODO: Test trylock with contention to force a failure. */
    {
        c89thrd_t thread;
        c89thread_test_c89mtx_trylock_data data;

        data.pMutex = &mutex;
        data.result = c89thrd_error;

        /* Make sure the mutex is locked before starting the thread. */
        result = c89mtx_lock(&mutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_lock() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89thrd_create(&thread, c89thread_test_c89mtx_trylock__thread_entry, &data);
        if (result != c89thrd_success) {
            printf("%s: c89thrd_create() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89thrd_join(thread, NULL);
        if (result != c89thrd_success) {
            printf("%s: c89thrd_join() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        result = c89mtx_unlock(&mutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_unlock() failed.\n", pTest->name);
            c89mtx_destroy(&mutex);
            return result;
        }

        if (data.result != c89thrd_success) {
            printf("%s: c89thread_test_c89mtx_trylock__thread_entry() failed with result %d.\n", pTest->name, data.result);
            c89mtx_destroy(&mutex);
            return data.result;
        }
    }

    c89mtx_destroy(&mutex);
    return c89thrd_success;
}


/* BEG test_c89mtx_basic_plain */
int c89thread_test_c89mtx_basic_plain(c89thread_test* pTest)
{
    return c89thread_test_c89mtx_basic(pTest, c89mtx_plain);
}
/* END test_c89mtx_basic_plain */

/* BEG test_c89mtx_basic_recursive */
int c89thread_test_c89mtx_basic_recursive(c89thread_test* pTest)
{
    int result;
    
    /* First just do the basic mutex test. */
    result = c89thread_test_c89mtx_basic(pTest, c89mtx_recursive);
    if (result != c89thrd_success) {
        printf("%s: Basic recursive mutex test failed.\n", pTest->name);
        return result;
    }

    /* Now test recursive locking. */
    {
        c89mtx_t recursiveMutex;

        result = c89mtx_init(&recursiveMutex, c89mtx_recursive);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_init() failed.\n", pTest->name);
            return result;
        }

        result = c89mtx_lock(&recursiveMutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_lock() failed.\n", pTest->name);
            c89mtx_destroy(&recursiveMutex);
            return result;
        }

        result = c89mtx_lock(&recursiveMutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_lock() failed.\n", pTest->name);
            c89mtx_unlock(&recursiveMutex);
            c89mtx_destroy(&recursiveMutex);
            return result;
        }

        result = c89mtx_unlock(&recursiveMutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_unlock() failed.\n", pTest->name);
            c89mtx_destroy(&recursiveMutex);
            return result;
        }

        result = c89mtx_unlock(&recursiveMutex);
        if (result != c89thrd_success) {
            printf("%s: c89mtx_unlock() failed.\n", pTest->name);
            c89mtx_destroy(&recursiveMutex);
            return result;
        }

        c89mtx_destroy(&recursiveMutex);
    }

    return c89thrd_success;
}
/* END test_c89mtx_basic_recursive */

/* BEG test_c89mtx_timed_plain */
int c89thread_test_c89mtx_timed_plain(c89thread_test* pTest)
{
    return c89thread_test_c89mtx_timed(pTest, c89mtx_plain);
}
/* END test_c89mtx_timed_plain */

/* BEG test_c89mtx_timed_recursive */
int c89thread_test_c89mtx_timed_recursive(c89thread_test* pTest)
{
    return c89thread_test_c89mtx_timed(pTest, c89mtx_recursive);
}
/* END test_c89mtx_timed_recursive */

/* BEG test_c89mtx_trylock_plain */
int c89thread_test_c89mtx_trylock_plain(c89thread_test* pTest)
{
    return c89thread_test_c89mtx_trylock(pTest, c89mtx_plain);
}
/* END test_c89mtx_trylock_plain */

/* BEG test_c89mtx_trylock_recursive */
int c89thread_test_c89mtx_trylock_recursive(c89thread_test* pTest)
{
    return c89thread_test_c89mtx_trylock(pTest, c89mtx_recursive);
}
/* END test_c89mtx_trylock_recursive */

/* END test_c89mtx */


int main(int argc, char** argv)
{
    c89thread_test test_root;
    c89thread_test test_c89thrd;
    c89thread_test test_c89thrd_create;
    c89thread_test test_c89thrd_join;
    c89thread_test test_c89thrd_detach;
    c89thread_test test_c89thrd_current;
    c89thread_test test_c89thrd_exit;
    c89thread_test test_c89thrd_yield;
    c89thread_test test_c89thrd_sleep;
    c89thread_test test_c89mtx;
    c89thread_test test_c89mtx_basic;
    c89thread_test test_c89mtx_basic_plain;
    c89thread_test test_c89mtx_basic_recursive;
    c89thread_test test_c89mtx_timed;
    c89thread_test test_c89mtx_timed_plain;
    c89thread_test test_c89mtx_timed_recursive;
    c89thread_test test_c89mtx_trylock;
    c89thread_test test_c89mtx_trylock_plain;
    c89thread_test test_c89mtx_trylock_recursive;
    c89thread_test test_c89cnd;
    c89thread_test test_c89sem;
    c89thread_test test_c89evnt;
    int result;

    (void)argc;
    (void)argv;

    /* Root. Only used for execution. */
    c89thread_test_init(&test_root, NULL, NULL, NULL, NULL);

    /* Thread. */
    c89thread_test_init(&test_c89thrd,                  "c89thrd",                  NULL,                                    NULL, &test_root);
    c89thread_test_init(&test_c89thrd_create,           "c89thrd_create",           c89thread_test_c89thrd_create,           NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_join,             "c89thrd_join",             c89thread_test_c89thrd_join,             NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_detach,           "c89thrd_detach",           c89thread_test_c89thrd_detach,           NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_current,          "c89thrd_current",          c89thread_test_c89thrd_current,          NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_exit,             "c89thrd_exit",             c89thread_test_c89thrd_exit,             NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_yield,            "c89thrd_yield",            c89thread_test_c89thrd_yield,            NULL, &test_c89thrd);
    c89thread_test_init(&test_c89thrd_sleep,            "c89thrd_sleep",            c89thread_test_c89thrd_sleep,            NULL, &test_c89thrd);

    /* Mutex. */
    c89thread_test_init(&test_c89mtx,                   "c89mtx",                   NULL,                                    NULL, &test_root);
    c89thread_test_init(&test_c89mtx_basic,             "c89mtx_basic",             NULL,                                    NULL, &test_c89mtx);
    c89thread_test_init(&test_c89mtx_basic_plain,       "c89mtx_basic_plain",       c89thread_test_c89mtx_basic_plain,       NULL, &test_c89mtx_basic);
    c89thread_test_init(&test_c89mtx_basic_recursive,   "c89mtx_basic_recursive",   c89thread_test_c89mtx_basic_recursive,   NULL, &test_c89mtx_basic);
    c89thread_test_init(&test_c89mtx_timed,             "c89mtx_timed",             NULL,                                    NULL, &test_c89mtx);
    c89thread_test_init(&test_c89mtx_timed_plain,       "c89mtx_timed_plain",       c89thread_test_c89mtx_timed_plain,       NULL, &test_c89mtx_timed);
    c89thread_test_init(&test_c89mtx_timed_recursive,   "c89mtx_timed_recursive",   c89thread_test_c89mtx_timed_recursive,   NULL, &test_c89mtx_timed);
    c89thread_test_init(&test_c89mtx_trylock,           "c89mtx_trylock",           NULL,                                    NULL, &test_c89mtx);
    c89thread_test_init(&test_c89mtx_trylock_plain,     "c89mtx_trylock_plain",     c89thread_test_c89mtx_trylock_plain,     NULL, &test_c89mtx_trylock);
    c89thread_test_init(&test_c89mtx_trylock_recursive, "c89mtx_trylock_recursive", c89thread_test_c89mtx_trylock_recursive, NULL, &test_c89mtx_trylock);

    /* Condition Variable. */
    c89thread_test_init(&test_c89cnd,                   "c89cnd",                   NULL,                                    NULL, &test_root);

    /* Semaphore. */
    c89thread_test_init(&test_c89sem,                   "c89sem",                   NULL,                                    NULL, &test_root);

    /* Event. */
    c89thread_test_init(&test_c89evnt,                   "c89evnt",                 NULL,                                    NULL, &test_root);

    result = c89thread_test_run(&test_root);

    /* Print the test summary. */
    printf("\n");
    c89thread_test_print_summary(&test_root);

    if (result == c89thrd_success) {
        return 0;
    } else {
        return 1;
    }
}