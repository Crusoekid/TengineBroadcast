cmake_minimum_required(VERSION 3.10)
project(FaceDetect)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fstrict-aliasing -fexceptions -frtti -ffast-math -flax-vector-conversions -O1")


set(distribution_DIR ${CMAKE_SOURCE_DIR}/../../../libs/${ANDROID_ABI})

include_directories(include)

add_library(tengine SHARED IMPORTED)
set_target_properties(tengine PROPERTIES IMPORTED_LOCATION ${distribution_DIR}/libtengine-lite.so)

add_library(opencvl SHARED IMPORTED)
set_target_properties(opencvl PROPERTIES IMPORTED_LOCATION ${distribution_DIR}/libopencv_java3.so)

find_library(
        log-lib
        log )

add_library(FaceDetect SHARED UltraFace.cpp FaceDetector.cpp)


target_link_libraries(FaceDetect tengine opencvl ${log-lib} z jnigraphics)