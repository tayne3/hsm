
add_library(test_main OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_include_directories(test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/catch)
target_link_libraries(test_main PUBLIC hsm::hsm hsm_compile_dependency)

add_executable(test_basic cases/test_basic.cpp)
target_link_libraries(test_basic PRIVATE test_main)
add_test(NAME test_basic COMMAND test_basic)

add_executable(test_hierarchical cases/test_hierarchical.cpp)
target_link_libraries(test_hierarchical PRIVATE test_main)
add_test(NAME test_hierarchical COMMAND test_hierarchical)

add_executable(test_ancestor cases/test_ancestor.cpp)
target_link_libraries(test_ancestor PRIVATE test_main)
add_test(NAME test_ancestor COMMAND test_ancestor)

add_executable(test_self_transition cases/test_self_transition.cpp)
target_link_libraries(test_self_transition PRIVATE test_main)
add_test(NAME test_self_transition COMMAND test_self_transition)

add_executable(test_start_with_transition cases/test_start_with_transition.cpp)
target_link_libraries(test_start_with_transition PRIVATE test_main)
add_test(NAME test_start_with_transition COMMAND test_start_with_transition)

add_executable(test_matcher cases/test_matcher.cpp)
target_link_libraries(test_matcher PRIVATE test_main)
add_test(NAME test_matcher COMMAND test_matcher)

add_executable(test_scope_proxy cases/test_scope_proxy.cpp)
target_link_libraries(test_scope_proxy PRIVATE test_main)
add_test(NAME test_scope_proxy COMMAND test_scope_proxy)

add_executable(test_dispatch cases/test_dispatch.cpp)
target_link_libraries(test_dispatch PRIVATE test_main)
add_test(NAME test_dispatch COMMAND test_dispatch)

add_executable(test_fast_policy cases/test_fast_policy.cpp)
target_link_libraries(test_fast_policy PRIVATE test_main)
add_test(NAME test_fast_policy COMMAND test_fast_policy)

add_executable(test_dynamic_policy cases/test_dynamic_policy.cpp)
target_link_libraries(test_dynamic_policy PRIVATE test_main)
add_test(NAME test_dynamic_policy COMMAND test_dynamic_policy)

add_executable(test_tag_policy cases/test_tag_policy.cpp)
target_link_libraries(test_tag_policy PRIVATE test_main)
add_test(NAME test_tag_policy COMMAND test_tag_policy)

add_executable(test_reentrancy cases/test_reentrancy.cpp)
target_link_libraries(test_reentrancy PRIVATE test_main)
add_test(NAME test_reentrancy COMMAND test_reentrancy)

add_executable(test_boundary cases/test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE test_main)
add_test(NAME test_boundary COMMAND test_boundary)
