cmake_minimum_required(VERSION 3.14)
project(mcap_vendor LANGUAGES C CXX)

# Avoid DOWNLOAD_EXTRACT_TIMESTAMP warning for CMake >= 3.24
if (POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

## Compile options
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -fPIC)
endif()
if (MSVC)
  add_compile_options(/W4 /WX
    /wd4251 # suppress warning about having template instances (such as std::string) as public class members
  )
  # suppress error disallowing `fopen` in favor of `fopen_s`
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()


add_library(
    McapWriter
    OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/writer_impl.cxx"
)
target_link_libraries(
    McapWriter
    McapWriterInterface
)


add_library(
    McapReader
    OBJECT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcap/reader_impl.cxx"
)
target_link_libraries(
    McapReader
    McapReaderInterface
)