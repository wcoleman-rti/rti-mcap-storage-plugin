add_library(RTIMcapCore STATIC src/SampleMetadata.cpp)
add_library(rti::mcap::core ALIAS RTIMcapCore)

target_include_directories(RTIMcapCore PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(RTIMcapCore PUBLIC
  RTIMcapMcapHeaders
  RTIConnextDDS::cpp2_api
)
target_compile_features(RTIMcapCore PUBLIC cxx_std_17)
set_target_properties(RTIMcapCore PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME core)
rti_mcap_target_options(RTIMcapCore)

install(TARGETS RTIMcapCore EXPORT RTIMcapcoreTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/rti DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
