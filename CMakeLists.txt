cmake_minimum_required(VERSION 3.14)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
    FATAL_ERROR
    "In-source builds are not supported. "
    "You may need to delete 'CMakeCache.txt' and 'CMakeFiles/' before rebuilding this project."
  )
endif()

project(hsm VERSION 0.1.4)
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(BUILD_SHARED_LIBS)
  set(HSM_LIB_TYPE SHARED)
else()
  set(HSM_LIB_TYPE STATIC)
endif()
message(STATUS "hsm v${PROJECT_VERSION} ${HSM_LIB_TYPE} library")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  option(HSM_BUILD_EXAMPLE "build example program" OFF)
  option(HSM_BUILD_TEST "build test program" OFF)

  get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(NOT isMultiConfig
     AND NOT CMAKE_BUILD_TYPE
     AND NOT CMAKE_CONFIGURATION_TYPES
  )
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
  endif()
    
  if(NOT DEFINED CMAKE_C_VISIBILITY_PRESET)
    set(CMAKE_C_VISIBILITY_PRESET hidden CACHE STRING "Preset for the export of private symbols")
    set_property(CACHE CMAKE_C_VISIBILITY_PRESET PROPERTY STRINGS hidden default)
  endif()

  if(NOT DEFINED CMAKE_CXX_VISIBILITY_PRESET)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden CACHE STRING "Preset for the export of private symbols")
    set_property(CACHE CMAKE_CXX_VISIBILITY_PRESET PROPERTY STRINGS hidden default)
  endif()

  if(NOT DEFINED CMAKE_VISIBILITY_INLINES_HIDDEN)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON CACHE BOOL "Whether to add a compile flag to hide symbols of inline functions")
  endif()
  
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  
  include(GNUInstallDirs)
  set(CMAKE_SKIP_BUILD_RPATH OFF)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)
  if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/${CMAKE_INSTALL_LIBDIR}")
  else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/${CMAKE_INSTALL_LIBDIR}")
  endif()
endif()

add_library(hsm_compile_dependency INTERFACE)
target_compile_features(hsm_compile_dependency INTERFACE c_std_99)
if(UNIX)
  target_link_libraries(hsm_compile_dependency INTERFACE pthread)
endif()

# set source charset to utf-8 for MSVC
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(hsm_compile_dependency INTERFACE 
		"$<$<COMPILE_LANGUAGE:C>:/utf-8>"
	)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(hsm_compile_dependency INTERFACE 
		"$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
		"$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>"
	)
endif()

# compiler warnings, skipped for MSVC & ClangCL (MSVC frontend)
if(NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" AND "${CMAKE_C_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")))
  target_compile_options(hsm_compile_dependency INTERFACE
    "$<$<COMPILE_LANGUAGE:C>:-Wall>"
    "$<$<COMPILE_LANGUAGE:C>:-Wextra>"
  )
  check_c_compiler_flag("-Werror=return-type" HAVE_C_WERROR_RETURN_TYPE)
  if(HAVE_C_WERROR_RETURN_TYPE)
    target_compile_options(hsm_compile_dependency INTERFACE
      "$<$<COMPILE_LANGUAGE:C>:-Werror=return-type>"
    )
  endif()
endif()
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")))
  target_compile_options(hsm_compile_dependency INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
  )
  check_cxx_compiler_flag("-Werror=return-type" HAVE_CXX_WERROR_RETURN_TYPE)
  if(HAVE_CXX_WERROR_RETURN_TYPE)
    target_compile_options(hsm_compile_dependency INTERFACE
      "$<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>"
    )
  endif()
endif()

# Handle -Wno-missing-field-initializers for older GCC
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 6.0)
  target_compile_options(hsm_compile_dependency INTERFACE 
		"$<$<COMPILE_LANGUAGE:C>:-Wno-missing-field-initializers>"
	)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
  target_compile_options(hsm_compile_dependency INTERFACE 
		"$<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>"
	)
endif()

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
