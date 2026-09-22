function(_recipe_blueboat-cpp_source)
  set(BLUEBOAT_TAG "v0.1.0")
  if(CL_REQ_VERSION)
    set(BLUEBOAT_TAG "v${CL_REQ_VERSION}")
  endif()


  cl_import_source(
    NAME blueboat-cpp
    URL https://github.com/OpenKit-OSS/blueboat-cpp/archive/refs/tags/${BLUEBOAT_TAG}.tar.gz
  )

  add_library(deps::blueboat-cpp::server ALIAS blueboat_server)
  add_library(deps::blueboat-cpp::client ALIAS blueboat_client)
  add_library(deps::blueboat-cpp ALIAS blueboat_common)
endfunction()
