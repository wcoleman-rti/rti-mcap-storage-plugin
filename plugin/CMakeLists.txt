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
cmake_minimum_required(VERSION 3.11)
project(McapStoragePlugin)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    set(msg "Defaulting CMAKE_BUILD_TYPE to Release")
    message(WARNING ${msg})
  endif()
endif()

set(CMAKE_DEBUG_POSTFIX d)

if(NOT DEFINED ENV{NDDSHOME})
    message(FATAL_ERROR "NDDSHOME environment variable must be set")
endif()

# Find RTI Connext dependencies
list(APPEND CMAKE_MODULE_PATH
    "$ENV{NDDSHOME}/resource/cmake"
    "${CMAKE_SOURCE_DIR}/resources/cmake"
)
find_package(
    RTIConnextDDS "7.3.0"
    REQUIRED
    COMPONENTS
        core
        routing_service
        recording_service
)

# Object library: SampleInfoType
#   Contains definitions and methods for (de)serializing DDS SampleInfo

# Run Codegen to generate the source code files for the types
include(ConnextDdsCodegen)
connextdds_rtiddsgen_run(
    IDL_FILE
        "$ENV{NDDSHOME}/resource/idl/dds_rtf2_dcps.idl"
    OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/include/rti/recording/storage/mcap_plugin/common/idl"
    VAR "SampleInfoType"
    LANG C++11
    EXTRA_ARGS -verbosity 1
)

add_library(
    SampleInfoType
    STATIC
    ${SampleInfoType_CXX11_GENERATED_SOURCES}
)

target_link_libraries(
    SampleInfoType
    RTIConnextDDS::cpp2_api
    ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    SampleInfoType
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)

target_compile_features(
    SampleInfoType
    PRIVATE
        cxx_std_17
)

target_compile_options(
    SampleInfoType
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

# Object library: McapStorageCommon
#   Contains common infrastructure for storage plugin
#   E.g.: (de)serialization, raw buffer format conversion, timestamp conversion
add_library(
    McapStorageCommon
    STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/type.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/info.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/data.cxx"
)

target_link_libraries(
    McapStorageCommon
    SampleInfoType
    RTIConnextDDS::routing_service_infrastructure
    RTIConnextDDS::cpp2_api
    ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    McapStorageCommon
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:McapReader,INTERFACE_INCLUDE_DIRECTORIES>>"
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:McapWriter,INTERFACE_INCLUDE_DIRECTORIES>>"
)

target_compile_features(
    McapStorageCommon
    PUBLIC
        cxx_std_17
)

target_compile_options(
    McapStorageCommon
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

# Shared library: McapStorageRecord
#   Shared library for RTI Recording Service Storage Plugin (MCAP format)
add_library(
    McapStorageRecord
    SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/McapStorageWriter.cxx"
)

target_link_libraries(
    McapStorageRecord
    McapWriter
    McapStorageCommon
)

target_include_directories(
    McapStorageRecord
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

# Shared library: McapStorageReplay
#   Shared library for RTI Replay Service Storage Plugin (MCAP format)
add_library(
    McapStorageReplay
    SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/McapStorageReader.cxx"
)

target_link_libraries(
    McapStorageReplay
    McapReader
    McapStorageCommon
)

target_include_directories(
    McapStorageReplay
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)


# Install the plugin shared libraries
install(
    TARGETS
        McapStorageRecord
        McapStorageReplay
    LIBRARY DESTINATION lib
)