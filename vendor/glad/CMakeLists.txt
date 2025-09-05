cmake_minimum_required(VERSION 3.10)
project(Glad)

add_library(glad include/glad/glad.h src/glad.c)

if(UNIX)
  set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif(UNIX)
target_include_directories(glad PUBLIC include/)
