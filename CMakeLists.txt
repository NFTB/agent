cmake_minimum_required(VERSION 3.16)
project(agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(main main.cpp)

if(APPLE)
    target_link_libraries(main PRIVATE proc)
endif()
