include(FetchContent)
set(DOCTEST_NO_INSTALL ON)
set(_extract_timestamp)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  set(_extract_timestamp DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
FetchContent_Declare(doctest
  URL "https://codeload.github.com/doctest/doctest/tar.gz/refs/tags/v2.4.12"
  URL_HASH "SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70"
  ${_extract_timestamp}
)
FetchContent_MakeAvailable(doctest)
