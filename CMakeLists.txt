cmake_minimum_required(VERSION 3.10)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

project(kanon_http)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Og")

set(CXX_FLAGS 
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
 -std=c++14
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

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

include_directories(${PROJECT_SOURCE_DIR}/src)

# I don't want to use the builtin variable ${BUILD_SHARED_LIBS}
# since it you don't set it to ON explicitly, then the NOT 
# ${BUILD_SHARED_LIBS} will be false, and generated libraries 
# are static. That's not I wanted behavior. I want a variable,
# it can build shared libraries default even though I don't set 
# it explitly, and the build of static libraries is an option.
set(BUILD_STATIC_LIBS OFF CACHE BOOL "Build static libraries")

message(STATUS "The libraries will be stored in ${CMAKE_BINARY_DIR}/lib")
macro (GenLib lib)
	#if (Not ${BUILD_SHARED_LIBS})
	if (${BUILD_STATIC_LIBS})
		message(STATUS "Build static library: ${lib}")
		add_library(${lib} STATIC ${ARGN})
		set_target_properties(${lib}
			PROPERTIES
			ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		)
	else ()
		message(STATUS "Build shared library: ${lib}")
		add_library(${lib} SHARED ${ARGN})
		set_target_properties(${lib}
			PROPERTIES
			LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		)
	endif (${BUILD_STATIC_LIBS})
endmacro ()

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(plugin)
add_subdirectory(cgi)