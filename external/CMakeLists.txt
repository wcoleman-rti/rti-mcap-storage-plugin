cmake_minimum_required(VERSION 3.14)
project(RTIStoragePluginMcap::External)

set(MCAP_VERSION v1.4.1)

# ================================================
# mcap

# Try to find MCAP first
find_package(mcap QUIET)

if (NOT mcap_FOUND)
    message(STATUS "MCAP not found, fetching it via FetchContent...")

    include(FetchContent)
    FetchContent_Declare(
        mcap
        GIT_REPOSITORY https://github.com/foxglove/mcap.git
        GIT_TAG        releases/cpp/${MCAP_VERSION}
    )
    FetchContent_MakeAvailable(mcap)

    # Add the MCAP CMakeLists.txt
    # add_subdirectory(${mcap_SOURCE_DIR}/cpp ${mcap_BINARY_DIR})

    set(MCAP_SOURCE_DIR ${mcap_SOURCE_DIR}/cpp/mcap)

    # define reader interface library
    add_library(
      mcap_interface
      INTERFACE
      "${MCAP_SOURCE_DIR}/include/mcap/mcap.hpp"
    )

    target_compile_features(
      mcap_interface
      INTERFACE
        cxx_std_17
    )

    target_include_directories(
      mcap_interface
      INTERFACE
        $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${MCAP_SOURCE_DIR}/include/mcap>
        $<INSTALL_INTERFACE:include>
    )

    
    if (NOT DEFINED MCAP_COMPRESSION_NO_LZ4)
      find_package(lz4 QUIET)
      if (${lz4_FOUND})
        message(STATUS "Found lz4 from ${lz4_DIR}")
        target_link_libraries(
          mcap_interface
          INTERFACE
            lz4
          )
      else()
        message(WARNING "Dependency LZ4 not found, disabling")
        target_compile_definitions(mcap PUBLIC "MCAP_COMPRESSION_NO_LZ4")
      endif()
    endif()

    
    if (NOT DEFINED MCAP_COMPRESSION_NO_ZSTD)
      find_package(zstd QUIET)
      if (${zstd_FOUND})
        message(STATUS "Found zstd from ${zstd_DIR}")
        target_link_libraries(
          mcap_interface
          INTERFACE
            zstd
          )
      else()
        message(WARNING "Dependency zstd not found, disabling")
        target_compile_definitions(mcap PUBLIC "MCAP_COMPRESSION_NO_ZSTD")
      endif()
    endif()
    
endif()