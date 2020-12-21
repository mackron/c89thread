<h4 align="center">C89 compatible threads.</h4>

<p align="center">
    <a href="https://discord.gg/9vpqbjU"><img src="https://img.shields.io/discord/712952679415939085?label=discord&logo=discord" alt="discord"></a>
    <a href="https://twitter.com/mackron"><img src="https://img.shields.io/twitter/follow/mackron?style=flat&label=twitter&color=1da1f2&logo=twitter" alt="twitter"></a>
</p>

This library aims to implement an equivalent to the C11 threading library. Not everything is implemented:

  * Condition variables are not supported on the Win32 build. If your compiler supports pthread, you
    can use that instead by putting `#define C89THREAD_USE_PTHREAD` before including c89thread.h.
  * Thread-specific storage (TSS/TLS) is not yet implemented.

The API should be compatible with the main C11 API, but all APIs have been namespaced with `c89`:

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
from any thread so long as you do your own synchronization. Threads can be created with an extended
function called `c89thrd_create_ex()` which takes a pointer to a structure containing custom allocation
callbacks which will be used instead of the global callbacks if specified. This function is specific to
c89thread and is not usable if you require strict C11 compatibility.

This is still work-in-progress and not much testing has been done. Use at your own risk.


Building
========
c89thread is a single file library. To use it, do something like the following in one .c file.

```c
#define C89THREAD_IMPLEMENTATION
#include "c89thread.h"
```

You can then #include this file in other parts of the program as you would with any other header file.

When compiling for Win32 it should work out of the box without needing to link to anything. If you're
using pthreads, you may need to link with `-lpthread`.
