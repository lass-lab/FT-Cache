#This is primarily a cpp code but will use log4c to log both the C and C++ aspects
include(FindPkgConfig)
pkg_check_modules(MERCURY REQUIRED IMPORTED_TARGET mercury)
pkg_check_modules(LOG4C REQUIRED IMPORTED_TARGET log4c)


#Dynamic Target
add_library(ftc_client SHARED ftc.cpp ftc_client.cpp wrappers.c ftc_data_mover.cpp ftc_logging.c ftc_comm.cpp ftc_comm_client.cpp)
target_compile_definitions(ftc_client PUBLIC FT_Cache_CLIENT)
target_compile_definitions(ftc_client PUBLIC FT_Cache_PRELOAD)
target_include_directories(ftc_client PRIVATE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(ftc_client PROPERTIES BUILD_RPATH /sw/summit/gcc/9.1.0-alpha+20190716/lib64)
#set_target_properties(ftc_client PROPERTIES BUILD_RPATH /sw/peak/gcc/10.2.0-2/lib64/)
target_link_libraries(ftc_client PRIVATE pthread dl PkgConfig::LOG4C PkgConfig::MERCURY)

#Server Daemon
add_executable(ftc_server ftc.cpp ftc_server.cpp ftc_data_mover.cpp ftc_comm.cpp ftc_logging.c )
target_compile_definitions(ftc_server PUBLIC FT_Cache_SERVER)
target_include_directories(ftc_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
#set_target_properties(ftc_server PROPERTIES BUILD_RPATH /sw/summit/gcc/9.1.0-alpha+20190716/lib64)
set_target_properties(ftc_server PROPERTIES BUILD_RPATH /sw/peak/gcc/10.2.0-2/lib64/)
target_link_libraries(ftc_server PRIVATE pthread PkgConfig::LOG4C rt PkgConfig::MERCURY)
install(TARGETS ftc_client DESTINATION lib)
install(TARGETS ftc_server DESTINATION bin)
