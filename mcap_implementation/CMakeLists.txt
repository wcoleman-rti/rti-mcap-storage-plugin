cmake_minimum_required(VERSION 3.14)
project(McapImpl)

# mcap writer implementation
add_library(
    McapWriter
    STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/writer_impl.cxx"
)
target_link_libraries(
    McapWriter
    McapWriterInterface
)
target_include_directories(
    McapWriter
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

# mcap reader implementation
add_library(
    McapReader
    STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/reader_impl.cxx"
)
target_link_libraries(
    McapReader
    McapReaderInterface
)
target_include_directories(
    McapReader
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)