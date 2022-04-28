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
    cxrError CloudXR::connect(const char *cmdLine,
                              update_tracking_state_call_back tracking_state_cb,
                              trigger_haptic_call_back trigger_haptic_cb,
                              receive_user_data_call_back receive_user_data_cb) {
        updateTrackingStateCallBack = tracking_state_cb;
        triggerHapticCallBack = trigger_haptic_cb;
        receiveUserDataCallBack = receive_user_data_cb;
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

    cxrError CloudXR::disconnect() {
        updateTrackingStateCallBack = nullptr;
        triggerHapticCallBack = nullptr;
        receiveUserDataCallBack = nullptr;
        {
            // 作用域为当前函数
            std::lock_guard<std::mutex> lockGuard(audioMutex);
            delete pAudioRender;
            pAudioRender = nullptr;
        }
        ALOGE("[CloudXR]disconnect");
        if (receiverHandle) {
            cxrDestroyReceiver(receiverHandle);
            receiverHandle = nullptr;
        }
        return cxrError_Success; //true
    }

    cxrError CloudXR::preRender(cxrFramesLatched *framesLatched) {
        if (!receiverHandle) {
            ALOGE("[CloudXR]receiverHandle is null");
            return cxrError_Receiver_Invalid;
        }
        if (clientState != cxrClientState_StreamingSessionInProgress) {
            ALOGE("[CloudXR]receiverHandle is cxrError_Streamer_Not_Ready %s",
                  ClientStateEnumToString(clientState));
            return cxrError_Streamer_Not_Ready;
        }

        const uint32_t timeoutMs = 500;
        cxrError frameErr = cxrLatchFrame(receiverHandle, framesLatched,
                                          cxrFrameMask_All, timeoutMs);
        bool frameValid = (frameErr == cxrError_Success);
        if (!frameValid) {
            ALOGE("[CloudXR]Error in LatchFrame [%0d] = %s", frameErr, cxrErrorString(frameErr));
            return cxrError_Frame_Invalid;
        }
        return cxrError_Success; //true
    }

    cxrError CloudXR::render(uint32_t eye, cxrFramesLatched framesLatched) {
        cxrBlitFrame(receiverHandle, &framesLatched, static_cast<uint32_t>(1 << eye));
        return cxrError_Success; //true
    }

    cxrError CloudXR::postRender(cxrFramesLatched framesLatched) {
        cxrReleaseFrame(receiverHandle, &framesLatched);
        return cxrError_Success; //true
    }

    cxrDeviceDesc CloudXR::getDeviceDesc() const {
        uint32_t dispW = 4320;
        uint32_t dispH = 2160;
        uint32_t fovX = 105, fovY = 105;
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
        desc.receiveAudio = static_cast<cxrBool>(GOptions.mReceiveAudio);
        desc.sendAudio = static_cast<cxrBool>(GOptions.mSendAudio);
        desc.posePollFreq = 0;
        desc.disablePosePrediction = cxrTrue;
        desc.angularVelocityInDeviceSpace = cxrFalse;
        desc.foveatedScaleFactor = static_cast<uint32_t>((GOptions.mFoveation < 100)
                                                         ? GOptions.mFoveation : 0);
        // if we have touch controller use Oculus type, else use Vive as more close to 3dof remotes
        desc.ctrlType = cxrControllerType_OculusTouch;

        const float halfFOVTanX = tanf(static_cast<float>(M_PI / 360.f * fovX));
        const float halfFOVTanY = tanf(static_cast<float>(M_PI / 360.f * fovY));

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
//        std::lock_guard<std::mutex> lockGuard(cloudMutex);
        if (updateTrackingStateCallBack) {
            updateTrackingStateCallBack(trackingState);
        }
    }

    void CloudXR::triggerHaptic(const cxrHapticFeedback *hapticFeedback) {
        if (triggerHapticCallBack) triggerHapticCallBack(hapticFeedback);
    }

    cxrBool CloudXR::renderAudio(const cxrAudioFrame *audioFrame) {
        //ALOGD("[CloudXR]renderAudio size:%d", audioFrame->streamSizeBytes);
        // 作用域为当前函数
        std::lock_guard<std::mutex> lockGuard(audioMutex);
        if (pAudioRender) {
            const uint32_t timeout = audioFrame->streamSizeBytes / CXR_AUDIO_BYTES_PER_MS;
            const uint32_t numFrames = timeout * CXR_AUDIO_SAMPLING_RATE / 1000;
            pAudioRender->write(audioFrame->streamBuffer, (int32_t) numFrames,
                                timeout * 1000 * 1000);
            return cxrTrue;
        }
        return cxrFalse;
    }

    void CloudXR::receiveUserData(const void *data, uint32_t size) {
        if (receiveUserDataCallBack) receiveUserDataCallBack(data, size);
    }

    void CloudXR::updateClientState(cxrClientState state, cxrStateReason reason) {
        ALOGD("[CloudXR]updateClientState state:%s, reason:%s",
              ClientStateEnumToString(state), StateReasonEnumToString(reason));
        clientState = state;
    }

} // end namespace ssnwt