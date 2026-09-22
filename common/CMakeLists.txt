file(GLOB BLUEBOAT_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(blueboat_common STATIC ${BLUEBOAT_COMMON_SOURCES})
target_include_directories(blueboat_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(blueboat_common PUBLIC cxx_std_20)

cl_add_dep(blueboat_common wslay PUBLIC)
cl_add_dep(blueboat_common nlohmann_json PUBLIC)
cl_add_dep(blueboat_common msgpack PUBLIC)
cl_add_dep(blueboat_common openssl PRIVATE)

find_package(Threads REQUIRED)
target_link_libraries(blueboat_common PUBLIC Threads::Threads)
