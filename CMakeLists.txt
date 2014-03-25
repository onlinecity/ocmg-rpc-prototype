cmake_minimum_required (VERSION 2.8.8)
project (ocmg-rpc-prototype)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Set a default build type if none was specified (we are not using an IDE)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Google protobuf
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

# Google flags
find_library(GFLAGS_LIB gflags REQUIRED)
find_path(GFLAGS_INCLUDE gflags.h PATH_SUFFIXES gflags)
include_directories(${GFLAGS_INCLUDE})

# Google log
find_library(GLOG_LIB glog REQUIRED)
find_path(GLOG_INCLUDE logging.h PATH_SUFFIXES glog)
include_directories(${GLOG_INCLUDE})

# ZMQ
find_package(ZeroMQ REQUIRED)
include_directories(${ZEROMQ_INCLUDE_DIR})

set(link_libs
    ${PROTOBUF_LIBRARIES}
    ${GLOG_LIB}
    ${GFLAGS_LIB}
    ${ZEROMQ_LIBRARIES}
)

include_directories (src)
add_subdirectory (src)

option (ENABLE_TEST "Compile unit testing" ON)

if (ENABLE_TEST)
    include(GTest.cmake)
    include_directories(${GTEST_INCLUDE_DIR})
    add_subdirectory(test)
endif (ENABLE_TEST)
