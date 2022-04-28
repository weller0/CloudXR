#include <android/native_window_jni.h>
#include <thread>
#include <cstring>
#include "log.h"
#include "EGLHelper.h"
#include "GraphicRender.h"
#include "nvidia/CloudXR.h"
#include "openxr/OpenXR.h"
#include "matrix.h"

ssnwt::GraphicRender *pGraphicRender = nullptr;
ssnwt::OpenXR *pOpenXr = nullptr;
char *cmdLine;
ANativeWindow *gNativeWindow = nullptr;
bool quit = false;

extern "C" {
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

                if (!pOpenXr->getControllerState(eye, &TrackingState.controller[eye].booleanComps,
                                                 &TrackingState.controller[eye].booleanCompsChanged,
                                                 TrackingState.controller[eye].scalarComps)) {
//                    ALOGD("getControllerSpace[%d] (%0.3f, %0.3f, %0.3f, %0.3f), key:%d, %d", eye,
//                          TrackingState.controller[eye].scalarComps[cxrAnalog_Grip],
//                          TrackingState.controller[eye].scalarComps[cxrAnalog_Trigger],
//                          TrackingState.controller[eye].scalarComps[cxrAnalog_TouchpadX],
//                          TrackingState.controller[eye].scalarComps[cxrAnalog_TouchpadY],
//                          TrackingState.controller[eye].booleanComps,
//                          TrackingState.controller[eye].booleanCompsChanged);
                }

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

void gl_main() {
    ALOGD("+++++ Enter gl thread +++++");
    ssnwt::EGLHelper eglHelper{};
    ssnwt::CloudXR cloudXr{};
    eglHelper.initialize(gNativeWindow);
    ALOGD("window (%d, %d)", eglHelper.getWidth(), eglHelper.getHeight());
    pGraphicRender->initialize(eglHelper.getWidth(), eglHelper.getHeight());
    cloudXr.connect(cmdLine, updateTrackingState, nullptr, nullptr);
    pOpenXr->initialize(gNativeWindow, onDraw);
    cxrFramesLatched framesLatched;
    while (!quit && pGraphicRender && pOpenXr) {
        ssnwt::GraphicRender::clear();
        bool cloudxrPrepared = cloudXr.preRender(&framesLatched) == cxrError_Success;
        for (int32_t eye = 0; eye < 2; eye++) {
            if (cloudxrPrepared && pGraphicRender->setupFrameBuffer(eye)) {
                cloudXr.render(eye, framesLatched);
            }
            ssnwt::GraphicRender::bindDefaultFrameBuffer();
        }
        if (cloudxrPrepared) cloudXr.postRender(framesLatched);
        pOpenXr->render();
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
    }
    ALOGD("cloudXr.disconnect()");
    cloudXr.disconnect();
    if (pOpenXr) {
        pOpenXr->release();
        pOpenXr = nullptr;
    }
    if (pGraphicRender) {
        pGraphicRender->release();
        pGraphicRender = nullptr;
    }
    eglHelper.release();
    ALOGD("----- exit gl thread -----");
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_initialize(JNIEnv *env, jclass thiz,
                                          jobject activity, jobject surface,
                                          jstring cmd) {
    gNativeWindow = ANativeWindow_fromSurface(env, surface);
    cmdLine = strdup(env->GetStringUTFChars(cmd, nullptr));
    ALOGD ("Java_com_ssnwt_cloudvr_CloudVRActivity_initialize gNativeWindow=%p", gNativeWindow);
    JavaVM *vm;
    env->GetJavaVM(&vm);
    pOpenXr = new ssnwt::OpenXR(vm, activity);
    pGraphicRender = new ssnwt::GraphicRender();
    std::thread mainThread(gl_main);
    mainThread.detach();
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudXR_release(JNIEnv *env, jclass thiz) {
    quit = true;
}
}