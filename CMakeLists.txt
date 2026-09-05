cmake_minimum_required(VERSION 3.15)

project(libhack VERSION 0.4.2 LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

option(LIBHACK_BUILD_EXAMPLES "Build the platform-specific examples" OFF)
option(LIBHACK_ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)
option(LIBHACK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(LIBHACK_BUILD_EXAMPLES AND WIN32)
    enable_language(CXX)
endif()

set(LIBHACK_SOURCES
    src/init.c
    src/logger.c
    src/process.c
    src/status_codes.c
    src/types.c
)

set(LIBHACK_HEADERS
    src/consts.h
    src/init.h
    src/logger.h
    src/mingw_aliases.h
    src/platform.h
    src/process.h
    src/status_codes.h
    src/types.h
    autorevision.h
)

add_library(hack SHARED ${LIBHACK_SOURCES} ${LIBHACK_HEADERS})
add_library(libhack::hack ALIAS hack)

target_compile_features(hack PUBLIC c_std_17)
set_target_properties(hack PROPERTIES
    C_EXTENSIONS OFF
    C_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME hack
)

target_include_directories(hack
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libhack>
)

target_compile_definitions(hack PRIVATE DLL_EXPORT)

if(MSVC)
    target_compile_options(hack PRIVATE /W4)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hack PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LIBHACK_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(hack PRIVATE /WX)
    else()
        target_compile_options(hack PRIVATE -Werror)
    endif()
endif()

if(WIN32)
    message(STATUS "Configuring libhack for Windows")
    target_link_libraries(hack PRIVATE psapi shlwapi)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring libhack for Linux")

    find_package(Threads REQUIRED)
    target_link_libraries(hack PRIVATE Threads::Threads)
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

if(LIBHACK_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(hack PRIVATE --coverage)
        target_link_options(hack PRIVATE --coverage)
    else()
        message(FATAL_ERROR "LIBHACK_ENABLE_COVERAGE requires GCC or Clang")
    endif()
endif()

if(BUILD_TESTING)
    add_executable(unit_test src/unit_test.c)
    target_compile_features(unit_test PRIVATE c_std_17)
    set_target_properties(unit_test PROPERTIES
        C_EXTENSIONS OFF
        C_STANDARD_REQUIRED ON
    )
    target_link_libraries(unit_test PRIVATE libhack::hack)
    target_include_directories(unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    if(LIBHACK_ENABLE_COVERAGE)
        target_compile_options(unit_test PRIVATE --coverage)
        target_link_options(unit_test PRIVATE --coverage)
    endif()

    add_test(NAME libhack.unit_test COMMAND unit_test)
endif()

if(LIBHACK_BUILD_EXAMPLES)
    add_subdirectory(src/examples)
endif()

install(TARGETS hack
    EXPORT libhackTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libhack
)

install(FILES ${LIBHACK_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libhack
)

install(EXPORT libhackTargets
    FILE libhackTargets.cmake
    NAMESPACE libhack::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libhack
)
