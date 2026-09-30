include(FetchContent)

set(_extract_timestamp)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  set(_extract_timestamp DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_Declare(mcap_source
  URL "https://github.com/foxglove/mcap/archive/refs/tags/releases/cpp/v2.1.3.tar.gz"
  URL_HASH "SHA256=181a7c52cc982444cdef533087b321538b025fcc5eec258e0489d68b9ce53ded"
  ${_extract_timestamp}
)
FetchContent_MakeAvailable(mcap_source)
set(RTI_MCAP_SOURCE_DIR "${mcap_source_SOURCE_DIR}/cpp/mcap")
if(NOT EXISTS "${RTI_MCAP_SOURCE_DIR}/include/mcap/writer.hpp")
  message(FATAL_ERROR "The MCAP v2.1.3 C++ headers were not found")
endif()

add_library(RTIMcapMcapHeaders INTERFACE)
set_target_properties(RTIMcapMcapHeaders PROPERTIES EXPORT_NAME mcap_headers)
target_include_directories(RTIMcapMcapHeaders SYSTEM INTERFACE
  "$<BUILD_INTERFACE:${RTI_MCAP_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(RTIMcapMcapHeaders INTERFACE cxx_std_17)
install(TARGETS RTIMcapMcapHeaders EXPORT RTIMcapDependenciesTargets)
install(DIRECTORY "${RTI_MCAP_SOURCE_DIR}/include/mcap"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(RTI_MCAP_BUILD_FILE)
  include(Implementation.cmake)
endif()
