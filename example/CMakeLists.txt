add_executable(basic_example basic_example/main.cpp)
target_link_libraries(basic_example PRIVATE hsm::hsm hsm_compile_dependency)
