add_library(RTIMcapFile STATIC src/File.cpp)
add_library(rti::mcap::file ALIAS RTIMcapFile)

target_include_directories(RTIMcapFile PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(RTIMcapFile PUBLIC
  RTIMcapMcap
)
target_compile_features(RTIMcapFile PUBLIC cxx_std_17)
set_target_properties(RTIMcapFile PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME file)
rti_mcap_target_options(RTIMcapFile)

install(TARGETS RTIMcapFile EXPORT RTIMcapfileTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/rti DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
