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
project(RTIStoragePluginMcap::Plugin)

# Object library: SampleInfoType
#   Contains definitions and methods for (de)serializing DDS SampleInfo

# Run Codegen to generate the source code files for the types
include(ConnextDdsCodegen)
connextdds_rtiddsgen_run(
    IDL_FILE
        "$ENV{NDDSHOME}/resource/idl/dds_rtf2_dcps.idl"
    OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/idl/
    VAR "SampleInfoIncludes"
    LANG C++11
    EXTRA_ARGS -verbosity 1
)

connextdds_rtiddsgen_run(
    IDL_FILE
        ${CMAKE_SOURCE_DIR}/resources/idl/SampleInfo.idl
    INCLUDE_DIRS
        "$ENV{NDDSHOME}/resource/idl/"
    OUTPUT_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/idl/
    VAR "SampleInfoType"
    LANG C++11
    EXTRA_ARGS -verbosity 1
)

add_library(
    SampleInfoTypeLib
    STATIC
    ${SampleInfoIncludes_CXX11_GENERATED_SOURCES}
    ${SampleInfoType_CXX11_GENERATED_SOURCES}
)

target_link_libraries(
    SampleInfoTypeLib
    RTIConnextDDS::cpp2_api
    ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    SampleInfoTypeLib
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_compile_features(
    SampleInfoTypeLib
    PUBLIC
        cxx_std_17
)

target_compile_options(
    SampleInfoTypeLib
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

# Object library: PluginCommon
#   Common static library for both Record and Replay Plugins
#   Includes:
#     - Utils.cxx: Utility functions for both plugins
#     - mcap: MCAP library
#     - SampleInfoTypeLib: SampleInfo type definitions and methods for (de)serializing DDS SampleInfo
#     - RTIConnextDDS::cpp2_api: RTI Connext DDS C++ API
#     - RTIConnextDDS::routing_service_infrastructure: RTI Routing Service Infrastructure
#     - ${CONNEXTDDS_EXTERNAL_LIBS}: External libraries required by RTI Connext DDS

add_library(
    PluginCommon
    STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils.cxx
)

target_link_libraries(
    PluginCommon
        mcap
        SampleInfoTypeLib
        RTIConnextDDS::cpp2_api
        RTIConnextDDS::routing_service_infrastructure
        ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    PluginCommon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_features(
    PluginCommon
    PUBLIC
        cxx_std_17
)

target_compile_options(
    PluginCommon
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

# Shared library: McapStorageRecord
#   Shared library for RTI Recording Service Storage Plugin (MCAP format)
add_library(
    McapStorageRecord
    SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/McapWriter.cxx
)

target_link_libraries(
    McapStorageRecord
        PluginCommon
)

target_include_directories(
    McapStorageRecord
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Shared library: McapStorageReplay
#   Shared library for RTI Replay Service Storage Plugin (MCAP format)
add_library(
    McapStorageReplay
    SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/McapReader.cxx
)

target_link_libraries(
    McapStorageReplay
        PluginCommon
)

target_include_directories(
    McapStorageReplay
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)


# Install the plugin shared libraries
install(
    TARGETS
        McapStorageRecord
        McapStorageReplay
    CONFIGURATIONS Debug
    LIBRARY DESTINATION debug/lib
)

set_target_properties(
    McapStorageRecord
    PROPERTIES
        OUTPUT_NAME rtirecordingstoragemcaprecord
)

set_target_properties(
    McapStorageReplay
    PROPERTIES
        OUTPUT_NAME rtirecordingstoragemcapreplay
)

install(
    TARGETS
        McapStorageRecord
        McapStorageReplay
    CONFIGURATIONS Release
    LIBRARY DESTINATION release/lib
)