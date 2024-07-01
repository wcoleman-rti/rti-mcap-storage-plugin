cmake_minimum_required(VERSION 3.14)
project(McapInterface)

# ================================================
# mcap

# checkout MCAP tag releases/cpp/v1.4.0
# https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.0
execute_process(
    COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR}/mcap checkout releases/cpp/v1.4.0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Script failed with error: ${error}")
else()
    message(STATUS "git checkout v1.4.0 mcap cpp: ${output}")
endif()

set(MCAP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mcap/cpp/mcap)
set(MCAP_COMPILE_DEFINITIONS
  MCAP_COMPRESSION_NO_ZSTD
  MCAP_COMPRESSION_NO_LZ4
)
set(MCAP_IMPL_COMPILE_FEATURES cxx_std_17)
set(MCAP_IMPL_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -fPIC)
SET(MCAP_INCLUDE_DIRECTORIES
  $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include/mcap>
  # $<INSTALL_INTERFACE:include>
)

# define reader interface library
add_library(McapReaderInterface
  INTERFACE
  "${MCAP_SOURCE_DIR}/include/mcap/reader.hpp"
)

# define writer interface library
add_library(McapWriterInterface
  INTERFACE
  "${MCAP_SOURCE_DIR}/include/mcap/writer.hpp"
)

target_compile_definitions(McapReaderInterface
  INTERFACE
  ${MCAP_COMPILE_DEFINITIONS}
)
target_compile_definitions(McapWriterInterface
  INTERFACE
  ${MCAP_COMPILE_DEFINITIONS}
)

target_compile_features(McapReaderInterface
  INTERFACE
  ${MCAP_IMPL_COMPILE_FEATURES}
)
target_compile_features(McapWriterInterface
  INTERFACE
  ${MCAP_IMPL_COMPILE_FEATURES}
)

target_compile_options(McapReaderInterface
  INTERFACE
  ${MCAP_IMPL_COMPILE_OPTIONS}
)

target_compile_options(McapWriterInterface
  INTERFACE
  ${MCAP_IMPL_COMPILE_OPTIONS}
)

target_include_directories(McapReaderInterface
  INTERFACE
  ${MCAP_INCLUDE_DIRECTORIES}
)
target_include_directories(McapWriterInterface
  INTERFACE
  ${MCAP_INCLUDE_DIRECTORIES}
)

# ================================================