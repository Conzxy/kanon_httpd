function (GenPluginTest plugin_name)
  add_executable(${plugin_name} ${ARGN})
  target_link_libraries(${plugin_name} kanon_base kanon_net http_common dl)
  set_target_properties(${plugin_name}
    PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    COMMAND ${plugin_name})
endfunction ()

GenPluginTest(adder_test adder_test.cc)