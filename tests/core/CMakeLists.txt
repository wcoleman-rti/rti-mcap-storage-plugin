add_executable(rti_mcap_core_test core_test.cxx)
target_link_libraries(rti_mcap_core_test PRIVATE
  rti::mcap::core RTIMcapTestTypes RTIMcapTestMain)
rti_mcap_target_options(rti_mcap_core_test)
add_test(NAME rti_mcap_core_test COMMAND rti_mcap_core_test)
