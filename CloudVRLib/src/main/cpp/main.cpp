#include <android/native_window_jni.h>
#include <thread>
#include <string.h>
#include "log.h"
#include "EGLHelper.h"
#include "GraphicRender.h"
#include "nvidia/CloudXR.h"
#include "openxr/OpenXR.h"

ssnwt::EGLHelper eglHelper;
ssnwt::GraphicRender graphicRender;
ssnwt::CloudXR cloudXr;
ssnwt::OpenXR *pOpenXr;
char *cmdLine;
ANativeWindow *gNativeWindow = nullptr;
bool quit = false;

extern "C" {
typedef struct ovrMatrix4f_ {
    float M[4][4];
} ovrMatrix4f;

ovrMatrix4f multiply(const ovrMatrix4f *a, const ovrMatrix4f *b) {
    ovrMatrix4f out;
    out.M[0][0] = a->M[0][0] * b->M[0][0] + a->M[0][1] * b->M[1][0] + a->M[0][2] * b->M[2][0] +
                  a->M[0][3] * b->M[3][0];
    out.M[1][0] = a->M[1][0] * b->M[0][0] + a->M[1][1] * b->M[1][0] + a->M[1][2] * b->M[2][0] +
                  a->M[1][3] * b->M[3][0];
    out.M[2][0] = a->M[2][0] * b->M[0][0] + a->M[2][1] * b->M[1][0] + a->M[2][2] * b->M[2][0] +
                  a->M[2][3] * b->M[3][0];
    out.M[3][0] = a->M[3][0] * b->M[0][0] + a->M[3][1] * b->M[1][0] + a->M[3][2] * b->M[2][0] +
                  a->M[3][3] * b->M[3][0];

    out.M[0][1] = a->M[0][0] * b->M[0][1] + a->M[0][1] * b->M[1][1] + a->M[0][2] * b->M[2][1] +
                  a->M[0][3] * b->M[3][1];
    out.M[1][1] = a->M[1][0] * b->M[0][1] + a->M[1][1] * b->M[1][1] + a->M[1][2] * b->M[2][1] +
                  a->M[1][3] * b->M[3][1];
    out.M[2][1] = a->M[2][0] * b->M[0][1] + a->M[2][1] * b->M[1][1] + a->M[2][2] * b->M[2][1] +
                  a->M[2][3] * b->M[3][1];
    out.M[3][1] = a->M[3][0] * b->M[0][1] + a->M[3][1] * b->M[1][1] + a->M[3][2] * b->M[2][1] +
                  a->M[3][3] * b->M[3][1];

    out.M[0][2] = a->M[0][0] * b->M[0][2] + a->M[0][1] * b->M[1][2] + a->M[0][2] * b->M[2][2] +
                  a->M[0][3] * b->M[3][2];
    out.M[1][2] = a->M[1][0] * b->M[0][2] + a->M[1][1] * b->M[1][2] + a->M[1][2] * b->M[2][2] +
                  a->M[1][3] * b->M[3][2];
    out.M[2][2] = a->M[2][0] * b->M[0][2] + a->M[2][1] * b->M[1][2] + a->M[2][2] * b->M[2][2] +
                  a->M[2][3] * b->M[3][2];
    out.M[3][2] = a->M[3][0] * b->M[0][2] + a->M[3][1] * b->M[1][2] + a->M[3][2] * b->M[2][2] +
                  a->M[3][3] * b->M[3][2];

    out.M[0][3] = a->M[0][0] * b->M[0][3] + a->M[0][1] * b->M[1][3] + a->M[0][2] * b->M[2][3] +
                  a->M[0][3] * b->M[3][3];
    out.M[1][3] = a->M[1][0] * b->M[0][3] + a->M[1][1] * b->M[1][3] + a->M[1][2] * b->M[2][3] +
                  a->M[1][3] * b->M[3][3];
    out.M[2][3] = a->M[2][0] * b->M[0][3] + a->M[2][1] * b->M[1][3] + a->M[2][2] * b->M[2][3] +
                  a->M[2][3] * b->M[3][3];
    out.M[3][3] = a->M[3][0] * b->M[0][3] + a->M[3][1] * b->M[1][3] + a->M[3][2] * b->M[2][3] +
                  a->M[3][3] * b->M[3][3];
    return out;
}

ovrMatrix4f createFromQuaternion(const XrQuaternionf q) {
    const float ww = q.w * q.w;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;

    ovrMatrix4f out;
    out.M[0][0] = ww + xx - yy - zz;
    out.M[0][1] = 2 * (q.x * q.y - q.w * q.z);
    out.M[0][2] = 2 * (q.x * q.z + q.w * q.y);
    out.M[0][3] = 0;

    out.M[1][0] = 2 * (q.x * q.y + q.w * q.z);
    out.M[1][1] = ww - xx + yy - zz;
    out.M[1][2] = 2 * (q.y * q.z - q.w * q.x);
    out.M[1][3] = 0;

    out.M[2][0] = 2 * (q.x * q.z - q.w * q.y);
    out.M[2][1] = 2 * (q.y * q.z + q.w * q.x);
    out.M[2][2] = ww - xx - yy + zz;
    out.M[2][3] = 0;

    out.M[3][0] = 0;
    out.M[3][1] = 0;
    out.M[3][2] = 0;
    out.M[3][3] = 1;
    return out;
}

ovrMatrix4f createTranslation(const float x, const float y, const float z) {
    ovrMatrix4f out;
    out.M[0][0] = 1.0f;
    out.M[0][1] = 0.0f;
    out.M[0][2] = 0.0f;
    out.M[0][3] = x;
    out.M[1][0] = 0.0f;
    out.M[1][1] = 1.0f;
    out.M[1][2] = 0.0f;
    out.M[1][3] = y;
    out.M[2][0] = 0.0f;
    out.M[2][1] = 0.0f;
    out.M[2][2] = 1.0f;
    out.M[2][3] = z;
    out.M[3][0] = 0.0f;
    out.M[3][1] = 0.0f;
    out.M[3][2] = 0.0f;
    out.M[3][3] = 1.0f;
    return out;
}

ovrMatrix4f getTransformFromPose(const XrPosef pose) {
    const ovrMatrix4f rotation = createFromQuaternion(pose.orientation);
    const ovrMatrix4f translation = createTranslation(pose.position.x, pose.position.y,
                                                      pose.position.z);
    return multiply(&translation, &rotation);
}

cxrMatrix34 cxrConvert(const ovrMatrix4f &m) {
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
                ALOGD("getControllerSpace (%0.3f, %0.3f, %0.3f, %0.3f), (%0.3f, %0.3f, %0.3f)",
                      location.pose.orientation.x, location.pose.orientation.y,
                      location.pose.orientation.z, location.pose.orientation.w,
                      location.pose.position.x, location.pose.position.y, location.pose.position.z);
                TrackingState.controller[eye].pose.deviceToAbsoluteTracking =
                        cxrConvert(getTransformFromPose(location.pose));
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
    graphicRender.draw(eye);
}

void gl_main() {
    ALOGD("+++++ Enter gl thread +++++");
    eglHelper.initialize(gNativeWindow);
    ALOGD("window (%d, %d)", eglHelper.getWidth(), eglHelper.getHeight());
    graphicRender.initialize(eglHelper.getWidth(), eglHelper.getHeight());

    cloudXr.connect(cmdLine, updateTrackingState);
    pOpenXr->initialize(gNativeWindow, onDraw);
    cxrFramesLatched framesLatched;
    while (!quit) {
        graphicRender.clear();
        bool cloudxrPrepared = cloudXr.preRender(&framesLatched) == cxrError_Success;
        for (uint32_t eye = 0; eye < 2; eye++) {
            if (cloudxrPrepared && graphicRender.setupFrameBuffer(eye)) {
                cloudXr.render(eye, framesLatched);
            }
            graphicRender.bindDefaultFrameBuffer();
        }
        if (cloudxrPrepared) cloudXr.postRender(framesLatched);
        pOpenXr->render();
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
    }
    cloudXr.disconnect();
    if (pOpenXr) {
        pOpenXr->release();
        pOpenXr = nullptr;
    }
    eglHelper.release();
    ALOGD("----- exit gl thread -----");
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudVRActivity_initialize(JNIEnv *env, jobject thiz,
                                                  jobject
                                                  activity, jobject surface,
                                                  jstring
                                                  cmd) {
    gNativeWindow = ANativeWindow_fromSurface(env, surface);
    cmdLine = strdup(env->GetStringUTFChars(cmd, 0));
    ALOGD ("Java_com_ssnwt_cloudvr_CloudVRActivity_initialize gNativeWindow=%p", gNativeWindow);
    JavaVM *vm;
    env->GetJavaVM(&vm);
    pOpenXr = new ssnwt::OpenXR(vm, activity);
    std::thread mainThread(gl_main);
    mainThread.detach();
}
JNIEXPORT void JNICALL
Java_com_ssnwt_cloudvr_CloudVRActivity_release(JNIEnv
                                               *env,
                                               jobject thiz
) {
    quit = true;
}
}