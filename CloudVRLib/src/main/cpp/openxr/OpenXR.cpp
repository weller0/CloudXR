//
// Created by arena on 2022-4-24.
//
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <vector>
#include "common.h"
#include "OpenXR.h"

namespace ssnwt {
    OpenXR::OpenXR(JavaVM *vm, jobject activity) {
        ALOGD("[OpenXR]+");
        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                               (PFN_xrVoidFunction *) (&initializeLoader)))) {
            XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
            memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
            loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
            loaderInitInfoAndroid.next = XR_NULL_HANDLE;
            loaderInitInfoAndroid.applicationVM = vm;
            loaderInitInfoAndroid.applicationContext = activity;
            initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *) &loaderInitInfoAndroid);
        }

        std::vector<const char *> extensions = {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
                                                XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};

        XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid{
                XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
        instanceCreateInfoAndroid.applicationVM = vm;
        instanceCreateInfoAndroid.applicationActivity = activity;
        instanceCreateInfoAndroid.next = XR_NULL_HANDLE;

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = (XrBaseInStructure *) &instanceCreateInfoAndroid;
        createInfo.enabledExtensionCount = extensions.size();
        createInfo.enabledExtensionNames = extensions.data();
        strcpy(createInfo.applicationInfo.applicationName, "HelloXR");
        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        OPENXR_CHECK(xrCreateInstance(&createInfo, &m_instance));
        ALOGD("[OpenXR]xrCreateInstance %p", &m_instance);

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        systemInfo.next = XR_NULL_HANDLE;
        OPENXR_CHECK(xrGetSystem(m_instance, &systemInfo, &m_systemId));
        ALOGD("[OpenXR]xrGetSystem %p", &m_systemId);
    }

    OpenXR::~OpenXR() {
        ALOGD("[OpenXR]-");
    }

    XrResult OpenXR::initialize(ANativeWindow *aNativeWindow) {
        ALOGD("[OpenXR]initialize");
        p_NativeWindow = aNativeWindow;
        CHECK(m_instance != XR_NULL_HANDLE);
        if (m_session == XR_NULL_HANDLE) {
            PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = nullptr;
            OPENXR_CHECK(xrGetInstanceProcAddr(m_instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                                               reinterpret_cast<PFN_xrVoidFunction *>(&pfnGetOpenGLESGraphicsRequirementsKHR)));

            XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{
                    XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
            OPENXR_CHECK(pfnGetOpenGLESGraphicsRequirementsKHR(m_instance, m_systemId,
                                                               &graphicsRequirements));
            GLint major = 0;
            GLint minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);

            const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
            if (graphicsRequirements.minApiVersionSupported > desiredApiVersion) {
                ALOGE("[OpenXR]Runtime does not support desired Graphics API and/or version");
            }

            ALOGV("[OpenXR]Creating session...");
            EGLDisplay display = eglGetCurrentDisplay();
            EGLContext context = eglGetCurrentContext();
            EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
            ALOGV("[OpenXR]display:%p, context:%p, surface:%p, aNativeWindow:%p",
                  display, context, surface, aNativeWindow);
            XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingAndroidGLES = {};
            graphicsBindingAndroidGLES.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
            graphicsBindingAndroidGLES.next = XR_NULL_HANDLE;
            graphicsBindingAndroidGLES.display = display;
            graphicsBindingAndroidGLES.config = XR_NULL_HANDLE;
            graphicsBindingAndroidGLES.context = context;

            XrSessionCreateInfo sessionCreateInfo = {};
            memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
            sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
            sessionCreateInfo.next = &graphicsBindingAndroidGLES;
            sessionCreateInfo.createFlags = 0;
            sessionCreateInfo.systemId = m_systemId;

            OPENXR_CHECK(xrCreateSession(m_instance, &sessionCreateInfo, &m_session));
            ALOGD("[OpenXR]xrCreateSession %p", &m_session);
            // Create an action set.
            {
                XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
                strcpy_s(actionSetInfo.actionSetName, "skyworthtouchcontroller");
                strcpy_s(actionSetInfo.localizedActionSetName, "Skyworth Touch Controller OpenXR");
                actionSetInfo.priority = 0;
                OPENXR_CHECK(xrCreateActionSet(m_instance, &actionSetInfo, &m_input.actionSet));
            }

            // Get the XrPath for the left and right hands - we will use them as subaction paths.
            OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left",
                                        &m_input.handSubactionPath[Side::LEFT]));
            OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right",
                                        &m_input.handSubactionPath[Side::RIGHT]));
            OPENXR_CHECK(xrStringToPath(m_instance, "/user/head", &m_input.headSubactionPath));

            ALOGD("[OpenXR]Create Controller actions");
            // Create Controller actions.
            {
                // Create an input action for grabbing objects with the left and right hands.
                XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "primarybutton");
                strcpy_s(actionInfo.localizedActionName, "Primary Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.primaryButtonAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "primarytouched");
                strcpy_s(actionInfo.localizedActionName, "Primary Touched");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.primaryTouchAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "secondarybutton");
                strcpy_s(actionInfo.localizedActionName, "Secondary Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.secondaryButtonAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "secondarytouched");
                strcpy_s(actionInfo.localizedActionName, "Secondary Touched");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.secondaryTouchAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "grippressed");
                strcpy_s(actionInfo.localizedActionName, "Grip Pressed");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gripPressedAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "menu");
                strcpy_s(actionInfo.localizedActionName, "Menu");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.menuAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "triggerpressed");
                strcpy_s(actionInfo.localizedActionName, "Trigger Pressed");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.triggerPressedAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "triggertouched");
                strcpy_s(actionInfo.localizedActionName, "Trigger Touched");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.triggerTouchedAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "thumbstickclicked");
                strcpy_s(actionInfo.localizedActionName, "Thumbstick Clicked");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.thumbstickClickedAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "thumbsticktouched");
                strcpy_s(actionInfo.localizedActionName, "Thumbstick Touched");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo,
                                            &m_input.thumbstickTouchedAction));

                actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                strcpy_s(actionInfo.actionName, "grip");
                strcpy_s(actionInfo.localizedActionName, "Grip");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.gripAction));

                actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
                strcpy_s(actionInfo.actionName, "trigger");
                strcpy_s(actionInfo.localizedActionName, "Trigger");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.triggerAction));

                actionInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
                strcpy_s(actionInfo.actionName, "thumbstick");
                strcpy_s(actionInfo.localizedActionName, "Thumbstick");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.thumbstickAction));

                actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                strcpy_s(actionInfo.actionName, "devicepose");
                strcpy_s(actionInfo.localizedActionName, "Device Pose");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.poseAction));

                // Create output actions for vibrating the left and right controller.
                actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
                strcpy_s(actionInfo.actionName, "haptic");
                strcpy_s(actionInfo.localizedActionName, "Haptic Output");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(xrCreateAction(m_input.actionSet, &actionInfo, &m_input.hapticAction));

            }

            ALOGD("[OpenXR]Creat HMD Action");
            //Creat HMD Action
            {
                XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "enterButton");
                strcpy_s(actionInfo.localizedActionName, "Enter Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.EnterButtonAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "homeButton");
                strcpy_s(actionInfo.localizedActionName, "Home Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.HomeButtonAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "volumeDownButton");
                strcpy_s(actionInfo.localizedActionName, "VolumeDown Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.VolumeDownAction));

                actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
                strcpy_s(actionInfo.actionName, "volumeUpButton");
                strcpy_s(actionInfo.localizedActionName, "VolumeUp Button");
                actionInfo.countSubactionPaths = uint32_t(m_input.handSubactionPath.size());
                actionInfo.subactionPaths = m_input.handSubactionPath.data();
                OPENXR_CHECK(
                        xrCreateAction(m_input.actionSet, &actionInfo, &m_input.VolumeUpAction));
            }

            ALOGD("[OpenXR]Suggest bindings for the Oculus Touch");
            // Suggest bindings for the Oculus Touch.
            {
                std::array<XrPath, Side::COUNT> primaryButtonPath;
                std::array<XrPath, Side::COUNT> primaryTouchPath;
                std::array<XrPath, Side::COUNT> secondaryButtonPath;
                std::array<XrPath, Side::COUNT> secondaryTouchedPath;
                std::array<XrPath, Side::COUNT> menuClickPath;
                std::array<XrPath, Side::COUNT> selectPath;
                std::array<XrPath, Side::COUNT> squeezeValuePath;
                std::array<XrPath, Side::COUNT> squeezeForcePath;
                std::array<XrPath, Side::COUNT> squeezeClickPath;
                std::array<XrPath, Side::COUNT> posePath;
                std::array<XrPath, Side::COUNT> hapticPath;
                std::array<XrPath, Side::COUNT> triggerValuePath;
                std::array<XrPath, Side::COUNT> gripValuePath;
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/x/click",
                                            &primaryButtonPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/a/click",
                                            &primaryButtonPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/y/click",
                                            &secondaryButtonPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/b/click",
                                            &secondaryButtonPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/x/touch",
                                            &primaryTouchPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/a/touch",
                                            &primaryTouchPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/y/touch",
                                            &secondaryTouchedPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/b/touch",
                                            &secondaryTouchedPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/menu/click",
                                            &menuClickPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/menu/click",
                                            &menuClickPath[Side::RIGHT]));

                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/select/click",
                                            &selectPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/select/click",
                                            &selectPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/value",
                                            &squeezeValuePath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/value",
                                            &squeezeValuePath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/force",
                                            &squeezeForcePath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/force",
                                            &squeezeForcePath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/squeeze/click",
                                            &squeezeClickPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/squeeze/click",
                                            &squeezeClickPath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/grip/pose",
                                            &posePath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/grip/pose",
                                            &posePath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/output/haptic",
                                            &hapticPath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/output/haptic",
                                            &hapticPath[Side::RIGHT]));

                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/trigger/value",
                                            &triggerValuePath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/trigger/value",
                                            &triggerValuePath[Side::RIGHT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/left/input/grip/value",
                                            &gripValuePath[Side::LEFT]));
                OPENXR_CHECK(xrStringToPath(m_instance, "/user/hand/right/input/grip/value",
                                            &gripValuePath[Side::RIGHT]));

                XrPath skyworthTouchInteractionProfilePath;
                OPENXR_CHECK(
                        xrStringToPath(m_instance,
                                       "/interaction_profiles/skyworth/touch_controller",
                                       &skyworthTouchInteractionProfilePath));
                std::vector<XrActionSuggestedBinding> bindings{{
                                                                       {m_input.primaryButtonAction, primaryButtonPath[Side::LEFT]},
                                                                       {m_input.primaryButtonAction, primaryButtonPath[Side::RIGHT]},
                                                                       {m_input.primaryTouchAction, primaryTouchPath[Side::LEFT]},
                                                                       {m_input.primaryTouchAction, primaryTouchPath[Side::RIGHT]},
                                                                       {m_input.secondaryButtonAction, secondaryButtonPath[Side::LEFT]},
                                                                       {m_input.secondaryButtonAction, secondaryButtonPath[Side::RIGHT]},
                                                                       {m_input.secondaryTouchAction, secondaryTouchedPath[Side::LEFT]},
                                                                       {m_input.secondaryTouchAction, secondaryTouchedPath[Side::RIGHT]},
                                                                       {m_input.menuAction, menuClickPath[Side::LEFT]},
                                                                       {m_input.menuAction, menuClickPath[Side::RIGHT]},
                                                                       {m_input.poseAction, posePath[Side::LEFT]},
                                                                       {m_input.poseAction, posePath[Side::RIGHT]},
                                                               }};
                XrInteractionProfileSuggestedBinding suggestedBindings{
                        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
                suggestedBindings.interactionProfile = skyworthTouchInteractionProfilePath;
                suggestedBindings.suggestedBindings = bindings.data();
                suggestedBindings.countSuggestedBindings = (uint32_t) bindings.size();
                OPENXR_CHECK(xrSuggestInteractionProfileBindings(m_instance, &suggestedBindings));
            }

            ALOGD("[OpenXR]xrCreateActionSpace");
            XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
            actionSpaceInfo.action = m_input.poseAction;
            actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
            actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::LEFT];
            OPENXR_CHECK(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                             &m_input.handSpace[Side::LEFT]));
            actionSpaceInfo.subactionPath = m_input.handSubactionPath[Side::RIGHT];
            OPENXR_CHECK(xrCreateActionSpace(m_session, &actionSpaceInfo,
                                             &m_input.handSpace[Side::RIGHT]));

            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &m_input.actionSet;
            OPENXR_CHECK(xrAttachSessionActionSets(m_session, &attachInfo));

            ALOGD("[OpenXR]Create a space to the first path");
            // Create a space to the first path
            XrReferenceSpaceCreateInfo spaceCreateInfo = {};
            spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
            OPENXR_CHECK(xrCreateReferenceSpace(m_session, &spaceCreateInfo, &m_appSpace));
            ALOGV("[OpenXR]xrCreateReferenceSpace:%p", &m_appSpace);
        }

        uint32_t viewCount;
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                                       &viewCount, nullptr));
        ALOGV("[OpenXR]viewCount:%d", viewCount);
        m_configViews.resize(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_instance, m_systemId,
                                                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                                       viewCount, &viewCount,
                                                       m_configViews.data()));

        m_views.resize(viewCount, {XR_TYPE_VIEW});
        if (viewCount > 0) {
            // Select a swapchain format.
            uint32_t swapchainFormatCount;
            OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
            std::vector<int64_t> swapchainFormats(swapchainFormatCount);
            OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, (uint32_t) swapchainFormats.size(),
                                                     &swapchainFormatCount,
                                                     swapchainFormats.data()));
            CHECK(swapchainFormatCount == swapchainFormats.size());

            std::vector<int64_t> supportedColorSwapchainFormats{GL_RGBA8, GL_RGBA8_SNORM};
            auto swapchainFormatIt = std::find_first_of(swapchainFormats.begin(),
                                                        swapchainFormats.end(),
                                                        supportedColorSwapchainFormats.begin(),
                                                        supportedColorSwapchainFormats.end());
            if (swapchainFormatIt == swapchainFormats.end()) {
                ALOGE("[OpenXR]No runtime swapchain format supported for color swapchain");
            }
            m_colorSwapchainFormat = *swapchainFormatIt;

            for (int i = 0; i < viewCount; i++) {
                const XrViewConfigurationView &vp = m_configViews[i];
                ALOGV("[OpenXR]Creating swapchain for view %d with dimensions Width=%d Height=%d SampleCount=%d",
                      i,
                      vp.recommendedImageRectWidth, vp.recommendedImageRectHeight,
                      vp.recommendedSwapchainSampleCount);
                // Create the swapchain.
                XrSwapchainCreateInfo swapchainCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCreateInfo.arraySize = 1;
                swapchainCreateInfo.format = m_colorSwapchainFormat;
                swapchainCreateInfo.width = vp.recommendedImageRectWidth;
                swapchainCreateInfo.height = vp.recommendedImageRectHeight;
                swapchainCreateInfo.mipCount = 1;
                swapchainCreateInfo.faceCount = 1;
                swapchainCreateInfo.sampleCount = vp.recommendedSwapchainSampleCount;
                swapchainCreateInfo.usageFlags =
                        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
                Swapchain swapchain{};
                swapchain.width = swapchainCreateInfo.width;
                swapchain.height = swapchainCreateInfo.height;
                OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

                m_swapchains.push_back(swapchain);

                uint32_t imageCount;
                OPENXR_CHECK(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));
                ALOGV("[OpenXR]imageCount:%d", imageCount);
                std::vector<XrSwapchainImageBaseHeader *> swapchainImages;
                std::vector<XrSwapchainImageOpenGLESKHR> swapchainImageBuffer(imageCount);
                for (XrSwapchainImageOpenGLESKHR &image : swapchainImageBuffer) {
                    image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
                    ALOGV("xmh AllocateSwapchainImageStructs");
                    swapchainImages.push_back(
                            reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
                }
//                for (int imageindex = 0; imageindex < imageCount; ++imageindex) {
//                    XrSwapchainImageOpenGLESKHR gleskhr{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR};
//                    ALOGV("xmh push_back XrSwapchainImageOpenGLESKHR:%d",imageindex);
//                    swapchainImages.push_back(reinterpret_cast<XrSwapchainImageBaseHeader*>(&gleskhr));
//                }
                OPENXR_CHECK(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount,
                                                        swapchainImages[0]));
                ALOGV("xmh AllocateSwapchainImageStructs 111");
                m_swapchainImages.insert(
                        std::make_pair(swapchain.handle, std::move(swapchainImages)));
            }
        }
        return XR_SUCCESS;
    }

    const XrEventDataBaseHeader *OpenXR::tryReadNextEvent() {
        // It is sufficient to clear the just the XrEventDataBuffer header to
        // XR_TYPE_EVENT_DATA_BUFFER
        auto *baseHeader = reinterpret_cast<XrEventDataBaseHeader *>(&m_eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const auto *const eventsLost = reinterpret_cast<const XrEventDataEventsLost *>(baseHeader);
                ALOGV("[OpenXR]%d events lost", eventsLost->type);
            }

            return baseHeader;
        }
        if (xr == XR_EVENT_UNAVAILABLE) {
            return nullptr;
        }
        return nullptr;
    }

    XrResult OpenXR::release() {
        ALOGD("[OpenXR]release");
        return XR_SUCCESS;
    }

    XrResult OpenXR::render() {
        while (const XrEventDataBaseHeader *event = tryReadNextEvent()) {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto &instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending *>(event);
//                    ALOGV("XrEventDataInstanceLossPending by %" PRIi64, instanceLossPending.lossTime);
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged *>(event);
                    switch (sessionStateChangedEvent.state) {
                        case XR_SESSION_STATE_READY: {
                            CHECK(m_session != XR_NULL_HANDLE);
                            XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                            sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                            sessionBeginInfo.next = p_NativeWindow;//need set NativeWindow
                            OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo));
                            break;
                        }
                        case XR_SESSION_STATE_STOPPING: {
                            CHECK(m_session != XR_NULL_HANDLE);
                            OPENXR_CHECK(xrEndSession(m_session));
                            break;
                        }
                        case XR_SESSION_STATE_EXITING: {
                            // Do not attempt to restart because user closed this session.
                            break;
                        }
                        case XR_SESSION_STATE_LOSS_PENDING: {
                            // Poll for a new instance.
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    ALOGV("[OpenXR]XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED");
//                    LogActionSourceName(m_input.grabAction, "Grab");
//                    LogActionSourceName(m_input.quitAction, "Quit");
//                    LogActionSourceName(m_input.poseAction, "Pose");
//                    LogActionSourceName(m_input.vibrateAction, "Vibrate");
                    break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    ALOGV("[OpenXR]Ignoring event type %d", event->type);
                    break;
                }
            }
        }

        CHECK(m_session != XR_NULL_HANDLE);

        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState));

        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo));

        std::vector<XrCompositionLayerBaseHeader *> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
        if (frameState.shouldRender == XR_TRUE) {
            if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews,
                            layer)) {
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
            }
        }

        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        frameEndInfo.layerCount = (uint32_t) layers.size();
        frameEndInfo.layers = layers.data();
        OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo));

        return XR_SUCCESS;
    }

    bool OpenXR::RenderLayer(XrTime predictedDisplayTime,
                             std::vector<XrCompositionLayerProjectionView> &projectionLayerViews,
                             XrCompositionLayerProjection &layer) {
        XrResult res;

        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = (uint32_t) m_views.size();
        uint32_t viewCountOutput;

        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        res = xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput,
                            &viewCountOutput, m_views.data());
        CHECK_XRRESULT(res, "xrLocateViews");
        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        CHECK(viewCountOutput == viewCapacityInput);
        CHECK(viewCountOutput == m_configViews.size());
        CHECK(viewCountOutput == m_swapchains.size());

        projectionLayerViews.resize(viewCountOutput);

        // Render view to the appropriate part of the swapchain image.
        for (uint32_t i = 0; i < viewCountOutput; i++) {
            // Each view has a separate swapchain which is acquired, rendered to, and released.
            const Swapchain viewSwapchain = m_swapchains[i];

            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};

            uint32_t swapchainImageIndex;
            OPENXR_CHECK(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo,
                                                 &swapchainImageIndex));

            XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            OPENXR_CHECK(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

            projectionLayerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionLayerViews[i].pose = m_views[i].pose;
            projectionLayerViews[i].fov = m_views[i].fov;
            projectionLayerViews[i].subImage.swapchain = viewSwapchain.handle;
            projectionLayerViews[i].subImage.imageRect.offset = {0, 0};
            projectionLayerViews[i].subImage.imageRect.extent = {viewSwapchain.width,
                                                                 viewSwapchain.height};

            const XrSwapchainImageBaseHeader *const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];
            RenderView(i, projectionLayerViews[i], swapchainImage, m_colorSwapchainFormat);

            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OPENXR_CHECK(xrReleaseSwapchainImage(viewSwapchain.handle, &releaseInfo));
        }

        layer.space = m_appSpace;
        layer.viewCount = (uint32_t) projectionLayerViews.size();
        layer.views = projectionLayerViews.data();
        return true;
    }

    void OpenXR::RenderView(const uint32_t eye, const XrCompositionLayerProjectionView &layerView,
                            const XrSwapchainImageBaseHeader *swapchainImage,
                            int64_t swapchainFormat) {
        CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.

        glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);

        const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR *>(swapchainImage)->image;

        glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
                   static_cast<GLint>(layerView.subImage.imageRect.offset.y),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
                   static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));
        glFrontFace(GL_CW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);

        const uint32_t depthTexture = GetDepthTexture(colorTexture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               colorTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture,
                               0);

        glClearColor(0.6f, eye == 0 ? 0.3f : 0, eye == 0 ? 0 : 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    uint32_t OpenXR::GetDepthTexture(uint32_t colorTexture) {
// If a depth-stencil view has already been created for this back-buffer, use it.
        auto depthBufferIt = m_colorToDepthMap.find(colorTexture);
        if (depthBufferIt != m_colorToDepthMap.end()) {
            return depthBufferIt->second;
        }

        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

        GLint width;
        GLint height;
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

        uint32_t depthTexture;
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

        m_colorToDepthMap.insert(std::make_pair(colorTexture, depthTexture));

        return depthTexture;
    }
}