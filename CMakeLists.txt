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


# update git submodules
execute_process(
    COMMAND git submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Script failed with error: ${error}")
else()
    message(STATUS "git submodule update: ${output}")
endif()


add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mcap_implementation")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/plugin")