add_executable(example_basic basic/main.cpp)
target_link_libraries(example_basic PRIVATE hsm::hsm hsm_compile_dependency)

add_executable(example_minimum minimum/main.cpp)
target_link_libraries(example_minimum PRIVATE hsm::hsm hsm_compile_dependency)
