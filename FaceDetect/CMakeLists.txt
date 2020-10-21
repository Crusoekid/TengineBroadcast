cmake_minimum_required(VERSION 3.10)
project(FaceDetect)
set(target FaceDetect)

set (CMAKE_CXX_FLAGS  " -std=c++11 -O3 -Wall -fno-stack-protector -fopenmp")

find_package(OpenCV REQUIRED)
include_directories(${CMAKE_CURRENT_LIST_DIR}/tengine/include/)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src/)

link_directories(tengine/libs)


add_library(tengine SHARED IMPORTED)
set_target_properties(tengine PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/tengine/lib/libtengine-lite.so)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR}/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR}/bin/)

add_executable(${target} src/main.cpp src/UltraFace.cpp)
target_link_libraries(${target} tengine ${OpenCV_LIBS})