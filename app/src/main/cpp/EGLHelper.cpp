#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <memory>
#include "EGLHelper.h"
#include "log.h"

namespace ssnwt {
    bool EGLHelper::initialize(ANativeWindow *window) {
        if (mContext)
            return true; // already initialized

        const EGLint attribs[] = {
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_DEPTH_SIZE, 0,
                EGL_NONE
        };
        mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major, minor;
        eglInitialize(mDisplay, &major, &minor);
        ALOGD("[EGLHelper]EGL version %d.%d", major, minor);
        EGLConfig configs[512];
        EGLint numConfigs = 0;
        eglGetConfigs(mDisplay, configs, 512, &numConfigs);
        EGLConfig config = 0;
        for (int i = 0; i < numConfigs; i++) {
            EGLint value = 0;
            eglGetConfigAttrib(mDisplay, configs[i], EGL_RENDERABLE_TYPE, &value);
            if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
                continue;
            }
            eglGetConfigAttrib(mDisplay, configs[i], EGL_SURFACE_TYPE, &value);
            if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) !=
                (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
                continue;
            }
            int j = 0;
            for (; attribs[j] != EGL_NONE; j += 2) {
                eglGetConfigAttrib(mDisplay, configs[i], attribs[j], &value);
                if (value != attribs[j + 1]) {
                    break;
                }
            }
            if (attribs[j] == EGL_NONE) {
                config = configs[i];
                ALOGD("[EGLHelper]found EGL config");
                break;
            }
        }

        if (config == 0) {
            ALOGE("[EGLHelper]failed to find EGL config");
            return false;
        }
        mSurface = eglCreateWindowSurface(mDisplay, config, window, NULL);
        if (mSurface == EGL_NO_SURFACE) {
            ALOGE("[EGLHelper]eglCreateWindowSurface failed");
            return false;
        }
        EGLint contextAttribs[] = {
                EGL_CONTEXT_CLIENT_VERSION, 3,
                EGL_NONE
        };
        mContext = eglCreateContext(mDisplay, config, NULL, contextAttribs);
        if (mContext == EGL_NO_CONTEXT) {
            ALOGE("[EGLHelper]eglCreateContext failed");
            return false;
        }
        if (eglMakeCurrent(mDisplay, mSurface, mSurface, mContext) == EGL_FALSE) {
            ALOGE("[EGLHelper]eglMakeCurrent failed");
            return false;
        }

        eglQuerySurface(mDisplay, mSurface, EGL_WIDTH, &mWidth);
        eglQuerySurface(mDisplay, mSurface, EGL_HEIGHT, &mHeight);
        ALOGD("[EGLHelper]window(%p), size(%d, %d)", window, mWidth, mHeight);
        return true;
    }

    void EGLHelper::release() {
        eglMakeCurrent(mDisplay, nullptr, nullptr, nullptr);
        if (mContext) {
            eglDestroyContext(mDisplay, mContext);
            mContext = nullptr;
        }

        if (mSurface) {
            eglDestroySurface(mDisplay, mSurface);
            mSurface = nullptr;
        }

        if (mDisplay) {
            eglTerminate(mDisplay);
            mDisplay = nullptr;
        }
    }

    void EGLHelper::swapBuffer() {
        eglSwapBuffers(mDisplay, mSurface);
    }
}