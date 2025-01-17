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
project(RTIStoragePluginMcap::Tests)

# Object library: SampleInfoType
#   Contains definitions and methods for (de)serializing DDS SampleInfo

# Run Codegen to generate the source code files for the types
include(ConnextDdsCodegen)
connextdds_rtiddsgen_run(
    IDL_FILE
        "${CMAKE_SOURCE_DIR}/resources/idl/Test.idl"
    OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/idl"
    VAR "TestType"
    LANG C++11
    EXTRA_ARGS
        -unboundedSupport
        -verbosity 1
)

add_library(
    TestTypeLib
    STATIC
        ${TestType_CXX11_GENERATED_SOURCES}
)

target_link_libraries(
    TestTypeLib
        RTIConnextDDS::cpp2_api
        ${CONNEXTDDS_EXTERNAL_LIBS}
)

target_include_directories(
    TestTypeLib
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)

target_compile_features(
    TestTypeLib
    PUBLIC
        cxx_std_17
)

target_compile_options(
    TestTypeLib
    PUBLIC
      -Wall -Wextra -Wpedantic -fPIC
)

set(TestSources
    "${CMAKE_CURRENT_SOURCE_DIR}/test.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils.cxx"
)


foreach(TestSource ${TestSources})

    get_filename_component(ExeName ${TestSource} NAME_WE)

    # Object library: McapStorageCommon
    #   Contains common infrastructure for storage plugin
    #   E.g.: (de)serialization, raw buffer format conversion, timestamp conversion
    add_executable(
        ${ExeName}
            ${TestSource}
    )

    target_link_libraries(
        ${ExeName}
            TestTypeLib
            McapStorageRecord
            McapStorageReplay
    )

    target_include_directories(
        ${ExeName}
        PRIVATE
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    )
endforeach()