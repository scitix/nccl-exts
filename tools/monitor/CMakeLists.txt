add_executable(sicl_monitor
  "${CMAKE_SOURCE_DIR}/src/ucommd/logger.cc"
  "${CMAKE_SOURCE_DIR}/src/ucommd/stats.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/monitor.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/main.cc")
set_target_properties(sicl_monitor PROPERTIES CXX_STANDARD 17)
target_link_libraries(sicl_monitor PRIVATE rt)
