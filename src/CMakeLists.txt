###########################################################
# Get the source files of server1 and server2
###########################################################
aux_source_directory(common COMMON_SRC)
aux_source_directory(unix UNIX_SRC)
aux_source_directory(http HTTP_SRC)
aux_source_directory(http2 HTTP2_SRC)
aux_source_directory(util UTIL_SRC)
aux_source_directory(config CONFIG_SRC)

set(HTTP_SERVER_SRC_1
  ${COMMON_SRC} ${HTTP_SRC} ${UTIL_SRC} ${UNIX_SRC})

set(HTTP_SERVER_SRC_2
  ${COMMON_SRC} ${HTTP2_SRC} ${UTIL_SRC} ${CONFIG_SRC})

###########################################################
# The http_common is useful for HttpDynamicResponse
###########################################################
GenLib(http_common ${COMMON_SRC})
GenLib(http_server_src1 ${HTTP_SERVER_SRC_1})
GenLib(http_server_src2 ${HTTP_SERVER_SRC_2})

set(BUILD_SERVER_2 ON CACHE BOOL "Control if build the http server2")

if (NOT ${BUILD_SERVER_2})
  add_executable(httpd_kanon1 http/http_server.cc main.cc)
  target_link_libraries(httpd_kanon1 kanon_base kanon_net http_server_src1)
  set_target_properties(httpd_kanon1
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    COMMOND http_server1)
else ()
  add_executable(httpd_kanon main2.cc http2/http_server2.cc)
  target_link_libraries(httpd_kanon kanon_base kanon_net http_server_src2 boost_program_options dl)
  set_target_properties(httpd_kanon
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    COMMOND http_server2)
endif (NOT ${BUILD_SERVER_2})

