
file(GLOB SRC_G "cases/*.cpp")
add_executable(test_main ${SRC_G} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_include_directories(test_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/catch)
target_link_libraries(test_main PUBLIC hsm::hsm hsm_compile_dependency)
add_test(NAME AllTests COMMAND test_main)
