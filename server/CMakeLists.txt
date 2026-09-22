file(GLOB BLUEBOAT_SERVER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(blueboat_server STATIC ${BLUEBOAT_SERVER_SOURCES})
target_include_directories(blueboat_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(blueboat_server PUBLIC cxx_std_20)
target_link_libraries(blueboat_server PUBLIC blueboat_common)

cl_add_dep(blueboat_server openssl PRIVATE)
