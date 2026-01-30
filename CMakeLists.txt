cmake_minimum_required(VERSION 3.14)

# In-source build guard
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
    "In-source builds are not supported. "
    "You may need to delete 'CMakeCache.txt' and 'CMakeFiles/' before rebuilding this project."
  )
endif()

include(${CMAKE_CURRENT_LIST_DIR}/cmake/GitVersion.cmake)
git_version_info(VERSION HSM_VERSION DEFAULT_VERSION 0.1.0)
project(hsm VERSION ${HSM_VERSION})

include(${CMAKE_CURRENT_LIST_DIR}/cmake/ProjectConfig.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake)

add_library(hsm INTERFACE)
add_library(hsm::hsm ALIAS hsm)
target_include_directories(hsm INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(HSM_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

if(HSM_BUILD_TEST)
  enable_testing()
  add_subdirectory(test)
endif()
