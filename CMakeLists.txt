cmake_minimum_required(VERSION 3.10)

project(Cereal CXX)

set(CMAKE_CXX_STANDARD 20)

include_directories(include)
link_directories(src)

# add_library(${PROJECT_NAME} STATIC include/cereal.hpp src/cereal.cpp "src/main.cpp")
add_executable(${PROJECT_NAME} src/main.cpp src/cereal.cpp)