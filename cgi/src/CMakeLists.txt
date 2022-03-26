
file(GLOB CGI_SOURCES *.cc)

message(STATUS "cgi source files: ${CGI_SOURCES}")

##################################################
# Generate the cgi executable files
##################################################
foreach (cgi_source ${CGI_SOURCES}) 
  get_filename_component(cgi_filename ${cgi_source} NAME_WE)
  add_executable(${cgi_filename} ${cgi_source} util/cgi_parse.cc)
  target_link_libraries(${cgi_filename} kanon_base kanon_net http_common)

  set_target_properties(${cgi_filename}
    PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/resources/cgi)
endforeach (cgi_source ${CGI_SOURCES}) 