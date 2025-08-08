cmake_minimum_required(VERSION 3.20)

project(VimUsing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


include_directories(./include)
add_executable(VimUsing ./src/main.cpp)
