cmake_minimum_required(VERSION 3.14)
project(mcap_implementation LANGUAGES C CXX)

# mcap writer implementation
add_library(
    McapWriter
    OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/writer_impl.cxx"
)
target_link_libraries(
    McapWriter
    McapWriterInterface
)

# mcap reader implementation
add_library(
    McapReader
    OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/reader_impl.cxx"
)
target_link_libraries(
    McapReader
    McapReaderInterface
)