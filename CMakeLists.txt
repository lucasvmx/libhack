cmake_minimum_required(VERSION 3.10)

# set the project name
project(libhack)

# add source files to build shared library
add_library(hack SHARED 
    src/main.c 
    src/process.c 
    src/init.c
    src/process.h
    src/consts.h
    src/init.h
	src/logger.c
	src/logger.h
    src/types.c
    src/types.h
)

add_executable(unit_test
    src/unit_test.c
    src/main.c 
    src/process.c 
    src/init.c
    src/process.h
	src/logger.c
    src/types.c
)

set(CMAKE_C_STANDARD 17)

if (CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(hack PRIVATE -Wall -Wextra)
    target_compile_options(unit_test PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(unit_test gcov)
endif()

# configure target properties
set_target_properties(hack
    PROPERTIES ARCHIVE_OUTPUT_NAME hack
)

# configure dll exporting
add_definitions(-DDLL_EXPORT -DDEBUG)

if(WIN32)
    message(STATUS "generating makefile for windows target")
    target_link_libraries(hack psapi shlwapi)
    target_link_libraries(unit_test psapi shlwapi)
elseif(UNIX)
    message(STATUS "generating makefile for unix target")
    target_link_libraries(hack procps)
    target_link_libraries(unit_test procps)
endif()


