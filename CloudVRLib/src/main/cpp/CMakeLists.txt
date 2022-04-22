cmake_minimum_required(VERSION 3.4.1)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cloudxrlib-jni
        SHARED
        EGLHelper.cpp
        GraphicRender.cpp
        main.cpp)

target_link_libraries(cloudxrlib-jni
        android
        log
        EGL
        GLESv3)