if (${BUILD_ALL_TESTS})
  add_executable(cgi_parse_test cgi_parse_test.cc ${CGI_SOURCE_DIR}/util/cgi_parse.cc)
endif (${BUILD_ALL_TESTS})
