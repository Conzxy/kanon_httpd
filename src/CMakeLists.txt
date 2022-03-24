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

GenLib(http_server_src1 ${HTTP_SERVER_SRC_1})
GenLib(http_server_src2 ${HTTP_SERVER_SRC_2})

if (NOT ${BUILD_SERVER_2})
  add_executable(http_server1 http_server.cc main.cc)
  target_link_libraries(http_server1 kanon_base kanon_net http_server_src1)
  set_target_properties(http_server1
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    COMMOND http_server1)
endif (NOT ${BUILD_SERVER_2})

# add_executable(http_server2 ${PROJECT_SOURCE_DIR}/main2.cc ${HTTP_SERVER_SRC_2})
# target_link_libraries(http_server kanon_base kanon_net)
