add_library(RTIMcapMcap STATIC mcap.cpp)
target_link_libraries(RTIMcapMcap PUBLIC RTIMcapMcapHeaders)
set_target_properties(RTIMcapMcap PROPERTIES
  POSITION_INDEPENDENT_CODE ON EXPORT_NAME mcap)
if(RTI_MCAP_ENABLE_SANITIZERS)
  target_compile_options(RTIMcapMcap PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(RTIMcapMcap PUBLIC -fsanitize=address,undefined)
endif()
install(TARGETS RTIMcapMcap EXPORT RTIMcapDependenciesTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

include(RTIMcapCompression)
foreach(kind IN ITEMS lz4 zstd)
  string(TOUPPER "${kind}" upper)
  if(RTI_MCAP_ENABLE_${upper})
    rti_mcap_find_compression(${kind})
    target_link_libraries(RTIMcapMcap PUBLIC rti::mcap::detail::${kind})
  else()
    target_compile_definitions(RTIMcapMcap PUBLIC MCAP_COMPRESSION_NO_${upper})
  endif()
endforeach()
