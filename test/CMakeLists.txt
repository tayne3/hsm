set(MAIN_CPP ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_library(Catch2 INTERFACE)
target_include_directories(Catch2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_basic cases/test_basic.cpp ${MAIN_CPP})
target_link_libraries(test_basic PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_basic COMMAND test_basic)

add_executable(test_hierarchical cases/test_hierarchical.cpp ${MAIN_CPP})
target_link_libraries(test_hierarchical PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_hierarchical COMMAND test_hierarchical)

add_executable(test_ancestor cases/test_ancestor.cpp ${MAIN_CPP})
target_link_libraries(test_ancestor PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_ancestor COMMAND test_ancestor)

add_executable(test_self_transition cases/test_self_transition.cpp ${MAIN_CPP})
target_link_libraries(test_self_transition PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_self_transition COMMAND test_self_transition)

add_executable(test_start_with_transition cases/test_start_with_transition.cpp ${MAIN_CPP})
target_link_libraries(test_start_with_transition PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_start_with_transition COMMAND test_start_with_transition)

add_executable(test_matcher cases/test_matcher.cpp ${MAIN_CPP})
target_link_libraries(test_matcher PRIVATE Catch2 hsm::hsm hsm_compile_dependency)
add_test(NAME test_matcher COMMAND test_matcher)
