function(rti_mcap_find_compression kind)
  if(TARGET rti::mcap::detail::${kind})
    return()
  endif()
  find_path(RTI_MCAP_${kind}_INCLUDE_DIR "${kind}.h" REQUIRED)
  find_library(RTI_MCAP_${kind}_LIBRARY NAMES "${kind}" REQUIRED)
  add_library(rti::mcap::detail::${kind} UNKNOWN IMPORTED GLOBAL)
  set_target_properties(rti::mcap::detail::${kind} PROPERTIES
    IMPORTED_LOCATION "${RTI_MCAP_${kind}_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${RTI_MCAP_${kind}_INCLUDE_DIR}"
  )
endfunction()
