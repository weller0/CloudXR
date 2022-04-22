cmake_minimum_required(VERSION 3.4.1)

# load native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)
# Export ANativeActivity_onCreate(),
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

# load openxr_loader as a shared lib
add_library(openxr_loader_libs SHARED IMPORTED)
set_target_properties(openxr_loader_libs
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/openxr/libs/${ANDROID_ABI}/libopenxr_loader.so)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/openxr)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/openxr/include)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wall -Werror")

add_definitions(-DXR_USE_PLATFORM_ANDROID)
add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)


include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(cloudxr-native
        SHARED
        GraphicRender.cpp
        openxr/gfxwrapper_opengl.c
        openxr/graphicsplugin_opengles.cpp
        openxr/logger.cpp
        openxr/platformplugin_android.cpp
        openxr/openxr_program.cpp
        main.cpp)

target_link_libraries(cloudxr-native
        native_app_glue
        openxr_loader_libs
        android
        log
        EGL
        GLESv3)