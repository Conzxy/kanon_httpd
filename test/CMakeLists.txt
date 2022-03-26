file(GLOB_RECURSE HTTP_TEST_SOURCES ${PROJECT_SOURCE_DIR}/test/*/*test.cc)

macro (RemoveDirFiles srcs dir)
  list(FILTER ${srcs} EXCLUDE REGEX "[0-9a-zA-Z_]*/${dir}/[0-9a-zA-Z_]*")
endmacro ()

if (${BUILD_SERVER_2})
  RemoveDirFiles(HTTP_TEST_SOURCES http)
  RemoveDirFiles(HTTP_TEST_SOURCES unix)
  RemoveDirFiles(HTTP_TEST_SOURCES plugin)
else ()
  RemoveDirFiles(HTTP_TEST_SOURCES plugin)
  RemoveDirFiles(HTTP_TEST_SOURCES http2)
endif (${BUILD_SERVER_2})

message(STATUS "http test source files: ${HTTP_TEST_SOURCES}")

set(BUILD_ALL_TESTS OFF CACHE BOOL "Control if build all http tests")

function (GenTest http_test_filename http_test_source)
  if (${BUILD_ALL_TESTS}) 
    add_executable(${http_test_filename} ${http_test_source})
  else ()
    add_executable(${http_test_filename} EXCLUDE_FROM_ALL ${http_test_source})
  endif (${BUILD_ALL_TESTS})

  set_target_properties(${http_test_filename}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    COMMAND ${http_test_filename})
endfunction ()

foreach (http_test_source ${HTTP_TEST_SOURCES})
  get_filename_component(http_test_filename ${http_test_source} NAME_WE)
  
  GenTest(${http_test_filename} ${http_test_source})

  if (${BUILD_SERVER_2})
    target_link_libraries(${http_test_filename} http_server_src2 gtest kanon_base kanon_net)
  else ()
    target_link_libraries(${http_test_filename} http_server_src1 gtest kanon_base kanon_net)
  endif (${BUILD_SERVER_2})
endforeach (http_test_source ${HTTP_TEST_SOURCES})

add_subdirectory(plugin)