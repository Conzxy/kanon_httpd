set(CGI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

include_directories(${CGI_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(test)
