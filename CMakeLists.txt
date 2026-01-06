cmake_minimum_required(VERSION 3.14)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreventInSourceBuilds.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GitVersion.cmake)
git_version_info(VERSION SMF_VERSION DEFAULT_VERSION 0.1.0)
project(smf VERSION ${SMF_VERSION})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectIsTopLevel.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/OptionVariables.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProjectConfig.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

if(SMF_BUILD_SHARED)
  add_library(smf_shared SHARED)
  set(SMF smf_shared)
  message(STATUS "smf v${SMF_VERSION} shared library")
else()
  add_library(smf_static STATIC)
  set(SMF smf_static)
  message(STATUS "smf v${SMF_VERSION} static library")
endif()
target_sources(${SMF} PRIVATE 
  src/smf.c
)
add_library(smf::smf ALIAS ${SMF})

target_link_libraries(${SMF} PRIVATE 
  smf_compile_dependency
)
target_include_directories(${SMF} PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(SMF_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

if(SMF_BUILD_TEST)
  CPMAddPackage(
    NAME cunit
    URL "https://codeload.github.com/tayne3/cunit/tar.gz/refs/tags/v0.2.3"
  )
  enable_testing()
  add_subdirectory(test)
endif()
