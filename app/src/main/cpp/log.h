#ifndef CLIENT_APP_OVR_LOG_H
#define CLIENT_APP_OVR_LOG_H

#include <android/log.h>

#define DEBUG_LOGGING
#define OVR_LOG_TAG "CloudXR_Jni"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARNING, OVR_LOG_TAG, __VA_ARGS__)
#ifdef DEBUG_LOGGING
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#define ALOGD(...)
#endif

#endif //CLIENT_APP_OVR_LOG_H
