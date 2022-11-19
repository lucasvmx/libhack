cmake_minimum_required(VERSION 3.10)

project(linux)

# Executable settings
add_executable(write_addr write_addr.c)

add_definitions(-DDEBUG)

# Add link directories
link_directories(${CMAKE_SOURCE_DIR})

# Add link libraries
find_library(LIBHACK hack ${CMAKE_SOURCE_DIR})
target_link_libraries(write_addr ${LIBHACK})

# Set language standard
set_property(TARGET write_addr PROPERTY C_STANDARD 17)
