add_executable(tests catch_amalgamated.cpp tests.cpp test_common_string.cpp)

target_link_libraries(tests PUBLIC sandman_compiler_flags sandman_lib)
