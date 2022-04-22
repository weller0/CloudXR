#include <android/native_window_jni.h>
#include <jni.h>
#include <thread>
#include "log.h"
#include "EGLHelper.h"
#include "GraphicRender.h"
#include "nvidia/CloudXR.h"

ssnwt::EGLHelper eglHelper;
ssnwt::GraphicRender graphicRender;
ssnwt::CloudXR cloudXr;
ANativeWindow *gNativeWindow = nullptr;
bool quit = false;

extern "C" {
void gl_main() {
    ALOGD("+++++ Enter gl thread +++++");
    eglHelper.initialize(gNativeWindow);
    graphicRender.initialize(eglHelper.getWidth(), eglHelper.getHeight());

    cloudXr.connect("-s 192.168.1.106",
                    graphicRender.getTextureIdL(),
                    graphicRender.getTextureIdR());
    while (!quit) {
        ALOGD("draw...");
        graphicRender.clear();
        for (int eye = 0; eye < 2; eye++) {
            if (graphicRender.setupFrameBuffer(eye, eglHelper.getWidth() / 2,
                                               eglHelper.getHeight())) {
                graphicRender.drawFbo(eye);
            }
            graphicRender.bindDefaultFrameBuffer();
            graphicRender.draw(eye);
        }
        eglHelper.swapBuffer();
    }
    eglHelper.release();
    ALOGD("----- exit gl thread -----");
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudVRActivity_initialize(JNIEnv *env, jobject thiz,
                                                  jobject activity, jobject surface,
                                                  jstring cmdLine) {
    gNativeWindow = ANativeWindow_fromSurface(env, surface);
    ALOGD("Java_com_ssnwt_cloudvr_CloudVRActivity_initialize gNativeWindow=%p", gNativeWindow);
    std::thread mainThread(gl_main);
    mainThread.detach();
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudVRActivity_release(JNIEnv *env, jobject thiz) {
    quit = true;
}
}