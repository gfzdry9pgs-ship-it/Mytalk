
cmake_minimum_required(VERSION 3.20)

project(mytalkclient)

set(CMAKE_CXX_STANDARD 20)

add_executable(mytalkclient client.cpp)

target_link_libraries(mytalkclient ws2_32)