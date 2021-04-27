cmake_minimum_required(VERSION 3.15)

project(Networking CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.71 REQUIRED COMPONENTS program_options "date_time" "regex" )
find_library(boost boost)
include_directories("/usr/local/include/boost")

add_executable(TCPServer  TCPServer/TCPServer.cpp 
                          TCPServer/server_certificate.hpp 
                          TCPServer/inja.hpp 
                          TCPServer/json.hpp)
target_link_libraries(
    TCPServer  PRIVATE Boost::headers Boost::program_options Boost::date_time Boost::regex ssl crypto pthread
                          )

include(GNUInstallDirs)
install(TARGETS TCPServer  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})