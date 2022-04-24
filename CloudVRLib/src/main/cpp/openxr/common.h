//
// Created by Richard on 2022-04-12.
//

#ifndef CUBEWORLD_COMMON_H
#define CUBEWORLD_COMMON_H

#include "OpenXR.h"
#include <string>
#include "log.h"

static void
XR_CheckErrors(XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        if (failOnError) {
            ALOGV("OpenXR error: %s\n", function);
        } else {
            ALOGE("OpenXR error: %s\n", function);
        }
    }
}

#if defined(BUILD_DEBUG)
#define OPENXR_CHECK(func) XR_CheckErrors(func, #func, true);
#else
#define OPENXR_CHECK(func) XR_CheckErrors(func, #func, false)
#endif

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)

#define CHECK(exp)                                      \
    do{                                                   \
        if (!(exp)) {                                   \
            ALOGE("Check failed,%s,%s", #exp, FILE_AND_LINE); \
        }                                               \
    }while(false)

#define CHECK_MSG(exp, msg)                  \
    {                                        \
        if (!(exp)) {                        \
            ALOGE("%s,%s,%s",msg, #exp, FILE_AND_LINE); \
        }                                    \
    }

#define CHECK_XRRESULT(result,msg) \
    {                              \
        if(result != XR_SUCCESS){ \
            ALOGE("%s error",msg);\
        }                          \
    }

#if !defined(XR_USE_PLATFORM_WIN32)
#define strcpy_s(dest, source) strncpy((dest), (source), sizeof(dest))
#endif

#endif //CUBEWORLD_COMMON_H
