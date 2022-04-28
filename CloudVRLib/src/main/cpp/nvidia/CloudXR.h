#ifndef CLOUDXRDEMO_CLOUDXR_H
#define CLOUDXRDEMO_CLOUDXR_H

#include <string>
#include "CloudXRClient.h"
#include "CloudXRClientOptions.h"
#include "AudioRender.h"

using namespace std;

namespace ssnwt {
    typedef void (*update_tracking_state_call_back)(cxrVRTrackingState *);

    typedef void (*trigger_haptic_call_back)(const cxrHapticFeedback *);

    typedef void (*receive_user_data_call_back)(const void *, uint32_t);

    class CloudXR {
    public:
        cxrError connect(const char *cmdLine,
                         update_tracking_state_call_back tracking_state_cb,
                         trigger_haptic_call_back trigger_haptic_cb,
                         receive_user_data_call_back receive_user_data_cb);

        cxrError disconnect();

        cxrError preRender(cxrFramesLatched *framesLatched);

        cxrError render(uint32_t eye, cxrFramesLatched framesLatched);

        cxrError postRender(cxrFramesLatched framesLatched);

    private:

        cxrDeviceDesc getDeviceDesc() const;

        static cxrClientCallbacks getClientCallbacks();

        void getTrackingState(cxrVRTrackingState *trackingState);

        void triggerHaptic(const cxrHapticFeedback *hapticFeedback);

        cxrBool renderAudio(const cxrAudioFrame *audioFrame);

        void receiveUserData(const void *data, uint32_t size);

        void updateClientState(cxrClientState state, cxrStateReason reason);

    private:
        cxrDeviceDesc deviceDesc{};
        cxrReceiverHandle receiverHandle = nullptr;
        ::CloudXR::ClientOptions GOptions;
        AudioRender *pAudioRender = nullptr;
        cxrClientState clientState = cxrClientState_ReadyToConnect;
        uint64_t connectionFlags = cxrConnectionFlags_ConnectAsync;

        update_tracking_state_call_back updateTrackingStateCallBack{0};
        trigger_haptic_call_back triggerHapticCallBack{0};
        receive_user_data_call_back receiveUserDataCallBack{0};
    };
} // end namespace ssnwt

#endif //CLOUDXRDEMO_CLOUDXR_H
