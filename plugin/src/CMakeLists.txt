file(GLOB PLUGIN_SRC *.cc)

set(GEN_PLUGIN OFF CACHE BOOL "Control if generate shared objects of plugin")

message(STATUS "plugin source files: ${PLUGIN_SRC}")

###############################################
# Generate shared object to the ${RootPath}/contents/* which
# source files from current directory
###############################################
if (${GEN_PLUGIN})
  message(STATUS "==========Generate plugin==========start")
  foreach (plugin_src ${PLUGIN_SRC})
    get_filename_component(plugin_file ${plugin_src} NAME_WE)
    message(STATUS "plugin: ${plugin_file}")
    execute_process(COMMAND g++
      -g -o ${PROJECT_SOURCE_DIR}/resources/contents/${plugin_file}
      ${plugin_src}
      -I${PROJECT_SOURCE_DIR}/src
      -shared -fPIC -L${CMAKE_BINARY_DIR}/lib -Wl,-rpath,${CMAKE_BINARY_DIR}/lib -lhttp_common)
  endforeach (plugin_src ${PLUGIN_SRC})
  message(STATUS "==========Generate plugin==========end")
endif (${GEN_PLUGIN})