add_executable(rti_mcap_storage_test storage_test.cxx)
target_link_libraries(rti_mcap_storage_test PRIVATE
  RTIMcapTestMain RTIMcapTestTypes RTIMcapStorageCommon
  rti::mcap::storage::record rti::mcap::storage::replay)
add_executable(rti_mcap_utility_test utils.cxx)
target_link_libraries(rti_mcap_utility_test PRIVATE
  RTIMcapTestMain RTIMcapTestTypes RTIMcapStorageCommon)
foreach(target IN ITEMS rti_mcap_storage_test rti_mcap_utility_test)
  rti_mcap_target_options(${target})
  add_test(NAME ${target} COMMAND ${target})
endforeach()
