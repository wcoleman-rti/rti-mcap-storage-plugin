add_executable(rti_mcap_benchmark EXCLUDE_FROM_ALL benchmark.cxx)
target_link_libraries(rti_mcap_benchmark PRIVATE
  RTIMcapTestTypes RTIMcapTestSupport RTIMcapStorageCommon
  rti::mcap::storage::record rti::mcap::storage::replay)
rti_mcap_target_options(rti_mcap_benchmark)
