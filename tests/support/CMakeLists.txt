if(RTI_MCAP_BUILD_CORE OR RTI_MCAP_BUILD_BENCHMARKS)
  include(ConnextDdsCodegen)
  connextdds_rtiddsgen_run(
    IDL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../idl/Test.idl"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/idl"
    VAR RTIMcapTestTypes
    LANG C++11
    EXTRA_ARGS -unboundedSupport -standard DDS_PSM_Cxx
  )
  add_library(RTIMcapTestTypes STATIC ${RTIMcapTestTypes_CXX11_GENERATED_SOURCES})
  target_link_libraries(RTIMcapTestTypes PUBLIC RTIConnextDDS::cpp2_api)
  target_include_directories(RTIMcapTestTypes PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
  target_compile_features(RTIMcapTestTypes PUBLIC cxx_std_17)
  rti_mcap_target_options(RTIMcapTestTypes)
endif()
add_library(RTIMcapTestSupport INTERFACE)
target_include_directories(RTIMcapTestSupport INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
if(RTI_MCAP_BUILD_TESTS)
  add_library(RTIMcapTestMain STATIC TestMain.cpp)
  target_link_libraries(RTIMcapTestMain PUBLIC doctest::doctest RTIMcapTestSupport)
  rti_mcap_target_options(RTIMcapTestMain)
endif()
