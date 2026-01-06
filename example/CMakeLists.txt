add_executable(basic_example 01_basic/main.c)
target_link_libraries(basic_example PRIVATE smf::smf)

add_executable(device_example 02_device/main.c 02_device/device_sm.c)
target_link_libraries(device_example PRIVATE smf::smf)
