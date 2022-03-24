file(GLOB_RECURSE HTTP_TEST_SOURCES ${PROJECT_SOURCE_DIR}/test/*/*test.cc)


if (${BUILD_SERVER_2})
  list(FILTER HTTP_TEST_SOURCES EXCLUDE REGEX "[0-9a-zA-Z_]*/http/[0-9a-zA-Z_]*")
  list(FILTER HTTP_TEST_SOURCES EXCLUDE REGEX "[0-9a-zA-Z_]*/unix/[0-9a-zA-Z_]*")
else ()
  list(FILTER HTTP_TEST_SOURCES EXCLUDE REGEX "[0-9a-zA-Z_]*/http2/[0-9a-zA-Z_]*")
endif (${BUILD_SERVER_2})

message(STATUS "http test source files: ${HTTP_TEST_SOURCES}")

foreach (http_test_source ${HTTP_TEST_SOURCES})
  get_filename_component(http_test_filename ${http_test_source} NAME_WE)
  
  if (${BUILD_ALL_TESTS}) 
    add_executable(${http_test_filename} ${http_test_source})
  else ()
    add_executable(${http_test_filename} EXCLUDE_FROM_ALL ${http_test_source})
  endif (${BUILD_ALL_TESTS})

  if (${BUILD_SERVER_2})
    target_link_libraries(${http_test_filename} http_server_src2 gtest kanon_base kanon_net dl)
  else ()
    target_link_libraries(${http_test_filename} http_server_src1 gtest kanon_base kanon_net dl)
  endif (${BUILD_SERVER_2})
  set_target_properties(${http_test_filename}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    COMMAND ${http_test_filename})
endforeach (http_test_source ${HTTP_TEST_SOURCES})
