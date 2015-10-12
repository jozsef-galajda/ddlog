cmake_minimum_required(VERSION 2.8)
project (hello_cmake)
#set(CMAKE_C_COMPILER g++)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}  -Wall -Werror -pedantic -Wno-variadic-macros")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}  -Wall -Werror -pedantic -Wno-variadic-macros")
add_executable(ddlog_test ddlog.c ddlog_test.c ddlog_server.c ddlog_display.c
        ddlog_display_debug.c ddlog_ext.c ddlog_ext_utils.c ddlog_debug.c)

add_library(ddlog SHARED ddlog.c ddlog_server.c ddlog_display.c ddlog_ext.c ddlog_ext_utils.c)
find_package (Threads)
include_directories(include)
target_link_libraries(ddlog_test ${CMAKE_THREAD_LIBS_INIT})
