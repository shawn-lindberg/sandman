add_executable(tests catch_amalgamated.cpp tests.cpp)

target_link_libraries(tests PUBLIC sandman_compiler_flags sandman_lib)
