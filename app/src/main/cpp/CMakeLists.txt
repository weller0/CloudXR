cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

# Export ANativeActivity_onCreate(),
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall -Werror")

add_library(cloudxr-native
        SHARED
        GraphicRender.cpp
        EGLHelper.cpp
        main.cpp)

target_link_libraries(cloudxr-native
        native_app_glue
        android
        log
        EGL
        GLESv3)