add_executable(write_addr write_addr.c)

target_compile_features(write_addr PRIVATE c_std_17)
target_include_directories(write_addr PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(write_addr PRIVATE libhack::hack)
