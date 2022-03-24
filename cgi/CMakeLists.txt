set(CGI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(${CGI_SOURCE_DIR})

function (GenCgiExec cgi_name)
  add_executable(${cgi_name} ${cgi_name}.cc util/cgi_parse.cc)
  target_link_libraries(${cgi_name} kanon_base kanon_net http_common)

  set_target_properties(${cgi_name}
  PROPERTIES 
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction ()

file(GLOB CGI_SOURCES "*.cc")

message(STATUS "cgi source files: ${CGI_SOURCES}")

foreach (cgi_source ${CGI_SOURCES}) 
  get_filename_component(cgi_source_filename ${cgi_source} NAME_WE)
  GenCgiExec(${cgi_source_filename})
endforeach (cgi_source ${CGI_SOURCES}) 

add_subdirectory(test)