add_executable(tests catch_amalgamated.cpp tests.cpp)

target_compile_definitions(tests 
                           PUBLIC SANDMAN_TEST_DATA_DIR="${CMAKE_BINARY_DIR}/data/"
                           PUBLIC SANDMAN_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}/")

target_link_libraries(tests PUBLIC sandman_compiler_flags sandman_lib)
