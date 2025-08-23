cmake_minimum_required(VERSION 3.22)
project(joyuinput LANGUAGES C CXX)

add_library(joyuinput SHARED joyuinput.cpp)

find_library(log-lib log)

target_link_libraries(joyuinput ${log-lib})
