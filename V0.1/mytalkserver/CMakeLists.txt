cmake_minimum_required(VERSION 3.20)

project(mytalk)

set(CMAKE_CXX_STANDARD 17)

add_executable(mytalkserver
        server.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(mytalkserver PRIVATE Threads::Threads)