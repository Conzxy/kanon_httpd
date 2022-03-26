##################################################
# Plugin test
##################################################

execute_process(COMMAND g++ 
  -shared -fpic -o ${CMAKE_BINARY_DIR}/test/plugin/demo 
  ${PROJECT_SOURCE_DIR}/test/plugin/demo.cc)

function (GenPluginTest filename)
  GenTest(${filename} ${ARGN})
  target_link_libraries(${filename} dl)
  set_target_properties(${filename}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test/plugin
    COMMAND ${filename}
  )
endfunction ()

GenPluginTest(plugin_loader_test plugin_loader_test.cc)