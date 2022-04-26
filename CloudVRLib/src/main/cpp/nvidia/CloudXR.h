#ifndef CLOUDXRDEMO_CLOUDXR_H
#define CLOUDXRDEMO_CLOUDXR_H

#include <string>
#include "CloudXRClient.h"
#include "CloudXRClientOptions.h"
#include "AudioRender.h"

using namespace std;

namespace ssnwt {
    class CloudXR {
    public:
        int connect(const char *cmdLine);

        int disconnect();

        int preRender(cxrFramesLatched *framesLatched);

        int render(uint32_t eye, cxrFramesLatched framesLatched);

        int postRender(cxrFramesLatched framesLatched);

    private:

        cxrDeviceDesc getDeviceDesc();

        cxrClientCallbacks getClientCallbacks();

        void getTrackingState(cxrVRTrackingState *trackingState);

        void triggerHaptic(const cxrHapticFeedback *hapticFeedback);

        cxrBool renderAudio(const cxrAudioFrame *audioFrame);

        void receiveUserData(const void *data, uint32_t size);

        void updateClientState(cxrClientState state, cxrStateReason reason);

    private:
        cxrDeviceDesc deviceDesc;
        cxrReceiverHandle receiverHandle = nullptr;
        ::CloudXR::ClientOptions GOptions;
        AudioRender *pAudioRender;
        cxrClientState clientState = cxrClientState_ReadyToConnect;
        cxrStateReason clientStateReason = cxrStateReason_NoError;
        uint64_t connectionFlags = cxrConnectionFlags_ConnectAsync;
    };
} // end namespace ssnwt

#endif //CLOUDXRDEMO_CLOUDXR_H
