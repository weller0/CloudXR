cmake_minimum_required(VERSION 3.4.1)

#### 添加 CloudXRClien 库 ####
set(CLOUDXR_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/nvidia/cloudxr)
add_library(libc++_shared SHARED IMPORTED)
set_target_properties(libc++_shared PROPERTIES IMPORTED_LOCATION
        ${CLOUDXR_SDK_ROOT}/libs/arm64-v8a/libc++_shared.so)
add_library(libCloudXRClient SHARED IMPORTED)
set_target_properties(libCloudXRClient PROPERTIES IMPORTED_LOCATION
        ${CLOUDXR_SDK_ROOT}/libs/arm64-v8a/libCloudXRClient.so)
add_library(libgrid SHARED IMPORTED)
set_target_properties(libgrid PROPERTIES IMPORTED_LOCATION
        ${CLOUDXR_SDK_ROOT}/libs/arm64-v8a/libgrid.so)
add_library(libGsAudioWebRTC SHARED IMPORTED)
set_target_properties(libGsAudioWebRTC PROPERTIES IMPORTED_LOCATION
        ${CLOUDXR_SDK_ROOT}/libs/arm64-v8a/libGsAudioWebRTC.so)
add_library(libPoco SHARED IMPORTED)
set_target_properties(libPoco PROPERTIES IMPORTED_LOCATION
        ${CLOUDXR_SDK_ROOT}/libs/arm64-v8a/libPoco.so)
include_directories(${CLOUDXR_SDK_ROOT}/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cloudxrlib-jni
        SHARED
        nvidia/AudioRender.cpp
        nvidia/CloudXR.cpp
        EGLHelper.cpp
        GraphicRender.cpp
        main.cpp)

target_link_libraries(cloudxrlib-jni
        android
        aaudio
        log
        EGL
        GLESv3
        libc++_shared
        libCloudXRClient
        libgrid
        libGsAudioWebRTC
        libPoco)