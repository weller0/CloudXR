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

#### 添加 OpenXR Loader 库 ####
set(OPENXR_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/openxr/openxr)
add_library(libopenxr_loader SHARED IMPORTED)
set_target_properties(libopenxr_loader PROPERTIES IMPORTED_LOCATION
        ${OPENXR_SDK_ROOT}/libs/arm64-v8a/libopenxr_loader.so)
include_directories(${OPENXR_SDK_ROOT}/include)
add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)

# 用于单独调试OpenXR和CloudXR
add_definitions(-DXR_USE_OPENXR)
add_definitions(-DXR_USE_CLOUDXR)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cloudxrlib-jni
        SHARED
        openxr/OpenXR.cpp
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
        libPoco
        libopenxr_loader)