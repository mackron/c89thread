cmake_minimum_required(VERSION 3.14)
project(c89thread VERSION 1.0)

# Options
option(C89THREAD_BUILD_EXAMPLES             "Build c89thread examples"                OFF)
option(C89THREAD_BUILD_TESTS                "Build c89thread tests"                   OFF)
option(C89THREAD_BUILD_TOOLS                "Build c89thread tools"                   OFF)
option(C89THREAD_FORCE_CXX                  "Force compilation as C++"                OFF)
option(C89THREAD_FORCE_C89                  "Force compilation as C89"                OFF)
option(C89THREAD_USE_MANUAL_RECURSIVE_MUTEX "Force the use of manual recursive mutex" OFF)

# Construct compiler options.
set(COMPILE_OPTIONS)

if(C89THREAD_FORCE_CXX AND C89THREAD_FORCE_C89)
    message(FATAL_ERROR "C89THREAD_FORCE_CXX and C89THREAD_FORCE_C89 cannot be enabled at the same time.")
endif()

if(C89THREAD_FORCE_CXX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C++ (GNU/Clang)")
        list(APPEND COMPILE_OPTIONS -x c++)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Compiling as C++ (MSVC)")
        list(APPEND COMPILE_OPTIONS /TP)
    else()
        message(WARNING "C89THREAD_FORCE_CXX is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

if(C89THREAD_FORCE_C89)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C89")
        list(APPEND COMPILE_OPTIONS -std=c89)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(WARNING "MSVC does not support forcing C89. C89THREAD_FORCE_C89 ignored.")
    else()
        message(WARNING "C89THREAD_FORCE_C89 is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    #list(APPEND COMPILE_OPTIONS /W4)
endif()


# Construct compiler defines
set(COMPILE_DEFINES)

if(C89THREAD_USE_MANUAL_RECURSIVE_MUTEX)
    list(APPEND COMPILE_DEFINES C89THREAD_USE_MANUAL_RECURSIVE_MUTEX)
endif()

# Link libraries
set(COMMON_LIBRARIES)

if (UNIX)
    find_library(LIB_PTHREAD NAMES pthread)
    if(LIB_PTHREAD)
        list(APPEND COMMON_LIBRARIES ${LIB_PTHREAD}) # Some compilers will not link to pthread by default so list it here just in case.
    endif()
endif()


# Common interface
add_library(c89thread_common INTERFACE)
target_compile_options    (c89thread_common INTERFACE ${COMPILE_OPTIONS})
target_link_libraries     (c89thread_common INTERFACE ${COMMON_LIBRARIES})
target_compile_definitions(c89thread_common INTERFACE ${COMPILE_DEFINES})


# Main library
add_library(c89thread STATIC
    c89thread.c
    c89thread.h
)

target_include_directories(c89thread PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries     (c89thread PRIVATE c89thread_common)


# Tests
if(C89THREAD_BUILD_TESTS)
    enable_testing()

    add_executable(c89thread_test tests/c89thread_test.c)
    target_link_libraries(c89thread_test PRIVATE c89thread)
    add_test(NAME c89thread_test COMMAND c89thread_test)

    add_executable(c89thread_basic tests/c89thread_basic.c)
    target_link_libraries(c89thread_basic PRIVATE c89thread)
    add_test(NAME c89thread_basic COMMAND c89thread_basic)

    # sandbox. Don't add a test for this.
    #add_executable(c89thread_sandbox tests/c89thread_sandbox.c)
    #target_link_libraries(c89thread_sandbox PRIVATE c89thread)
endif()
