add_executable(pid pid.c)
add_executable(read_addr read_addr.c)
add_executable(write_addr write_addr.c)
add_executable(dll_inject dll_inject.c)
add_executable(version_info version_info.cpp)
add_executable(submodule_addr get_submodule_addr.c)
add_library(hello SHARED hello.c)

set(LIBHACK_WINDOWS_EXAMPLES
    pid
    read_addr
    write_addr
    dll_inject
    version_info
    submodule_addr
)

foreach(example IN LISTS LIBHACK_WINDOWS_EXAMPLES)
    target_include_directories(${example} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${example} PRIVATE libhack::hack)
endforeach()

target_compile_features(pid PRIVATE c_std_11)
target_compile_features(read_addr PRIVATE c_std_11)
target_compile_features(write_addr PRIVATE c_std_11)
target_compile_features(dll_inject PRIVATE c_std_11)
target_compile_features(submodule_addr PRIVATE c_std_11)
target_compile_features(version_info PRIVATE cxx_std_11)

target_link_libraries(hello PRIVATE user32)
