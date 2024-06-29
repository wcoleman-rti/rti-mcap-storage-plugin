cmake_minimum_required(VERSION 3.14)
project(mcap LANGUAGES C CXX)

set (MCAP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mcap/cpp/mcap)

add_library(
    McapReaderInterface
    INTERFACE
    "${MCAP_SOURCE_DIR}/include/mcap/reader.hpp"
)

add_library(
    McapWriterInterface
    INTERFACE
    "${MCAP_SOURCE_DIR}/include/mcap/writer.hpp"
)

target_compile_definitions(McapReaderInterface
  INTERFACE
    MCAP_COMPRESSION_NO_ZSTD
    MCAP_COMPRESSION_NO_LZ4
)

target_compile_features(McapReaderInterface
  INTERFACE
    cxx_std_17
)

target_compile_definitions(McapWriterInterface
  INTERFACE
    MCAP_COMPRESSION_NO_ZSTD
    MCAP_COMPRESSION_NO_LZ4
)

target_include_directories(McapReaderInterface
  INTERFACE
    $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include/mcap>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(McapWriterInterface
  INTERFACE
    $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include/mcap>
    $<INSTALL_INTERFACE:include>
)