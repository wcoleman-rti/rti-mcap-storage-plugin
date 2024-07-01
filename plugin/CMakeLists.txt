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

if(NOT BUILD_SHARED_LIBS)
    set(msg
        "Static linking is not supported for this example. You can build it "
        "using the -DBUILD_SHARED_LIBS=ON CMake parameter")
    message(FATAL_ERROR ${msg})
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(msg
        "Not specifying CMAKE_BUILD_TYPE can lead to undefined behaviour and "
        "is not supported for this example. Please provide a valid value for "
        "the CMAKE_BUILD_TYPE parameter")
    message(FATAL_ERROR ${msg})
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

# Object library: StreamInfoType
#   Contains definitions and methods for (de)serializing DDS SampleInfo

# Run Codegen to generate the source code files for the types
include(ConnextDdsCodegen)
connextdds_rtiddsgen_run(
    IDL_FILE
        "$ENV{NDDSHOME}/resource/idl/dds_rtf2_dcps.idl"
    OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/include/rti/recording/storage/mcap_plugin/common/idl"
    VAR "StreamInfoType"
    LANG C++11
    EXTRA_ARGS -verbosity 1
)

add_library(
    StreamInfoType
    OBJECT
    ${StreamInfoType_CXX11_GENERATED_SOURCES}
)

target_link_libraries(
    StreamInfoType
    RTIConnextDDS::cpp2_api
    ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    StreamInfoType
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
)

target_compile_features(
    StreamInfoType
    PRIVATE
        cxx_std_17
)

target_compile_options(
    StreamInfoType
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

# Object library: McapStorageCommon
#   Contains common infrastructure for storage plugin
#   E.g.: (de)serialization, raw buffer format conversion, timestamp conversion
add_library(
    McapStorageCommon
    OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/type.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/info.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rti/recording/storage/mcap_plugin/common/data.cxx"
)

target_sources(
    McapStorageCommon
    INTERFACE
    $<TARGET_OBJECTS:StreamInfoType>
)

target_link_libraries(
    McapStorageCommon
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
    "$<BUILD_INTERFACE:$<TARGET_PROPERTY:StreamInfoType,INTERFACE_INCLUDE_DIRECTORIES>>"
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