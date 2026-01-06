add_executable(test_basic test_basic.c)
add_test(NAME test_basic COMMAND test_basic)
target_link_libraries(test_basic PRIVATE smf::smf cunit::cunit)

add_executable(test_flat test_flat.c)
add_test(NAME test_flat COMMAND test_flat)
target_link_libraries(test_flat PRIVATE smf::smf cunit::cunit)

add_executable(test_5_ancestor test_5_ancestor.c)
add_test(NAME test_5_ancestor COMMAND test_5_ancestor)
target_link_libraries(test_5_ancestor PRIVATE smf::smf cunit::cunit)

add_executable(test_hierarchical test_hierarchical.c)
add_test(NAME test_hierarchical COMMAND test_hierarchical)
target_link_libraries(test_hierarchical PRIVATE smf::smf cunit::cunit)

add_executable(test_self_transition test_self_transition.c)
add_test(NAME test_self_transition COMMAND test_self_transition)
target_link_libraries(test_self_transition PRIVATE smf::smf cunit::cunit)
