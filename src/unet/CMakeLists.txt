add_library(nccl-net SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/plugin.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/unet_ib.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/ibvwrap.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/socket.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/misc.cc"
  "${CMAKE_SOURCE_DIR}/src/ucommd/logger.cc"
  "${CMAKE_SOURCE_DIR}/src/ucommd/stats.cc"
  "${CMAKE_SOURCE_DIR}/src/ucommd/gen_vtopo.cc"
  "${CMAKE_SOURCE_DIR}/src/ucommd/unet_perf.cc")
target_link_libraries(nccl-net PRIVATE pthread rt ibverbs)
