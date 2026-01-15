cmake_minimum_required(VERSION 3.14)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/PreventInSourceBuilds.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/GitVersion.cmake)
git_version_info(VERSION SMF_VERSION DEFAULT_VERSION 0.1.0)
project(smf VERSION ${SMF_VERSION})

include(${CMAKE_CURRENT_LIST_DIR}/cmake/ProjectConfig.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake)

set(SMF_LIB_TYPE STATIC)
if(BUILD_SHARED_LIBS)
  set(SMF_LIB_TYPE SHARED)
endif()
message(STATUS "smf v${SMF_VERSION} ${SMF_LIB_TYPE} library")

add_library(smf ${SMF_LIB_TYPE} src/smf.c)
add_library(smf::smf ALIAS smf)
target_link_libraries(smf PRIVATE smf_compile_dependency)
target_include_directories(smf PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
if(BUILD_SHARED_LIBS)
  target_compile_definitions(smf PRIVATE SMF_LIB_EXPORT INTERFACE SMF_SHARED)
endif()

if(SMF_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

if(SMF_BUILD_TEST)
  CPMAddPackage(
    NAME cunit
    URL "https://codeload.github.com/tayne3/cunit/tar.gz/refs/tags/v0.2.5"
    URL_HASH "SHA256=73429bf8bc733ce5431b012689e56f9d255bab3d6d1ac745311f81514b322186"
    OPTIONS "BUILD_SHARED_LIBS OFF"
  )
  enable_testing()
  add_subdirectory(test)
endif()
