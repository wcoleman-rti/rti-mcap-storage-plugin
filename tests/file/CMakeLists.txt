add_executable(rti_mcap_file_test file_test.cxx)
target_link_libraries(rti_mcap_file_test PRIVATE rti::mcap::file RTIMcapTestMain)
rti_mcap_target_options(rti_mcap_file_test)
add_test(NAME rti_mcap_file_test COMMAND rti_mcap_file_test)
