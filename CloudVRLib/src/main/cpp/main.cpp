#include <android/native_window_jni.h>
#include <thread>
#include <cstring>
#include "log.h"
#include "EGLHelper.h"
#include "GraphicRender.h"
#include "FpsCounter.h"

#ifdef XR_USE_CLOUDXR

#include "nvidia/CloudXR.h"
#include "matrix.h"

#endif // XR_USE_CLOUDXR

#ifdef XR_USE_OPENXR

#include "openxr/OpenXR.h"
#include "matrix.h"

#endif// XR_USE_OPENXR

struct HMDInfo {
    char *cmdLine;
    uint32_t fovX;
    uint32_t fovY;
    uint32_t fps;
    float ipd;
    float predOffset;
};

ssnwt::GraphicRender *pGraphicRender = nullptr;
#ifdef XR_USE_OPENXR
ssnwt::OpenXR *pOpenXr = nullptr;
#endif // XR_USE_OPENXR
ANativeWindow *pNativeWindow = nullptr;
int32_t mSurfaceWidth = 0, mSurfaceHeight = 0;
struct HMDInfo hmdInfo{};
bool quit = false;
bool paused = true;
bool isSurfaceChanged = false;
uint32_t lastBooleanComps = 0;

extern "C" {
#ifdef XR_USE_OPENXR
matrix4f getTransformFromPose(const XrPosef pose) {
    const matrix4f rotation = createFromQuaternion(pose.orientation.x, pose.orientation.y,
                                                   pose.orientation.z, pose.orientation.w);
    const matrix4f translation = createTranslation(pose.position.x, pose.position.y,
                                                   pose.position.z);
    return multiply(&translation, &rotation);
}

cxrMatrix34 cxrConvert(const matrix4f &m) {
    cxrMatrix34 out{};
    // The matrices are compatible so doing a memcpy() here
    //  noting that we are a [3][4] and ovr uses [4][4]
    memcpy(&out, &m, sizeof(out));
    return out;
}

void updateTrackingState(cxrVRTrackingState *trackingState) {
    XrSpaceLocation location;
    cxrVRTrackingState TrackingState = {};
    // hmd
    if (pOpenXr->getLocateSpace(&location) == XR_SUCCESS) {
        TrackingState.hmd.pose.deviceToAbsoluteTracking =
                cxrConvert(getTransformFromPose(location.pose));
        TrackingState.hmd.pose.poseIsValid = cxrTrue;
        TrackingState.hmd.pose.deviceIsConnected = cxrTrue;
        TrackingState.hmd.pose.trackingResult = cxrTrackingResult_Running_OK;
    }
    // left/right controller
    if (pOpenXr->syncAction() == XR_SUCCESS) {
        for (uint32_t eye = 0; eye < CXR_NUM_CONTROLLERS; eye++) {
            if (pOpenXr->getControllerSpace(eye, &location) == XR_SUCCESS) {
                TrackingState.controller[eye].pose.deviceToAbsoluteTracking =
                        cxrConvert(getTransformFromPose(location.pose));

                TrackingState.controller[eye].booleanComps = lastBooleanComps;
                pOpenXr->getControllerState(eye, &TrackingState.controller[eye].booleanComps,
                                            &TrackingState.controller[eye].booleanCompsChanged,
                                            TrackingState.controller[eye].scalarComps);
                lastBooleanComps = TrackingState.controller[eye].booleanComps;
                TrackingState.controller[eye].pose.poseIsValid = cxrTrue;
                TrackingState.controller[eye].pose.deviceIsConnected = cxrTrue;
                TrackingState.controller[eye].pose.trackingResult = cxrTrackingResult_Running_OK;
            }
        }
    }
    if (trackingState != nullptr) {
        *trackingState = TrackingState;
    }
}

void onDraw(uint32_t eye) {
    if (pGraphicRender) pGraphicRender->draw(eye);
}
#elif XR_USE_CLOUDXR
float angleY = 0;
matrix4f getTransformFromPose() {
    angleY += 0.001;
    if (angleY > 2 * M_PI) angleY = 0;
    const matrix4f rotation = rotationY(angleY);
    const matrix4f translation = createTranslation(0, 0, 0);
    return multiply(&translation, &rotation);
}

cxrMatrix34 cxrConvert(const matrix4f &m) {
    cxrMatrix34 out{};
    // The matrices are compatible so doing a memcpy() here
    //  noting that we are a [3][4] and ovr uses [4][4]
    memcpy(&out, &m, sizeof(out));
    return out;
}
void updateTrackingState(cxrVRTrackingState *trackingState) {
    cxrVRTrackingState TrackingState = {};
    TrackingState.hmd.pose.deviceToAbsoluteTracking =
            cxrConvert(getTransformFromPose());
    TrackingState.hmd.pose.poseIsValid = cxrTrue;
    TrackingState.hmd.pose.deviceIsConnected = cxrTrue;
    TrackingState.hmd.pose.trackingResult = cxrTrackingResult_Running_OK;
    if (trackingState != nullptr) {
        *trackingState = TrackingState;
    }
}
#endif // XR_USE_OPENXR

void gl_main() {
    ALOGD("[main]+++++ Enter gl thread +++++");
    ssnwt::EGLHelper eglHelper{};
    eglHelper.initialize();
#ifdef XR_USE_OPENXR
    eglHelper.setSurface();
    pOpenXr->initialize(onDraw);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    eglHelper.setSurface(pNativeWindow);
#endif

#ifdef XR_USE_CLOUDXR
    ssnwt::CloudXR cloudXr{};
    cxrFramesLatched framesLatched;
#endif // XR_USE_CLOUDXR
    while (!quit && pGraphicRender) {
        if (paused || pNativeWindow == nullptr) {
            ALOGW("[main]Already paused, so do not render.");
#ifdef XR_USE_CLOUDXR
            cloudXr.disconnect();
#endif // XR_USE_CLOUDXR
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        if (isSurfaceChanged) {
            isSurfaceChanged = false;
#ifdef XR_USE_CLOUDXR
            cloudXr.connect(hmdInfo.cmdLine, mSurfaceWidth, mSurfaceHeight,
                            hmdInfo.fovX, hmdInfo.fovY,
                            hmdInfo.ipd, hmdInfo.predOffset, 1, 1, hmdInfo.fps,
                            updateTrackingState, nullptr, nullptr);
#endif // XR_USE_CLOUDXR
#ifdef XR_USE_OPENXR
            pOpenXr->setSurface(pNativeWindow);
#endif // XR_USE_OPENXR
            eglHelper.setSurface(pNativeWindow);
            ALOGD("[main]window (%d, %d)", mSurfaceWidth, mSurfaceHeight);
            pGraphicRender->initialize(mSurfaceWidth, mSurfaceHeight);
        }

        if (!eglHelper.isValid()) {
            ALOGW("[main]EGL is not valid, so do not render.");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        ssnwt::frameStart();
        ssnwt::GraphicRender::clear();
#ifdef XR_USE_CLOUDXR
        bool cloudxrPrepared = cloudXr.preRender(&framesLatched) == cxrError_Success;
#endif // XR_USE_CLOUDXR
        for (int32_t eye = 0; eye < 2; eye++) {
            if (pGraphicRender->setupFrameBuffer(eye)) {
#ifdef XR_USE_CLOUDXR
                if (cloudxrPrepared) cloudXr.render(eye, framesLatched);
#else
                ssnwt::GraphicRender::clear(eye);
#endif // XR_USE_CLOUDXR
            }
            ssnwt::GraphicRender::bindDefaultFrameBuffer();
#ifndef XR_USE_OPENXR
            if (pGraphicRender) pGraphicRender->draw(eye);
#endif // XR_USE_OPENXR
        }
#ifdef XR_USE_CLOUDXR
        if (cloudxrPrepared) cloudXr.postRender(framesLatched);
#endif // XR_USE_CLOUDXR

#ifdef XR_USE_OPENXR
        pOpenXr->render();
#else
        eglHelper.swapBuffers();
#endif // XR_USE_OPENXR

        ssnwt::frameEnd();
#ifdef XR_USE_CLOUDXR
        if (!cloudxrPrepared) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
#endif // XR_USE_CLOUDXR
    }
#ifdef XR_USE_CLOUDXR
    ALOGD("[main]cloudXr.disconnect()");
    cloudXr.disconnect();
#endif // XR_USE_CLOUDXR

#ifdef XR_USE_OPENXR
    if (pOpenXr) {
        pOpenXr->release();
        pOpenXr = nullptr;
    }
#endif
    if (pGraphicRender) {
        pGraphicRender->release();
        pGraphicRender = nullptr;
    }
    eglHelper.release();
    ALOGD("[main]----- exit gl thread -----");
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_initialize(JNIEnv *env, jclass thiz,
                                          jobject activity, jstring cmd, jint fovX, jint fovY,
                                          jint fps, jfloat ipd, jfloat predOffset) {
    JavaVM *vm;
    env->GetJavaVM(&vm);
    hmdInfo.cmdLine = strdup(env->GetStringUTFChars(cmd, nullptr));
    hmdInfo.fovX = fovX;
    hmdInfo.fovY = fovY;
    hmdInfo.fps = fps;
    hmdInfo.ipd = ipd;
    hmdInfo.predOffset = predOffset;
#ifdef XR_USE_OPENXR
    pOpenXr = new ssnwt::OpenXR(vm, activity);
#endif
    pGraphicRender = new ssnwt::GraphicRender();
    std::thread mainThread(gl_main);
    mainThread.detach();
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_setSurface(JNIEnv *env, jclass clazz, jobject surface,
                                          jint width, jint height) {
    isSurfaceChanged = true;
    mSurfaceWidth = width;
    mSurfaceHeight = height;
    pNativeWindow = ANativeWindow_fromSurface(env, surface);
    ALOGD("[main]setSurface gNativeWindow=%p", pNativeWindow);
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_resume(JNIEnv *env, jclass clazz) {
    ALOGD("[main]Resume");
    paused = false;
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_pause(JNIEnv *env, jclass clazz) {
    ALOGD("[main]Pause");
    pNativeWindow = nullptr;
    paused = true;
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_release(JNIEnv *env, jclass thiz) {
    ALOGD("[main]Release");
    quit = true;
}
}