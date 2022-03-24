cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

project(kanon_http)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Og")

set(CXX_FLAGS 
 -g
 -Wall
 -Wextra
 -Wno-return-local-addr
 -Wno-unused-parameter
 -Wno-unused-function
 -Wno-switch
 -Wno-format-security
 -Werror
 #-Wconversion
 #-Wshadow
 -Wwrite-strings # in fact, this is default specified
 #cxx standard 
 -std=c++11
 -pthread
	
 # linker opt
 -rdynamic
 # machine opt
 -march=native
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	list(REMOVE_ITEM CXX_FLAGS "-Wno-return-local-addr")
	list(REMOVE_ITEM CXX_FLAGS "-rdynamic")
	list(APPEND CXX_FLAGS "-Wthread-safety")
endif()

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}; BUILD_TYPE: ${CAMKE_BUILD_TYPE}")

include_directories(${PROJECT_SOURCE_DIR}/src)

function (GenLib lib)
	add_library(${lib} ${ARGN})

	if (NOT ${BUILD_SHARED_LIBS})
		message(STATUS "Build static library: ${lib}")
		set_target_properties(${lib}
			PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		)
	else ()
		message(STATUS "Build shared library: ${lib}")
		set_target_properties(${lib}
			PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		)
	endif (NOT ${BUILD_SHARED_LIBS})
endfunction ()

set(BUILD_ALL_TESTS OFF CACHE BOOL "Build all http tests")
set(BUILD_SERVER_2 OFF CACHE BOOL "Build the http server2")

message(STATUS "BUILD_ALL_TESTS = ${BUILD_ALL_TESTS}")
message(STATUS "BUILD_SERVER_2 = ${BUILD_SERVER_2}")

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(cgi)