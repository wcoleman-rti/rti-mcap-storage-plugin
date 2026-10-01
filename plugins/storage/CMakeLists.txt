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
add_library(RTIMcapStorageCommon STATIC src/Utils.cxx)
target_link_libraries(RTIMcapStorageCommon PUBLIC
  rti::mcap::core
  rti::mcap::file
  RTIConnextDDS::cpp2_api
  RTIConnextDDS::routing_service_infrastructure
)
target_include_directories(RTIMcapStorageCommon PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(RTIMcapStorageCommon PUBLIC cxx_std_17)
set_target_properties(RTIMcapStorageCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)
rti_mcap_target_options(RTIMcapStorageCommon)

add_library(RTIMcapStorageRecord SHARED src/McapWriter.cxx)
add_library(RTIMcapStorageReplay SHARED src/McapReader.cxx)
add_library(rti::mcap::storage::record ALIAS RTIMcapStorageRecord)
add_library(rti::mcap::storage::replay ALIAS RTIMcapStorageReplay)
foreach(target IN ITEMS RTIMcapStorageRecord RTIMcapStorageReplay)
  target_link_libraries(${target} PRIVATE RTIMcapStorageCommon)
  rti_mcap_target_options(${target})
  set_target_properties(${target} PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
  )
endforeach()
set_target_properties(RTIMcapStorageRecord PROPERTIES
  OUTPUT_NAME rtirecordingstoragemcaprecord EXPORT_NAME storage::record)
set_target_properties(RTIMcapStorageReplay PROPERTIES
  OUTPUT_NAME rtirecordingstoragemcapreplay EXPORT_NAME storage::replay)

install(TARGETS RTIMcapStorageRecord RTIMcapStorageReplay EXPORT RTIMcapstorageTargets
  LIBRARY DESTINATION "$<IF:$<CONFIG:Debug>,debug,release>/lib"
  RUNTIME DESTINATION "$<IF:$<CONFIG:Debug>,debug,release>/bin"
  ARCHIVE DESTINATION "$<IF:$<CONFIG:Debug>,debug,release>/lib"
)
