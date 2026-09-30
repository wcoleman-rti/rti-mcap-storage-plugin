#
# (c) 2019 Copyright, Real-Time Innovations, Inc.  All rights reserved.
#
#  RTI grants Licensee a license to use, modify, compile, and create derivative
#  works of the Software.  Licensee has the right to distribute object form
#  only for use with RTI products.  The Software is provided "as is", with no
#  warranty of any type, including any warranty for fitness for any purpose.
#  RTI is under no obligation to maintain or support the Software.  RTI shall
#  not be liable for any incidental or consequential damages arising out of the
#  use or inability to use the software.
#
cmake_minimum_required(VERSION 3.20)
project(RTIMcap VERSION 1.0.0 LANGUAGES CXX)

set(RTI_MCAP_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(RTI_MCAP_TOP_LEVEL ON)
  include(CTest)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()
endif()

option(RTI_MCAP_BUILD_CORE "Build DDS/MCAP conversion and metadata" ON)
option(RTI_MCAP_BUILD_FILE "Build MCAP file storage" ON)
option(RTI_MCAP_BUILD_STORAGE "Build Recording/Replay storage plugins" ${RTI_MCAP_TOP_LEVEL})
set(_test_default OFF)
if(RTI_MCAP_TOP_LEVEL AND BUILD_TESTING)
  set(_test_default ON)
endif()
option(RTI_MCAP_BUILD_TESTS "Build project-owned tests" ${_test_default})
option(RTI_MCAP_BUILD_BENCHMARKS "Build the opt-in project benchmark" OFF)
option(RTI_MCAP_ENABLE_SANITIZERS "Instrument project targets with ASan and UBSan" OFF)
option(RTI_MCAP_ENABLE_LZ4 "Enable MCAP LZ4 compression" OFF)
option(RTI_MCAP_ENABLE_ZSTD "Enable MCAP Zstd compression" OFF)

if(RTI_MCAP_BUILD_STORAGE AND (NOT RTI_MCAP_BUILD_CORE OR NOT RTI_MCAP_BUILD_FILE))
  message(FATAL_ERROR "RTI_MCAP_BUILD_STORAGE requires RTI_MCAP_BUILD_CORE and RTI_MCAP_BUILD_FILE")
endif()
if(RTI_MCAP_BUILD_BENCHMARKS AND NOT RTI_MCAP_BUILD_STORAGE)
  message(FATAL_ERROR "RTI_MCAP_BUILD_BENCHMARKS requires RTI_MCAP_BUILD_STORAGE")
endif()
if(NOT RTI_MCAP_BUILD_CORE AND NOT RTI_MCAP_BUILD_FILE)
  message(FATAL_ERROR "Enable at least one RTIMcap component")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/cmake")
include(RTIMcapTargetOptions)
include(RTIMcapConnext)
if(RTI_MCAP_BUILD_CORE)
  rti_mcap_find_connext(${RTI_MCAP_BUILD_STORAGE})
endif()

add_subdirectory(resources/dependencies/mcap)
if(RTI_MCAP_BUILD_CORE)
  add_subdirectory(sdk/core)
endif()
if(RTI_MCAP_BUILD_FILE)
  add_subdirectory(sdk/file)
endif()
if(RTI_MCAP_BUILD_STORAGE)
  add_subdirectory(plugins/storage)
endif()
if(RTI_MCAP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(resources/dependencies/doctest)
endif()
if(RTI_MCAP_BUILD_TESTS OR RTI_MCAP_BUILD_BENCHMARKS)
  add_subdirectory(tests)
endif()
if(RTI_MCAP_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

set(RTI_MCAP_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/RTIMcap")
configure_package_config_file(resources/cmake/RTIMcapConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/RTIMcapConfig.cmake"
  INSTALL_DESTINATION "${RTI_MCAP_PACKAGE_DIR}"
)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/RTIMcapConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/RTIMcapConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/RTIMcapConfigVersion.cmake"
  resources/cmake/RTIMcapConnext.cmake
  resources/cmake/RTIMcapCompression.cmake
  DESTINATION "${RTI_MCAP_PACKAGE_DIR}"
)
install(EXPORT RTIMcapDependenciesTargets
  NAMESPACE rti::mcap:: DESTINATION "${RTI_MCAP_PACKAGE_DIR}")
foreach(component IN ITEMS core file storage)
  string(TOUPPER "${component}" upper)
  if(RTI_MCAP_BUILD_${upper})
    install(EXPORT RTIMcap${component}Targets
      NAMESPACE rti::mcap:: DESTINATION "${RTI_MCAP_PACKAGE_DIR}")
  endif()
endforeach()