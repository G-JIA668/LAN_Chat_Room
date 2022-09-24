cmake_minimum_required(VERSION 3.0)

project(REACTORTEST CXX)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CXX_FLAGS
 -g
 -D_FILE_OFFSET_BITS=64
 -Wall
 -Wextra
 -Wconversion
 -Wno-unused-parameter
 -Woverloaded-virtual
 -Wpointer-arith
 -Wshadow
 -Wwrite-strings
 -march=native
 -rdynamic
 )
if(CMAKE_BUILD_BITS EQUAL 32)
  list(APPEND CXX_FLAGS "-m32")
endif()
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")

find_package(Boost REQUIRED)
find_package(Protobuf)
find_package(CURL)
find_path(CARES_INCLUDE_DIR ares.h)
find_library(CARES_LIBRARY NAMES cares)
find_path(MHD_INCLUDE_DIR microhttpd.h)
find_library(MHD_LIBRARY NAMES microhttpd)
find_library(BOOSTTEST_LIBRARY NAMES boost_unit_test_framework)

find_library(MUDUO_BASE muduo_base ./lib)
find_library(MUDUO_NET muduo_net ./lib)


include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS} " " ${CMAKE_CXX_FLAGS_${BUILD_TYPE}})


add_executable(chatLogServer.exe chatLogServer.cpp)
target_link_libraries(chatLogServer.exe ${MUDUO_NET} ${MUDUO_BASE} pthread)

add_executable(joiner.exe joiner.cpp)
target_link_libraries(joiner.exe ${MUDUO_NET} ${MUDUO_BASE} pthread)

add_executable(inputbox inputbox.cpp)

