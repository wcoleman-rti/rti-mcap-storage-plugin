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


# CMAKE Build Type default = Release
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