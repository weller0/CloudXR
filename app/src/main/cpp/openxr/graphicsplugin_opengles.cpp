// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <GLES3/gl3.h>
#include <map>
#include <list>
#include <memory>
#include <utility>
#include <openxr/openxr_platform.h>
#include "check.h"
#include "common.h"
#include "geometry.h"
#include "graphicsplugin.h"
#include "platformplugin.h"
#include "gfxwrapper_opengl.h"
#include "xr_linear.h"
#include "GraphicRender.h"

namespace {
    struct OpenGLESGraphicsPlugin : public IGraphicsPlugin {
        OpenGLESGraphicsPlugin(const std::shared_ptr<Options> & /*unused*/,
                               const std::shared_ptr<IPlatformPlugin> /*unused*/&) {};

        OpenGLESGraphicsPlugin(const OpenGLESGraphicsPlugin &) = delete;

        OpenGLESGraphicsPlugin &operator=(const OpenGLESGraphicsPlugin &) = delete;

        OpenGLESGraphicsPlugin(OpenGLESGraphicsPlugin &&) = delete;

        OpenGLESGraphicsPlugin &operator=(OpenGLESGraphicsPlugin &&) = delete;

        ~OpenGLESGraphicsPlugin() override {
            if (m_swapchainFramebuffer != 0) {
                glDeleteFramebuffers(1, &m_swapchainFramebuffer);
            }

            for (auto &colorToDepth : m_colorToDepthMap) {
                if (colorToDepth.second != 0) {
                    glDeleteTextures(1, &colorToDepth.second);
                }
            }
        }

        std::vector<std::string> GetInstanceExtensions() const override {
            return {XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
        }

        ksGpuWindow window{};

        void InitializeDevice(XrInstance instance, XrSystemId systemId) override {
            // Extension function must be loaded by name
            PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = nullptr;
            CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR",
                                              reinterpret_cast<PFN_xrVoidFunction *>(&pfnGetOpenGLESGraphicsRequirementsKHR)));

            XrGraphicsRequirementsOpenGLESKHR graphicsRequirements{
                    XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
            CHECK_XRCMD(pfnGetOpenGLESGraphicsRequirementsKHR(instance, systemId,
                                                              &graphicsRequirements));

            // Initialize the gl extensions. Note we have to open a window.
            ksDriverInstance driverInstance{};
            ksGpuQueueInfo queueInfo{};
            ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
            ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
            ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
            if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0, colorFormat,
                                    depthFormat, sampleCount, 640, 480, false)) {
                THROW("Unable to create GL context");
            }
            Log::Write(Log::Level::Info,
                       Fmt("ksGpuWindow_Create nativeWindow(%p)", window.nativeWindow));
            GLint major = 0;
            GLint minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);

            const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
            if (graphicsRequirements.minApiVersionSupported > desiredApiVersion) {
                THROW("Runtime does not support desired Graphics API and/or version");
            }

            m_contextApiMajorVersion = major;

            m_graphicsBinding.display = window.display;
            m_graphicsBinding.config = (EGLConfig) 0;
            m_graphicsBinding.context = window.context.context;
            m_graphicsBinding.next = window.nativeWindow;
            Log::Write(Log::Level::Info, Fmt("xmh.nativeWindow(%p)", window.nativeWindow));

            glGenFramebuffers(1, &m_swapchainFramebuffer);

            m_graphicRender.initialize();
        }

        int64_t
        SelectColorSwapchainFormat(const std::vector<int64_t> &runtimeFormats) const override {
            // List of supported color swapchain formats.
            std::vector<int64_t> supportedColorSwapchainFormats{GL_RGBA8, GL_RGBA8_SNORM};

            // In OpenGLES 3.0+, the R, G, and B values after blending are converted into the non-linear
            // sRGB automatically.
            if (m_contextApiMajorVersion >= 3) {
                supportedColorSwapchainFormats.push_back(GL_SRGB8_ALPHA8);
            }

            auto swapchainFormatIt = std::find_first_of(runtimeFormats.begin(),
                                                        runtimeFormats.end(),
                                                        supportedColorSwapchainFormats.begin(),
                                                        supportedColorSwapchainFormats.end());
            if (swapchainFormatIt == runtimeFormats.end()) {
                THROW("No runtime swapchain format supported for color swapchain");
            }

            return *swapchainFormatIt;
        }

        const XrBaseInStructure *GetGraphicsBinding() const override {
            return reinterpret_cast<const XrBaseInStructure *>(&m_graphicsBinding);
        }

        std::vector<XrSwapchainImageBaseHeader *> AllocateSwapchainImageStructs(
                uint32_t capacity, const XrSwapchainCreateInfo & /*swapchainCreateInfo*/) override {
            // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
            // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
            std::vector<XrSwapchainImageOpenGLESKHR> swapchainImageBuffer(capacity);
            std::vector<XrSwapchainImageBaseHeader *> swapchainImageBase;
            for (XrSwapchainImageOpenGLESKHR &image : swapchainImageBuffer) {
                image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
                swapchainImageBase.push_back(
                        reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
            }

            // Keep the buffer alive by moving it into the list of buffers.
            m_swapchainImageBuffers.push_back(std::move(swapchainImageBuffer));

            return swapchainImageBase;
        }

        void RenderView(const uint32_t eye, const XrCompositionLayerProjectionView &layerView,
                        const XrSwapchainImageBaseHeader *swapchainImage,
                        int64_t swapchainFormat) override {
            CHECK(layerView.subImage.imageArrayIndex == 0);  // Texture arrays not supported.
            UNUSED_PARM(swapchainFormat);                    // Not used in this function for now.

            glBindFramebuffer(GL_FRAMEBUFFER, m_swapchainFramebuffer);

            const uint32_t colorTexture = reinterpret_cast<const XrSwapchainImageOpenGLESKHR *>(swapchainImage)->image;

            glViewport(static_cast<GLint>(layerView.subImage.imageRect.offset.x),
                       static_cast<GLint>(layerView.subImage.imageRect.offset.y),
                       static_cast<GLsizei>(layerView.subImage.imageRect.extent.width),
                       static_cast<GLsizei>(layerView.subImage.imageRect.extent.height));

//            Log::Write(Log::Level::Info,
//                       Fmt("xmh viewport(%d,%d,%d,%d)", layerView.subImage.imageRect.offset.x,
//                           layerView.subImage.imageRect.offset.y,
//                           layerView.subImage.imageRect.extent.width,
//                           layerView.subImage.imageRect.extent.height));
            glFrontFace(GL_CW);
            glCullFace(GL_BACK);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            const uint32_t depthTexture = GetDepthTexture(colorTexture);
//            Log::Write(Log::Level::Info,
//                       Fmt("Render colorTexture(%u),depthTexture(%u)", colorTexture, depthTexture));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   colorTexture, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture,
                                   0);

            m_graphicRender.draw(eye, layerView.pose);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        uint32_t
        GetSupportedSwapchainSampleCount(const XrViewConfigurationView &) override { return 1; }

        uint32_t GetDepthTexture(uint32_t colorTexture) {
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

    private:
        XrGraphicsBindingOpenGLESAndroidKHR m_graphicsBinding{
                XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};

        std::list<std::vector<XrSwapchainImageOpenGLESKHR>> m_swapchainImageBuffers;
        GLuint m_swapchainFramebuffer{0};
        GLint m_contextApiMajorVersion{0};

        // Map color buffer to associated depth buffer. This map is populated on demand.
        std::map<uint32_t, uint32_t> m_colorToDepthMap;

        ssnwt::GraphicRender m_graphicRender;
    };
}  // namespace

std::shared_ptr<IGraphicsPlugin> CreateGraphicsPlugin(const std::shared_ptr<Options> &options,
                                                      std::shared_ptr<IPlatformPlugin> platformPlugin) {
    return std::make_shared<OpenGLESGraphicsPlugin>(options, platformPlugin);
}
