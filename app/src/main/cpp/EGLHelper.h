#ifndef CLOUDXR_EGLHELPER_H
#define CLOUDXR_EGLHELPER_H

#include <EGL/egl.h>

namespace ssnwt {
    class EGLHelper {
    public:
        int initializeEGL(ANativeWindow *window);

        int releaseEGL();

        int swapBuffer();

        int getWidth() { return width; }

        int getHeight() { return height; }

        EGLContext getEGLContext() { return context; }

        EGLDisplay getEGLDisplay() { return display; }

        EGLSurface getEGLSurface() { return surface; }

        bool isReady() { return ready; }

    private:
        bool ready = false;
        EGLint width = 0;
        EGLint height = 0;
        EGLContext context = EGL_NO_CONTEXT;
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
    };
}


#endif //CLOUDXR_EGLHELPER_H
