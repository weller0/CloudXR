#include <EGL/egl.h>
#include "CloudXR.h"
#include "log.h"
#include "SensorUtils.h"

#define CASE(x) \
case x:     \
return #x

const char *ClientStateEnumToString(cxrClientState state) {
    switch (state) {
        CASE(cxrClientState_ReadyToConnect);
        CASE(cxrClientState_ConnectionAttemptInProgress);
        CASE(cxrClientState_ConnectionAttemptFailed);
        CASE(cxrClientState_StreamingSessionInProgress);
        CASE(cxrClientState_Disconnected);
        CASE(cxrClientState_Exiting);
        default:
            return "";
    }
}

const char *StateReasonEnumToString(cxrStateReason reason) {
    switch (reason) {
        CASE(cxrStateReason_HEVCUnsupported);
        CASE(cxrStateReason_VersionMismatch);
        CASE(cxrStateReason_DisabledFeature);
        CASE(cxrStateReason_RTSPCannotConnect);
        CASE(cxrStateReason_HolePunchFailed);
        CASE(cxrStateReason_NetworkError);
        CASE(cxrStateReason_AuthorizationFailed);
        CASE(cxrStateReason_DisconnectedExpected);
        CASE(cxrStateReason_DisconnectedUnexpected);
        default:
            return "";
    }
}

#undef CASE

namespace ssnwt {
    int CloudXR::connect(const char *cmdLine, int texEyeL, int texEyeR) {
        textures[0] = texEyeL;
        textures[1] = texEyeR;
        GOptions.ParseString(cmdLine);
        ALOGV("[CloudXR]mServerIP %s", GOptions.mServerIP.c_str());
        deviceDesc = getDeviceDesc();
        cxrClientCallbacks callbacks = getClientCallbacks();
        cxrGraphicsContext context{cxrGraphicsContext_GLES};
        context.egl.display = eglGetCurrentDisplay();
        context.egl.context = eglGetCurrentContext();

        if (context.egl.context == nullptr) {
            ALOGV("[CloudXR]Error, null context");
        }
        cxrReceiverDesc desc = {};
        desc.requestedVersion = CLOUDXR_VERSION_DWORD;
        desc.deviceDesc = deviceDesc;
        desc.clientCallbacks = callbacks;
        desc.clientContext = this;
        desc.shareContext = &context;
        desc.numStreams = 2;
        desc.receiverMode = cxrStreamingMode_XR;
        desc.debugFlags = GOptions.mDebugFlags;
        desc.logMaxSizeKB = CLOUDXR_LOG_MAX_DEFAULT;
        desc.logMaxAgeDays = CLOUDXR_LOG_MAX_DEFAULT;
        cxrError err = cxrCreateReceiver(&desc, &receiverHandle);
        if (err != cxrError_Success) {
            ALOGE("[CloudXR]Failed to create CloudXR receiver. Error %d, %s.", err,
                  cxrErrorString(err));
            return err;
        }
        ALOGV("[CloudXR]Receiver created!");
        err = cxrConnect(receiverHandle, GOptions.mServerIP.c_str(), connectionFlags);
        if (err != cxrError_Success) {
            ALOGE("[CloudXR]Failed to connect to CloudXR server at %s. Error %d, %s.",
                  GOptions.mServerIP.c_str(), (int) err, cxrErrorString(err));
            disconnect();
            return err;
        } else {
            ALOGV("[CloudXR]Receiver created for server: %s", GOptions.mServerIP.c_str());
        }
        delete pAudioRender;
        pAudioRender = new AudioRender();
        return cxrError_Success; //true
    }

    int CloudXR::disconnect() {
        if (receiverHandle) {
            cxrDestroyReceiver(receiverHandle);
            receiverHandle = nullptr;
        }
        delete pAudioRender;
        pAudioRender = nullptr;
        return cxrError_Success; //true
    }

    int CloudXR::render() {
        if (!receiverHandle) {
            ALOGE("[CloudXR]receiverHandle is null");
            return cxrError_Receiver_Invalid;
        }
        if (clientState != cxrClientState_StreamingSessionInProgress) {
            //ALOGE("receiverHandle is cxrError_Streamer_Not_Ready");
            return cxrError_Streamer_Not_Ready;
        }

        cxrFramesLatched framesLatched;
        const uint32_t timeoutMs = 500;
        cxrError frameErr = cxrLatchFrame(receiverHandle, &framesLatched,
                                          cxrFrameMask_All, timeoutMs);
        bool frameValid = (frameErr == cxrError_Success);
        if (!frameValid) {
            ALOGE("[CloudXR]Error in LatchFrame [%0d] = %s", frameErr, cxrErrorString(frameErr));
        }

        for (int eye = 0; eye < 2; eye++) {
            cxrVideoFrame &vf = framesLatched.frames[eye];
            // TODO:bind fbo
//            if (openGlTools.setupFrameBuffer(textures[eye], vf.widthFinal, vf.heightFinal)) {
//                if (frameValid) {
//                    // blit streamed frame into the world layer
//                    cxrBlitFrame(receiverHandle, &framesLatched, 1 << eye);
//                } else {
//                    openGlTools.clear(eye);
//                }
//            }
//            openGlTools.bindDefaultFrameBuffer();
        }
        if (frameValid) {
            cxrReleaseFrame(receiverHandle, &framesLatched);
        }
        return cxrError_Success; //true
    }

    cxrDeviceDesc CloudXR::getDeviceDesc() {
        int dispW = 4320;
        int dispH = 2160;
        int fovX = 105, fovY = 105;
        float playX = 1, playZ = 1; // 半径1米
        cxrDeviceDesc desc = {};
        desc.width = dispW / 2;
        desc.height = dispH;
        desc.maxResFactor = GOptions.mMaxResFactor;
        desc.deliveryType = cxrDeliveryType_Stereo_RGB;
        const int maxWidth = (int) (desc.maxResFactor * (float) desc.width);
        const int maxHeight = (int) (desc.maxResFactor * (float) desc.height);
        ALOGD("[CloudXR]HMD size requested as %d x %d, max %d x %d",
              desc.width, desc.height, maxWidth, maxHeight);
        desc.fps = 72;
        desc.ipd = 0.064f;
        desc.predOffset = 0.02f;
        desc.receiveAudio = GOptions.mReceiveAudio;
        desc.sendAudio = GOptions.mSendAudio;
        desc.posePollFreq = 0;
        desc.disablePosePrediction = false;
        desc.angularVelocityInDeviceSpace = false;
        desc.foveatedScaleFactor = (GOptions.mFoveation < 100) ? GOptions.mFoveation : 0;
        // if we have touch controller use Oculus type, else use Vive as more close to 3dof remotes
        desc.ctrlType = cxrControllerType_OculusTouch;

        const float halfFOVTanX = tanf(M_PI / 360.f * fovX);
        const float halfFOVTanY = tanf(M_PI / 360.f * fovY);

        desc.proj[0][0] = -halfFOVTanX;
        desc.proj[0][1] = halfFOVTanX;
        desc.proj[0][2] = -halfFOVTanY;
        desc.proj[0][3] = halfFOVTanY;
        desc.proj[1][0] = -halfFOVTanX;
        desc.proj[1][1] = halfFOVTanX;
        desc.proj[1][2] = -halfFOVTanY;
        desc.proj[1][3] = halfFOVTanY;

        desc.chaperone.universe = cxrUniverseOrigin_Standing;
        desc.chaperone.origin.m[0][0] = desc.chaperone.origin.m[1][1] = desc.chaperone.origin.m[2][2] = 1;
        desc.chaperone.origin.m[0][1] = desc.chaperone.origin.m[0][2] = desc.chaperone.origin.m[0][3] = 0;
        desc.chaperone.origin.m[1][0] = desc.chaperone.origin.m[1][2] = desc.chaperone.origin.m[1][3] = 0;
        desc.chaperone.origin.m[2][0] = desc.chaperone.origin.m[2][1] = desc.chaperone.origin.m[2][3] = 0;
        desc.chaperone.playArea.v[0] = 2.f * playX;
        desc.chaperone.playArea.v[1] = 2.f * playZ;
        return desc;
    }

    cxrClientCallbacks CloudXR::getClientCallbacks() {
        cxrClientCallbacks callbacks = {};
        callbacks.GetTrackingState = [](void *context, cxrVRTrackingState *trackingState) {
            return reinterpret_cast<CloudXR *>(context)->getTrackingState(trackingState);
        };
        callbacks.TriggerHaptic = [](void *context, const cxrHapticFeedback *haptic) {
            return reinterpret_cast<CloudXR *>(context)->triggerHaptic(haptic);
        };
        callbacks.RenderAudio = [](void *context, const cxrAudioFrame *audioFrame) {
            return reinterpret_cast<CloudXR *>(context)->renderAudio(audioFrame);
        };
        callbacks.ReceiveUserData = [](void *context, const void *data, uint32_t size) {
            return reinterpret_cast<CloudXR *>(context)->receiveUserData(data, size);
        };
        callbacks.UpdateClientState = [](void *context, cxrClientState state,
                                         cxrStateReason reason) {
            return reinterpret_cast<CloudXR *>(context)->updateClientState(state, reason);
        };
        return callbacks;
    }

    void CloudXR::getTrackingState(cxrVRTrackingState *trackingState) {
        cxrVRTrackingState TrackingState = {};
        // 旋转\位移矩阵
        //memcpy(&TrackingState.hmd.pose.deviceToAbsoluteTracking,
        //       &deviceToAbsoluteTracking, sizeof(cxrMatrix34));
        // 速度
        //TrackingState.hmd.pose.velocity = {0, 0, 0,};
        // 角速度
        //TrackingState.hmd.pose.angularVelocity = {0.065, -0.001, 0.010};
        TrackingState.hmd.pose = readImu();
        TrackingState.hmd.pose.poseIsValid = cxrTrue;
        TrackingState.hmd.pose.deviceIsConnected = cxrTrue;
        TrackingState.hmd.pose.trackingResult = cxrTrackingResult_Running_OK;

//        TrackingState.controller[0].pose.deviceToAbsoluteTracking = {};
//        TrackingState.controller[0].pose.velocity = {0, 0, 0,};
//        TrackingState.controller[0].pose.angularVelocity = {0, 0, 0,};
//        TrackingState.controller[0].pose.poseIsValid = cxrTrue;
//        TrackingState.controller[0].pose.deviceIsConnected = cxrTrue;
//        TrackingState.controller[0].pose.trackingResult = cxrTrackingResult_Running_OK;
//
//        TrackingState.controller[1].pose.deviceToAbsoluteTracking = {};
//        TrackingState.controller[1].pose.velocity = {0, 0, 0,};
//        TrackingState.controller[1].pose.angularVelocity = {0, 0, 0,};
//        TrackingState.controller[1].pose.poseIsValid = cxrTrue;
//        TrackingState.controller[1].pose.deviceIsConnected = cxrTrue;
//        TrackingState.controller[1].pose.trackingResult = cxrTrackingResult_Running_OK;

        if (trackingState != nullptr) {
            *trackingState = TrackingState;
        }
    }

    void CloudXR::triggerHaptic(const cxrHapticFeedback *hapticFeedback) {
        ALOGD("[CloudXR]triggerHaptic");
    }

    cxrBool CloudXR::renderAudio(const cxrAudioFrame *audioFrame) {
        //ALOGD("renderAudio size:%d", audioFrame->streamSizeBytes);
        if (pAudioRender) {
            const uint32_t timeout = audioFrame->streamSizeBytes / CXR_AUDIO_BYTES_PER_MS;
            const uint32_t numFrames = timeout * CXR_AUDIO_SAMPLING_RATE / 1000;
            pAudioRender->write(audioFrame->streamBuffer, numFrames, timeout * 1000 * 1000);
        }
        return cxrTrue;
    }

    void CloudXR::receiveUserData(const void *data, uint32_t size) {
        ALOGD("[CloudXR]receiveUserData size:%d", size);
    }

    void CloudXR::updateClientState(cxrClientState state, cxrStateReason reason) {
        ALOGD("[CloudXR]updateClientState state:%s, reason:%s",
              ClientStateEnumToString(state), StateReasonEnumToString(reason));
        clientState = state;
        clientStateReason = reason;
    }

} // end namespace ssnwt