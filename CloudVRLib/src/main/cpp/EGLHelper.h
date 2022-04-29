#ifndef CLIENT_APP_OVR_EGLHELPER_H
#define CLIENT_APP_OVR_EGLHELPER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace ssnwt {
    class EGLHelper {
    public:
        bool initialize();

        bool setSurface(ANativeWindow *window);

        bool setSurface();

        void release();

        bool isValid() { return mContext != 0 && mSurface != 0; }

        EGLDisplay getDisplay() { return mDisplay; }

        EGLContext getContext() { return mContext; }

        void swapBuffers() { eglSwapBuffers(mDisplay, mSurface); }

    private:
        EGLDisplay mDisplay = 0;
        EGLConfig mConfig = 0;
        EGLContext mContext = 0;
        EGLSurface mSurface = 0;
        EGLint mWidth = 0, mHeight = 0;
    };
}
#endif //CLIENT_APP_OVR_EGLHELPER_H
