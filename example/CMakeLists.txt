add_executable(example_toggle toggle/main.cpp)
target_link_libraries(example_toggle PRIVATE hsm::hsm hsm_compile_dependency)

add_executable(example_door door/main.cpp)
target_link_libraries(example_door PRIVATE hsm::hsm hsm_compile_dependency)

add_executable(example_dist_agent dist_agent/main.cpp)
target_link_libraries(example_dist_agent PRIVATE hsm::hsm hsm_compile_dependency)
