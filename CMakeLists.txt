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

if (CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(hack PRIVATE -Wall -Wextra)
endif()

# configure target properties
set_target_properties(hack
    PROPERTIES ARCHIVE_OUTPUT_NAME hack
)

# configure dll exporting
add_definitions(-DDLL_EXPORT -DDEBUG)

if(MSVC OR MINGW)
    TARGET_LINK_LIBRARIES(hack psapi)
    TARGET_LINK_LIBRARIES(hack shlwapi)
endif()

if(UNIX)
    TARGET_LINK_LIBRARIES(hack procps)
endif()

# setup flags by compiler
if(MINGW)
    add_definitions(-DMINGW)
	
	# configure for 32 bit building
    #set(CMAKE_C_FLAGS "-m32")
elseif(MSVC)
    add_definitions(-DVISUAL_STUDIO)
endif()
