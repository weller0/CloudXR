#include "EGLHelper.h"
#include "log.h"

namespace ssnwt {
    int EGLHelper::initializeEGL(ANativeWindow *window) {
        const EGLint attribs[] = {
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_NONE
        };
        EGLint format;
        EGLint numConfigs;

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        eglInitialize(display, nullptr, nullptr);

        /* Here, the application chooses the configuration it desires.
         * find the best match if possible, otherwise use the very first one
         */
        eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);
        auto supportedConfigs = new EGLConfig[numConfigs];
        eglChooseConfig(display, attribs, supportedConfigs, numConfigs, &numConfigs);
        auto i = 0;
        for (; i < numConfigs; i++) {
            auto &cfg = supportedConfigs[i];
            EGLint r, g, b, d;
            if (eglGetConfigAttrib(display, cfg, EGL_RED_SIZE, &r) &&
                eglGetConfigAttrib(display, cfg, EGL_GREEN_SIZE, &g) &&
                eglGetConfigAttrib(display, cfg, EGL_BLUE_SIZE, &b) &&
                eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE, &d) &&
                r == 8 && g == 8 && b == 8 && d == 0) {

                config = supportedConfigs[i];
                break;
            }
        }
        if (i == numConfigs) {
            config = supportedConfigs[0];
        }

        if (config == nullptr) {
            ALOGE("Unable to initialize EGLConfig");
            releaseEGL();
            return -1;
        }

        /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
         * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
         * As soon as we picked a EGLConfig, we can safely reconfigure the
         * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
        surface = eglCreateWindowSurface(display, config, window, nullptr);
        context = eglCreateContext(display, config, nullptr, nullptr);

        if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
            ALOGE("Unable to eglMakeCurrent");
            releaseEGL();
            return -1;
        }

        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        ALOGD("initializeEGL width:%d, height:%d", width, height);
        ready = true;
        return 0;
    }

    int EGLHelper::releaseEGL() {
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) {
                eglDestroyContext(display, context);
            }
            if (surface != EGL_NO_SURFACE) {
                eglDestroySurface(display, surface);
            }
            eglTerminate(display);
        }
        config = nullptr;
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
        ready = false;
        ALOGD("releaseEGL");
        return 0;
    }

    int EGLHelper::swapBuffer() {
        if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
            eglSwapBuffers(display, surface);
        }
        return 0;
    }
}