//
// Created by arena on 2022-4-24.
//

#ifndef CLOUDXR_OPENXR_H
#define CLOUDXR_OPENXR_H

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <jni.h>
#include <android/native_window.h>
#include <array>
#include <map>
#include <list>
#include <vector>
#include <CloudXRCommon.h>

namespace Side {
    const int LEFT = 0;
    const int RIGHT = 1;
    const int COUNT = 2;
}  // namespace Side
struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};
struct InputState {
    XrActionSet actionSet{XR_NULL_HANDLE};
    XrAction thumbstickAction{XR_NULL_HANDLE};
    XrAction gripAction{XR_NULL_HANDLE};
    XrAction triggerAction{XR_NULL_HANDLE};
    XrAction gripPressedAction{XR_NULL_HANDLE};
    XrAction menuAction{XR_NULL_HANDLE};
    XrAction primaryButtonAction{XR_NULL_HANDLE};
    XrAction secondaryButtonAction{XR_NULL_HANDLE};
    XrAction triggerPressedAction{XR_NULL_HANDLE};
    XrAction triggerTouchedAction{XR_NULL_HANDLE};
    XrAction thumbstickClickedAction{XR_NULL_HANDLE};
    XrAction thumbstickTouchedAction{XR_NULL_HANDLE};
    XrAction EnterButtonAction{XR_NULL_HANDLE};
    XrAction HomeButtonAction{XR_NULL_HANDLE};
    XrAction VolumeDownAction{XR_NULL_HANDLE};
    XrAction VolumeUpAction{XR_NULL_HANDLE};
    XrAction poseAction{XR_NULL_HANDLE};
    XrAction hapticAction{XR_NULL_HANDLE};
    std::array<XrPath, Side::COUNT> handSubactionPath;
    XrPath headSubactionPath;
    std::array<XrSpace, Side::COUNT> handSpace;
};

namespace ssnwt {
    typedef void (*draw_frame_call_back)(uint32_t);

    class OpenXR {
    public:
        OpenXR(JavaVM *vm, jobject activity);

        XrResult initialize(draw_frame_call_back cb);

        void setSurface(ANativeWindow *window);

        XrResult render();

        XrResult release();

        XrResult getLocateSpace(XrSpaceLocation *location) {
            return xrLocateSpace(m_appSpace, m_appSpace, 0, location);
        }

        XrResult getControllerSpace(uint32_t side, XrSpaceLocation *location) {
            return xrLocateSpace(m_input.handSpace[side], m_appSpace, 0, location);
        }

        XrResult syncAction() {
            // Sync actions
            const XrActiveActionSet activeActionSet{m_input.actionSet, XR_NULL_PATH};
            XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = 1;
            syncInfo.activeActionSets = &activeActionSet;
            return xrSyncActions(m_session, &syncInfo);
        }

        XrResult getControllerState(uint32_t side, uint32_t *booleanComps,
                                    uint32_t *booleanCompsChanged, float *scalarComps);

    private:
        void processEvent();

        const XrEventDataBaseHeader *tryReadNextEvent();

        bool RenderLayer(XrTime predictedDisplayTime, XrCompositionLayerProjection &layer);

        void RenderView(XrRect2Di imageRect, const uint32_t colorTexture);

        uint32_t GetDepthTexture(uint32_t colorTexture);

        XrInstance m_instance{XR_NULL_HANDLE};
        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_appSpace{XR_NULL_HANDLE};
        XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
        ANativeWindow *p_NativeWindow{};

        InputState m_input{};
        XrEventDataBuffer m_eventDataBuffer{};

        std::vector<XrView> m_views{};
        int64_t m_colorSwapchainFormat{-1};
        std::vector<XrViewConfigurationView> m_configViews{};
        std::vector<Swapchain> m_swapchains;
        std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader *>> m_swapchainImages{};

        GLuint m_swapchainFramebuffer{0};
        std::map<uint32_t, uint32_t> m_colorToDepthMap{};
        std::list<std::vector<XrSwapchainImageOpenGLESKHR>> m_swapchainImageBuffers{};

        draw_frame_call_back m_draw_frame_cb{0};
    };
}

#endif //CLOUDXR_OPENXR_H
