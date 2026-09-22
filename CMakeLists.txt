cmake_minimum_required(VERSION 3.14)
project(blueboat_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cl-bootstrap.cmake)
cl_repo()

option(BLUEBOAT_BUILD_EXAMPLES "Build the blueboat-cpp chat example (server + client)" ${PROJECT_IS_TOP_LEVEL})

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)

if(BLUEBOAT_BUILD_EXAMPLES)
  add_subdirectory(examples/server)
  add_subdirectory(examples/client)
endif()
