#ifndef CLIENT_APP_OVR_EGLHELPER_H
#define CLIENT_APP_OVR_EGLHELPER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace ssnwt {
    class EGLHelper {
    public:
        bool initialize(ANativeWindow *window);

        void release();

        bool isValid() { return mContext != 0 && mSurface != 0; }

        EGLint getWidth() { return mWidth; }

        EGLint getHeight() { return mHeight; }

    private:
        EGLDisplay mDisplay = 0;
        EGLContext mContext = 0;
        EGLSurface mSurface = 0;
        EGLint mWidth = 0, mHeight = 0;
    };
}
#endif //CLIENT_APP_OVR_EGLHELPER_H
