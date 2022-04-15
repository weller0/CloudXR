/*
Copyright (c) 2016 Oculus VR, LLC.
Portions of macOS, iOS, functionality copyright (c) 2016 The Brenwill Workshop Ltd.

SPDX-License-Identifier: Apache-2.0
*/

#include "gfxwrapper_opengl.h"

/*
================================================================================================================================

System level functionality

================================================================================================================================
*/

static void *AllocAlignedMemory(size_t size, size_t alignment) {
    alignment = (alignment < sizeof(void *)) ? sizeof(void *) : alignment;
    return memalign(alignment, size);
}

static void FreeAlignedMemory(void *ptr) {
    free(ptr);
}

static void Print(const char *format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 4096, format, args);
    va_end(args);

    __android_log_print(ANDROID_LOG_VERBOSE, "atw", "%s", buffer);
}

static void Error(const char *format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 4096, format, args);
    va_end(args);

    __android_log_print(ANDROID_LOG_ERROR, "atw", "%s", buffer);
    // Without exiting, the application will likely crash.
    if (format != NULL) {
        exit(0);
    }
}

/*
================================================================================================================================

Frame logging.

Each thread that calls ksFrameLog_Open will open its own log.
A frame log is always opened for a specified number of frames, and will
automatically close after the specified number of frames have been recorded.
The CPU and GPU times for the recorded frames will be listed at the end of the log.

ksFrameLog

static void ksFrameLog_Open( const char * fileName, const int frameCount );
static void ksFrameLog_Write( const char * fileName, const int lineNumber, const char * function );
static void ksFrameLog_BeginFrame();
static void ksFrameLog_EndFrame( const ksNanoseconds cpuTimeNanoseconds, const ksNanoseconds gpuTimeNanoseconds, const int
gpuTimeFramesDelayed );

================================================================================================================================
*/

typedef struct {
    FILE *fp;
    ksNanoseconds *frameCpuTimes;
    ksNanoseconds *frameGpuTimes;
    int frameCount;
    int frame;
} ksFrameLog;

__thread ksFrameLog *threadFrameLog;

static ksFrameLog *ksFrameLog_Get() {
    ksFrameLog *l = threadFrameLog;
    if (l == NULL) {
        l = (ksFrameLog *) malloc(sizeof(ksFrameLog));
        memset(l, 0, sizeof(ksFrameLog));
        threadFrameLog = l;
    }
    return l;
}

static void ksFrameLog_Open(const char *fileName, const int frameCount) {
    ksFrameLog *l = ksFrameLog_Get();
    if (l != NULL && l->fp == NULL) {
        l->fp = fopen(fileName, "wb");
        if (l->fp == NULL) {
            Print("Failed to open %s\n", fileName);
        } else {
            Print("Opened frame log %s for %d frames.\n", fileName, frameCount);
            l->frameCpuTimes = (ksNanoseconds *) malloc(frameCount * sizeof(l->frameCpuTimes[0]));
            l->frameGpuTimes = (ksNanoseconds *) malloc(frameCount * sizeof(l->frameGpuTimes[0]));
            memset(l->frameCpuTimes, 0, frameCount * sizeof(l->frameCpuTimes[0]));
            memset(l->frameGpuTimes, 0, frameCount * sizeof(l->frameGpuTimes[0]));
            l->frameCount = frameCount;
            l->frame = 0;
        }
    }
}

static void ksFrameLog_Write(const char *fileName, const int lineNumber, const char *function) {
    ksFrameLog *l = ksFrameLog_Get();
    if (l != NULL && l->fp != NULL) {
        if (l->frame < l->frameCount) {
            fprintf(l->fp, "%s(%d): %s\r\n", fileName, lineNumber, function);
        }
    }
}

static void ksFrameLog_BeginFrame() {
    ksFrameLog *l = ksFrameLog_Get();
    if (l != NULL && l->fp != NULL) {
        if (l->frame < l->frameCount) {
#if defined(_DEBUG)
            fprintf(l->fp, "================ BEGIN FRAME %d ================\r\n", l->frame);
#endif
        }
    }
}

static void
ksFrameLog_EndFrame(const ksNanoseconds cpuTimeNanoseconds, const ksNanoseconds gpuTimeNanoseconds,
                    const int gpuTimeFramesDelayed) {
    ksFrameLog *l = ksFrameLog_Get();
    if (l != NULL && l->fp != NULL) {
        if (l->frame < l->frameCount) {
            l->frameCpuTimes[l->frame] = cpuTimeNanoseconds;
#if defined(_DEBUG)
            fprintf(l->fp, "================ END FRAME %d ================\r\n", l->frame);
#endif
        }
        if (l->frame >= gpuTimeFramesDelayed && l->frame < l->frameCount + gpuTimeFramesDelayed) {
            l->frameGpuTimes[l->frame - gpuTimeFramesDelayed] = gpuTimeNanoseconds;
        }

        l->frame++;

        if (l->frame >= l->frameCount + gpuTimeFramesDelayed) {
            for (int i = 0; i < l->frameCount; i++) {
                fprintf(l->fp, "frame %d: CPU = %1.1f ms, GPU = %1.1f ms\r\n", i,
                        l->frameCpuTimes[i] * 1e-6f,
                        l->frameGpuTimes[i] * 1e-6f);
            }

            Print("Closing frame log file (%d frames).\n", l->frameCount);
            fclose(l->fp);
            free(l->frameCpuTimes);
            free(l->frameGpuTimes);
            memset(l, 0, sizeof(ksFrameLog));
        }
    }
}

/*
================================================================================================================================

OpenGL error checking.

================================================================================================================================
*/

#if defined(_DEBUG)
#define GL(func)                                 \
    func;                                        \
    ksFrameLog_Write(__FILE__, __LINE__, #func); \
    GlCheckErrors(#func);
#else
#define GL(func) func;
#endif

#if defined(_DEBUG)
#define EGL(func)                                                  \
    ksFrameLog_Write(__FILE__, __LINE__, #func);                   \
    if (func == EGL_FALSE) {                                       \
        Error(#func " failed: %s", EglErrorString(eglGetError())); \
    }
#else
#define EGL(func)                                                  \
    if (func == EGL_FALSE) {                                       \
        Error(#func " failed: %s", EglErrorString(eglGetError())); \
    }
#endif

#if defined(OS_ANDROID) || defined(OS_LINUX_WAYLAND)

static const char *EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

#endif

static const char *GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
#if !defined(OS_APPLE_MACOS) && !defined(OS_ANDROID) && !defined(OS_APPLE_IOS)
            case GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
#endif
        default:
            return "unknown";
    }
}

static const char *GlFramebufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
#if !defined(OS_ANDROID) && !defined(OS_APPLE_IOS)
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER";
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                return "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER";
            case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS";
#endif
        default:
            return "unknown";
    }
}

static void GlCheckErrors(const char *function) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        Error("GL error: %s: %s", function, GlErrorString(error));
    }
}

/*
================================================================================================================================

OpenGL extensions.

================================================================================================================================
*/

typedef struct {
    bool timer_query;                       // GL_ARB_timer_query, GL_EXT_disjoint_timer_query
    bool texture_clamp_to_border;           // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool buffer_storage;                    // GL_ARB_buffer_storage
    bool multi_sampled_storage;             // GL_ARB_texture_storage_multisample
    bool multi_view;                        // GL_OVR_multiview, GL_OVR_multiview2
    bool multi_sampled_resolve;             // GL_EXT_multisampled_render_to_texture
    bool multi_view_multi_sampled_resolve;  // GL_OVR_multiview_multisampled_render_to_texture

    int texture_clamp_to_border_id;
} ksOpenGLExtensions;

ksOpenGLExtensions glExtensions;

/*
================================
Get proc address / extensions
================================
*/
void (*GetExtension(const char *functionName))() { return eglGetProcAddress(functionName); }

GLint glGetInteger(GLenum pname) {
    GLint i;
    GL(glGetIntegerv(pname, &i));
    return i;
}

static bool GlCheckExtension(const char *extension) {
    GL(const GLint numExtensions = glGetInteger(GL_NUM_EXTENSIONS));
    for (int i = 0; i < numExtensions; i++) {
        GL(const GLubyte *string = glGetStringi(GL_EXTENSIONS, i));
        if (strcmp((const char *) string, extension) == 0) {
            return true;
        }
    }
    return false;
}

typedef void(GL_APIENTRY *PFNGLBUFFERSTORAGEEXTPROC)(GLenum target, GLsizeiptr size,
                                                     const void *data, GLbitfield flags);

typedef void(GL_APIENTRY *PFNGLTEXSTORAGE3DMULTISAMPLEPROC)(GLenum target, GLsizei samples,
                                                            GLenum internalformat, GLsizei width,
                                                            GLsizei height, GLsizei depth,
                                                            GLboolean fixedsamplelocations);

// EGL_KHR_fence_sync, GL_OES_EGL_sync, VG_KHR_EGL_sync
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR;

// GL_EXT_disjoint_timer_query
PFNGLQUERYCOUNTEREXTPROC glQueryCounter;
PFNGLGETQUERYOBJECTI64VEXTPROC glGetQueryObjecti64v;
PFNGLGETQUERYOBJECTUI64VEXTPROC glGetQueryObjectui64v;

// GL_EXT_buffer_storage
PFNGLBUFFERSTORAGEEXTPROC glBufferStorage;

// GL_OVR_multiview
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;

// GL_EXT_multisampled_render_to_texture
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT;

// GL_OVR_multiview_multisampled_render_to_texture
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;

#ifndef GL_ES_VERSION_3_2
PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample;
#endif

#if !defined(EGL_OPENGL_ES3_BIT)
#define EGL_OPENGL_ES3_BIT 0x0040
#endif

// GL_EXT_texture_cube_map_array
#if !defined(GL_TEXTURE_CUBE_MAP_ARRAY)
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#endif

// GL_EXT_texture_filter_anisotropic
#if !defined(GL_TEXTURE_MAX_ANISOTROPY_EXT)
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

// GL_EXT_texture_border_clamp or GL_OES_texture_border_clamp
#if !defined(GL_CLAMP_TO_BORDER)
#define GL_CLAMP_TO_BORDER 0x812D
#endif

// No 1D textures in OpenGL ES.
#if !defined(GL_TEXTURE_1D)
#define GL_TEXTURE_1D 0x0DE0
#endif

// No 1D texture arrays in OpenGL ES.
#if !defined(GL_TEXTURE_1D_ARRAY)
#define GL_TEXTURE_1D_ARRAY 0x8C18
#endif

// No multi-sampled texture arrays in OpenGL ES.
#if !defined(GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#endif

void GlInitExtensions() {
    eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC) GetExtension("eglCreateSyncKHR");
    eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC) GetExtension("eglDestroySyncKHR");
    eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC) GetExtension("eglClientWaitSyncKHR");
    eglGetSyncAttribKHR = (PFNEGLGETSYNCATTRIBKHRPROC) GetExtension("eglGetSyncAttribKHR");

    glQueryCounter = (PFNGLQUERYCOUNTEREXTPROC) GetExtension("glQueryCounterEXT");
    glGetQueryObjecti64v = (PFNGLGETQUERYOBJECTI64VEXTPROC) GetExtension("glGetQueryObjecti64vEXT");
    glGetQueryObjectui64v = (PFNGLGETQUERYOBJECTUI64VEXTPROC) GetExtension(
            "glGetQueryObjectui64vEXT");

    glBufferStorage = (PFNGLBUFFERSTORAGEEXTPROC) GetExtension("glBufferStorageEXT");

    glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) GetExtension(
                    "glRenderbufferStorageMultisampleEXT");
    glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) GetExtension(
                    "glFramebufferTexture2DMultisampleEXT");
    glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) GetExtension(
            "glFramebufferTextureMultiviewOVR");
    glFramebufferTextureMultisampleMultiviewOVR =
            (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) GetExtension(
                    "glFramebufferTextureMultisampleMultiviewOVR");

#ifndef GL_ES_VERSION_3_2
    glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)GetExtension("glTexStorage3DMultisample");
#endif

    glExtensions.timer_query = GlCheckExtension("GL_EXT_disjoint_timer_query");
    glExtensions.texture_clamp_to_border =
            GlCheckExtension("GL_EXT_texture_border_clamp") ||
            GlCheckExtension("GL_OES_texture_border_clamp");
    glExtensions.buffer_storage = GlCheckExtension("GL_EXT_buffer_storage");
    glExtensions.multi_view = GlCheckExtension("GL_OVR_multiview2");
    glExtensions.multi_sampled_resolve = GlCheckExtension("GL_EXT_multisampled_render_to_texture");
    glExtensions.multi_view_multi_sampled_resolve = GlCheckExtension(
            "GL_OVR_multiview_multisampled_render_to_texture");

    glExtensions.texture_clamp_to_border_id =
            (GlCheckExtension("GL_OES_texture_border_clamp")
             ? GL_CLAMP_TO_BORDER
             : (GlCheckExtension("GL_EXT_texture_border_clamp") ? GL_CLAMP_TO_BORDER
                                                                : (GL_CLAMP_TO_EDGE)));
}

/*
================================================================================================================================

Driver Instance.

================================================================================================================================
*/

bool ksDriverInstance_Create(ksDriverInstance *instance) {
    memset(instance, 0, sizeof(ksDriverInstance));
    return true;
}

void ksDriverInstance_Destroy(ksDriverInstance *instance) {
    memset(instance, 0, sizeof(ksDriverInstance));
}

/*
================================================================================================================================

GPU Device.

================================================================================================================================
*/

bool ksGpuDevice_Create(ksGpuDevice *device, ksDriverInstance *instance,
                        const ksGpuQueueInfo *queueInfo) {
    /*
            Use an extensions to select the appropriate device:
            https://www.opengl.org/registry/specs/NV/gpu_affinity.txt
            https://www.opengl.org/registry/specs/AMD/wgl_gpu_association.txt
            https://www.opengl.org/registry/specs/AMD/glx_gpu_association.txt

            On Linux configure each GPU to use a separate X screen and then select
            the X screen to render to.
    */

    memset(device, 0, sizeof(ksGpuDevice));

    device->instance = instance;
    device->queueInfo = *queueInfo;

    return true;
}

void ksGpuDevice_Destroy(ksGpuDevice *device) { memset(device, 0, sizeof(ksGpuDevice)); }

/*
================================================================================================================================

GPU Context.

================================================================================================================================
*/

ksGpuSurfaceBits ksGpuContext_BitsForSurfaceFormat(const ksGpuSurfaceColorFormat colorFormat,
                                                   const ksGpuSurfaceDepthFormat depthFormat) {
    ksGpuSurfaceBits bits;
    bits.redBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                    ? 8
                    : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                       ? 8
                       : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                          ? 5
                          : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 5 : 8))));
    bits.greenBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                      ? 8
                      : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                         ? 8
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                            ? 6
                            : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 6 : 8))));
    bits.blueBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                     ? 8
                     : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                        ? 8
                        : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                           ? 5
                           : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 5 : 8))));
    bits.alphaBits = ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
                      ? 8
                      : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                         ? 8
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                            ? 0
                            : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? 0 : 8))));
    bits.colorBits = bits.redBits + bits.greenBits + bits.blueBits + bits.alphaBits;
    bits.depthBits =
            ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D16) ? 16 : ((depthFormat ==
                                                                       KS_GPU_SURFACE_DEPTH_FORMAT_D24)
                                                                      ? 24 : 0));
    return bits;
}

GLenum ksGpuContext_InternalSurfaceColorFormat(const ksGpuSurfaceColorFormat colorFormat) {
    return ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
            ? GL_RGBA8
            : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
               ? GL_RGBA8
               : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                  ? GL_RGB565
                  : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5) ? GL_RGB565 : GL_RGBA8))));
}

GLenum ksGpuContext_InternalSurfaceDepthFormat(const ksGpuSurfaceDepthFormat depthFormat) {
    return ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D16)
            ? GL_DEPTH_COMPONENT16
            : ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D24) ? GL_DEPTH_COMPONENT24
                                                                : GL_DEPTH_COMPONENT24));
}

static bool ksGpuContext_CreateForSurface(ksGpuContext *context, const ksGpuDevice *device,
                                          const int queueIndex,
                                          const ksGpuSurfaceColorFormat colorFormat,
                                          const ksGpuSurfaceDepthFormat depthFormat,
                                          const ksGpuSampleCount sampleCount, EGLDisplay display) {
    context->device = device;

    context->display = display;

    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig when the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted on the time warped frontbuffer.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    EGL(eglGetConfigs(display, configs, MAX_CONFIGS, &numConfigs));

    const ksGpuSurfaceBits bits = ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

    const EGLint configAttribs[] = {EGL_RED_SIZE, bits.greenBits,
                                    EGL_GREEN_SIZE, bits.redBits,
                                    EGL_BLUE_SIZE, bits.blueBits,
                                    EGL_ALPHA_SIZE, bits.alphaBits,
                                    EGL_DEPTH_SIZE, bits.depthBits,
            // EGL_STENCIL_SIZE,	0,
                                    EGL_SAMPLE_BUFFERS, (sampleCount > KS_GPU_SAMPLE_COUNT_1),
                                    EGL_SAMPLES,
                                    (sampleCount > KS_GPU_SAMPLE_COUNT_1) ? sampleCount : 0,
                                    EGL_NONE};

    context->config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT) != EGL_OPENGL_ES3_BIT) {
            continue;
        }

        // Without EGL_KHR_surfaceless_context, the config needs to support both pbuffers and window surfaces.
        eglGetConfigAttrib(display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            context->config = configs[i];
            break;
        }
    }
    if (context->config == 0) {
        Error("Failed to find EGLConfig");
        return false;
    }

    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, OPENGL_VERSION_MAJOR, EGL_NONE, EGL_NONE,
                               EGL_NONE};
    // Use the default priority if KS_GPU_QUEUE_PRIORITY_MEDIUM is selected.
    const ksGpuQueuePriority priority = device->queueInfo.queuePriorities[queueIndex];
    if (priority != KS_GPU_QUEUE_PRIORITY_MEDIUM) {
        contextAttribs[2] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
        contextAttribs[3] = (priority == KS_GPU_QUEUE_PRIORITY_LOW) ? EGL_CONTEXT_PRIORITY_LOW_IMG
                                                                    : EGL_CONTEXT_PRIORITY_HIGH_IMG;
    }
    context->context = eglCreateContext(display, context->config, EGL_NO_CONTEXT, contextAttribs);
    if (context->context == EGL_NO_CONTEXT) {
        Error("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return false;
    }

    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    context->tinySurface = eglCreatePbufferSurface(display, context->config, surfaceAttribs);
    if (context->tinySurface == EGL_NO_SURFACE) {
        Error("eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(display, context->context);
        context->context = EGL_NO_CONTEXT;
        return false;
    }
    context->mainSurface = context->tinySurface;

    return true;
}

bool ksGpuContext_CreateShared(ksGpuContext *context, const ksGpuContext *other, int queueIndex) {
    UNUSED_PARM(queueIndex);

    memset(context, 0, sizeof(ksGpuContext));

    context->device = other->device;
    context->display = other->display;
    EGLint configID;
    if (!eglQueryContext(context->display, other->context, EGL_CONFIG_ID, &configID)) {
        Error("eglQueryContext EGL_CONFIG_ID failed: %s", EglErrorString(eglGetError()));
        return false;
    }
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    EGL(eglGetConfigs(context->display, configs, MAX_CONFIGS, &numConfigs));
    context->config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;
        eglGetConfigAttrib(context->display, configs[i], EGL_CONFIG_ID, &value);
        if (value == configID) {
            context->config = configs[i];
            break;
        }
    }
    if (context->config == 0) {
        Error("Failed to find share context config.");
        return false;
    }
    EGLint surfaceType = 0;
    eglGetConfigAttrib(context->display, context->config, EGL_SURFACE_TYPE, &surfaceType);

#if defined(OS_ANDROID)
    if ((surfaceType & EGL_PBUFFER_BIT) == 0) {
        Error("Share context config does have EGL_PBUFFER_BIT.");
        return false;
    }
#endif
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, OPENGL_VERSION_MAJOR, EGL_NONE};
    context->context = eglCreateContext(context->display, context->config, other->context,
                                        contextAttribs);
    if (context->context == EGL_NO_CONTEXT) {
        Error("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return false;
    }
#if defined(OS_ANDROID)
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    context->tinySurface = eglCreatePbufferSurface(context->display, context->config,
                                                   surfaceAttribs);
    if (context->tinySurface == EGL_NO_SURFACE) {
        Error("eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(context->display, context->context);
        context->context = EGL_NO_CONTEXT;
        return false;
    }
    context->mainSurface = context->tinySurface;
#endif
    return true;
}

void ksGpuContext_Destroy(ksGpuContext *context) {
    if (context->display != 0) {
        EGL(eglMakeCurrent(context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    }
    if (context->context != EGL_NO_CONTEXT) {
        EGL(eglDestroyContext(context->display, context->context));
    }

    if (context->mainSurface != context->tinySurface) {
        EGL(eglDestroySurface(context->display, context->mainSurface));
    }
    if (context->tinySurface != EGL_NO_SURFACE) {
        EGL(eglDestroySurface(context->display, context->tinySurface));
    }
    context->tinySurface = EGL_NO_SURFACE;
    context->display = 0;
    context->config = 0;
    context->mainSurface = EGL_NO_SURFACE;
    context->context = EGL_NO_CONTEXT;
}

void ksGpuContext_WaitIdle(ksGpuContext *context) {
    UNUSED_PARM(context);

    GL(glFinish());
}

void ksGpuContext_SetCurrent(ksGpuContext *context) {
    EGL(eglMakeCurrent(context->display, context->mainSurface, context->mainSurface,
                       context->context));
}

void ksGpuContext_UnsetCurrent(ksGpuContext *context) {
    EGL(eglMakeCurrent(context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

bool ksGpuContext_CheckCurrent(ksGpuContext *context) {
    return (eglGetCurrentContext() == context->context);
}

static void ksGpuContext_GetLimits(ksGpuContext *context, ksGpuLimits *limits) {
    UNUSED_PARM(context);

    limits->maxPushConstantsSize = 512;
    limits->maxSamples = glGetInteger(GL_MAX_SAMPLES);
}

/*
================================================================================================================================

GPU Window.

================================================================================================================================
*/
typedef enum  // https://developer.android.com/ndk/reference/group___input.html
{
    KEY_A = AKEYCODE_A,
    KEY_B = AKEYCODE_B,
    KEY_C = AKEYCODE_C,
    KEY_D = AKEYCODE_D,
    KEY_E = AKEYCODE_E,
    KEY_F = AKEYCODE_F,
    KEY_G = AKEYCODE_G,
    KEY_H = AKEYCODE_H,
    KEY_I = AKEYCODE_I,
    KEY_J = AKEYCODE_J,
    KEY_K = AKEYCODE_K,
    KEY_L = AKEYCODE_L,
    KEY_M = AKEYCODE_M,
    KEY_N = AKEYCODE_N,
    KEY_O = AKEYCODE_O,
    KEY_P = AKEYCODE_P,
    KEY_Q = AKEYCODE_Q,
    KEY_R = AKEYCODE_R,
    KEY_S = AKEYCODE_S,
    KEY_T = AKEYCODE_T,
    KEY_U = AKEYCODE_U,
    KEY_V = AKEYCODE_V,
    KEY_W = AKEYCODE_W,
    KEY_X = AKEYCODE_X,
    KEY_Y = AKEYCODE_Y,
    KEY_Z = AKEYCODE_Z,
    KEY_RETURN = AKEYCODE_ENTER,
    KEY_TAB = AKEYCODE_TAB,
    KEY_ESCAPE = AKEYCODE_ESCAPE,
    KEY_SHIFT_LEFT = AKEYCODE_SHIFT_LEFT,
    KEY_CTRL_LEFT = AKEYCODE_CTRL_LEFT,
    KEY_ALT_LEFT = AKEYCODE_ALT_LEFT,
    KEY_CURSOR_UP = AKEYCODE_DPAD_UP,
    KEY_CURSOR_DOWN = AKEYCODE_DPAD_DOWN,
    KEY_CURSOR_LEFT = AKEYCODE_DPAD_LEFT,
    KEY_CURSOR_RIGHT = AKEYCODE_DPAD_RIGHT
} ksKeyboardKey;

typedef enum {
    MOUSE_LEFT = 0, MOUSE_RIGHT = 1
} ksMouseButton;

static void app_handle_cmd(struct android_app *app, int32_t cmd) {
    ksGpuWindow *window = (ksGpuWindow *) app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            Print("onStart()");
            Print("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            Print("onResume()");
            Print("    APP_CMD_RESUME");
            window->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            Print("onPause()");
            Print("    APP_CMD_PAUSE");
            window->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            Print("onStop()");
            Print("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            Print("onDestroy()");
            Print("    APP_CMD_DESTROY");
            window->nativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            Print("xmh surfaceCreated()");
            Print("xmh    APP_CMD_INIT_WINDOW");
            window->nativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            Print("surfaceDestroyed()");
            Print("    APP_CMD_TERM_WINDOW");
            window->nativeWindow = NULL;
            break;
        }
    }
}

static int32_t app_handle_input(struct android_app *app, AInputEvent *event) {
    ksGpuWindow *window = (ksGpuWindow *) app->userData;

    const int type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        int keyCode = AKeyEvent_getKeyCode(event);
        const int action = AKeyEvent_getAction(event);
        if (action == AKEY_EVENT_ACTION_DOWN) {
            // Translate controller input to useful keys.
            switch (keyCode) {
                case AKEYCODE_BUTTON_A:
                    keyCode = AKEYCODE_Q;
                    break;
                case AKEYCODE_BUTTON_B:
                    keyCode = AKEYCODE_W;
                    break;
                case AKEYCODE_BUTTON_X:
                    keyCode = AKEYCODE_E;
                    break;
                case AKEYCODE_BUTTON_Y:
                    keyCode = AKEYCODE_M;
                    break;
                case AKEYCODE_BUTTON_START:
                    keyCode = AKEYCODE_L;
                    break;
                case AKEYCODE_BUTTON_SELECT:
                    keyCode = AKEYCODE_ESCAPE;
                    break;
            }
            if (keyCode >= 0 && keyCode < 256) {
                window->input.keyInput[keyCode] = true;
                return 1;
            }
        }
        return 0;
    } else if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int source = AInputEvent_getSource(event);
        // Events with source == AINPUT_SOURCE_TOUCHSCREEN come from the phone's builtin touch screen.
        // Events with source == AINPUT_SOURCE_MOUSE come from the trackpad on the right side of the GearVR.
        if (source == AINPUT_SOURCE_TOUCHSCREEN || source == AINPUT_SOURCE_MOUSE) {
            const int action = AKeyEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
            const float x = AMotionEvent_getRawX(event, 0);
            const float y = AMotionEvent_getRawY(event, 0);
            if (action == AMOTION_EVENT_ACTION_UP) {
                window->input.mouseInput[MOUSE_LEFT] = true;
                window->input.mouseInputX[MOUSE_LEFT] = (int) x;
                window->input.mouseInputY[MOUSE_LEFT] = (int) y;
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

void ksGpuWindow_Destroy(ksGpuWindow *window) {
    ksGpuContext_Destroy(&window->context);
    ksGpuDevice_Destroy(&window->device);

    if (window->display != 0) {
        EGL(eglTerminate(window->display));
        window->display = 0;
    }

    if (window->app != NULL) {
        (*window->java.vm)->DetachCurrentThread(window->java.vm);
        window->java.vm = NULL;
        window->java.env = NULL;
        window->java.activity = 0;
    }
}

static float GetDisplayRefreshRate(const Java_t *java) {
    // Retrieve Context.WINDOW_SERVICE.
    jclass contextClass = (*java->env)->FindClass(java->env, "android/content/Context");
    jfieldID field_WINDOW_SERVICE = (*java->env)->GetStaticFieldID(java->env, contextClass,
                                                                   "WINDOW_SERVICE",
                                                                   "Ljava/lang/String;");
    jobject WINDOW_SERVICE = (*java->env)->GetStaticObjectField(java->env, contextClass,
                                                                field_WINDOW_SERVICE);
    (*java->env)->DeleteLocalRef(java->env, contextClass);

    // WindowManager windowManager = (WindowManager) activity.getSystemService( Context.WINDOW_SERVICE );
    const jclass activityClass = (*java->env)->GetObjectClass(java->env, java->activity);
    const jmethodID getSystemServiceMethodId =
            (*java->env)->GetMethodID(java->env, activityClass, "getSystemService",
                                      "(Ljava/lang/String;)Ljava/lang/Object;");
    const jobject windowManager =
            (*java->env)->CallObjectMethod(java->env, java->activity, getSystemServiceMethodId,
                                           WINDOW_SERVICE);
    (*java->env)->DeleteLocalRef(java->env, activityClass);

    // Display display = windowManager.getDefaultDisplay();
    const jclass windowManagerClass = (*java->env)->GetObjectClass(java->env, windowManager);
    const jmethodID getDefaultDisplayMethodId =
            (*java->env)->GetMethodID(java->env, windowManagerClass, "getDefaultDisplay",
                                      "()Landroid/view/Display;");
    const jobject display = (*java->env)->CallObjectMethod(java->env, windowManager,
                                                           getDefaultDisplayMethodId);
    (*java->env)->DeleteLocalRef(java->env, windowManagerClass);

    // float refreshRate = display.getRefreshRate();
    const jclass displayClass = (*java->env)->GetObjectClass(java->env, display);
    const jmethodID getRefreshRateMethodId = (*java->env)->GetMethodID(java->env, displayClass,
                                                                       "getRefreshRate", "()F");
    const float refreshRate = (*java->env)->CallFloatMethod(java->env, display,
                                                            getRefreshRateMethodId);
    (*java->env)->DeleteLocalRef(java->env, displayClass);

    (*java->env)->DeleteLocalRef(java->env, display);
    (*java->env)->DeleteLocalRef(java->env, windowManager);
    (*java->env)->DeleteLocalRef(java->env, WINDOW_SERVICE);

    return refreshRate;
}

struct android_app *global_app;

bool
ksGpuWindow_Create(ksGpuWindow *window, ksDriverInstance *instance, const ksGpuQueueInfo *queueInfo,
                   const int queueIndex,
                   const ksGpuSurfaceColorFormat colorFormat,
                   const ksGpuSurfaceDepthFormat depthFormat,
                   const ksGpuSampleCount sampleCount, const int width, const int height,
                   const bool fullscreen) {
    memset(window, 0, sizeof(ksGpuWindow));

    window->colorFormat = colorFormat;
    window->depthFormat = depthFormat;
    window->sampleCount = sampleCount;
    window->windowWidth = width;
    window->windowHeight = height;
    window->windowSwapInterval = 1;
    window->windowRefreshRate = 60.0f;
    window->windowFullscreen = true;
    window->windowActive = false;
    window->windowExit = false;
    window->lastSwapTime = GetTimeNanoseconds();

    window->app = global_app;
    window->nativeWindow = window->nativeWindow;
    Print("ksGpuWindow_Create app(%p),window->nativeWindow(%p)", window->app, window->nativeWindow);
    window->resumed = false;

    if (window->app != NULL) {
        window->app->userData = window;
        window->app->onAppCmd = app_handle_cmd;
        window->app->onInputEvent = app_handle_input;
        window->java.vm = window->app->activity->vm;
        window->nativeWindow = window->app->window;
        (*window->java.vm)->AttachCurrentThread(window->java.vm, &window->java.env, NULL);
        window->java.activity = window->app->activity->clazz;

        window->windowRefreshRate = GetDisplayRefreshRate(&window->java);

        // Keep the display on and bright.
        // Also make sure there is only one "HWC" next to the "FB TARGET" (adb shell dumpsys SurfaceFlinger).
        ANativeActivity_setWindowFlags(window->app->activity,
                                       AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    }

    window->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGL(eglInitialize(window->display, &window->majorVersion, &window->minorVersion));

    ksGpuDevice_Create(&window->device, instance, queueInfo);
    ksGpuContext_CreateForSurface(&window->context, &window->device, queueIndex, colorFormat,
                                  depthFormat, sampleCount,
                                  window->display);
    ksGpuContext_SetCurrent(&window->context);

    GlInitExtensions();

    return true;
}

static bool ksGpuWindow_SupportedResolution(const int width, const int height) {
    UNUSED_PARM(width);
    UNUSED_PARM(height);

    // Assume the HWC can handle any window size.
    return true;
}

void ksGpuWindow_Exit(ksGpuWindow *window) {
    // Call finish() on the activity and ksGpuWindow_ProcessEvents will handle the rest.
    ANativeActivity_finish(window->app->activity);
}

ksGpuWindowEvent ksGpuWindow_ProcessEvents(ksGpuWindow *window) {
    if (window->app == NULL) {
        return KS_GPU_WINDOW_EVENT_NONE;
    }

    const bool windowWasActive = window->windowActive;

    for (;;) {
        int events;
        struct android_poll_source *source;
        const int timeoutMilliseconds = (window->windowActive == false &&
                                         window->app->destroyRequested == 0) ? -1 : 0;
        if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void **) &source) < 0) {
            break;
        }

        if (source != NULL) {
            source->process(window->app, source);
        }

        if (window->nativeWindow != NULL &&
            window->context.mainSurface == window->context.tinySurface) {
            Print("        ANativeWindow_setBuffersGeometry %d x %d", window->windowWidth,
                  window->windowHeight);
            ANativeWindow_setBuffersGeometry(window->nativeWindow, window->windowWidth,
                                             window->windowHeight, 0);

            const EGLint surfaceAttribs[] = {EGL_NONE};
            Print("        mainSurface = eglCreateWindowSurface( nativeWindow )");
            window->context.mainSurface =
                    eglCreateWindowSurface(window->context.display, window->context.config,
                                           window->nativeWindow, surfaceAttribs);
            if (window->context.mainSurface == EGL_NO_SURFACE) {
                Error("        eglCreateWindowSurface() failed: %s", EglErrorString(eglGetError()));
                return KS_GPU_WINDOW_EVENT_EXIT;
            }
            Print("        eglMakeCurrent( mainSurface )");
            EGL(eglMakeCurrent(window->context.display, window->context.mainSurface,
                               window->context.mainSurface,
                               window->context.context));

            eglQuerySurface(window->context.display, window->context.mainSurface, EGL_WIDTH,
                            &window->windowWidth);
            eglQuerySurface(window->context.display, window->context.mainSurface, EGL_HEIGHT,
                            &window->windowHeight);
        }

        if (window->resumed != false && window->nativeWindow != NULL) {
            window->windowActive = true;
        } else {
            window->windowActive = false;
        }

        if (window->nativeWindow == NULL &&
            window->context.mainSurface != window->context.tinySurface) {
            Print("        eglMakeCurrent( tinySurface )");
            EGL(eglMakeCurrent(window->context.display, window->context.tinySurface,
                               window->context.tinySurface,
                               window->context.context));
            Print("        eglDestroySurface( mainSurface )");
            EGL(eglDestroySurface(window->context.display, window->context.mainSurface));
            window->context.mainSurface = window->context.tinySurface;
        }
    }

    if (window->app->destroyRequested != 0) {
        return KS_GPU_WINDOW_EVENT_EXIT;
    }
    if (windowWasActive != window->windowActive) {
        return (window->windowActive) ? KS_GPU_WINDOW_EVENT_ACTIVATED
                                      : KS_GPU_WINDOW_EVENT_DEACTIVATED;
    }
    return KS_GPU_WINDOW_EVENT_NONE;
}

void ksGpuWindow_SwapInterval(ksGpuWindow *window, int swapInterval) {
    if (swapInterval != window->windowSwapInterval) {
        EGL(eglSwapInterval(window->context.display, swapInterval));
        window->windowSwapInterval = swapInterval;
    }
}

void ksGpuWindow_SwapBuffers(ksGpuWindow *window) {
    EGL(eglSwapBuffers(window->context.display, window->context.mainSurface));

    ksNanoseconds newTimeNanoseconds = GetTimeNanoseconds();

    // Even with smoothing, this is not particularly accurate.
    const float frameTimeNanoseconds = 1000.0f * 1000.0f * 1000.0f / window->windowRefreshRate;
    const float deltaTimeNanoseconds =
            (float) newTimeNanoseconds - window->lastSwapTime - frameTimeNanoseconds;
    if (fabsf(deltaTimeNanoseconds) < frameTimeNanoseconds * 0.75f) {
        newTimeNanoseconds = (ksNanoseconds) (window->lastSwapTime + frameTimeNanoseconds +
                                              0.025f * deltaTimeNanoseconds);
    }
    // const float smoothDeltaNanoseconds = (float)( newTimeNanoseconds - window->lastSwapTime );
    // Print( "frame delta = %1.3f (error = %1.3f)\n", smoothDeltaNanoseconds * 1e-6f,
    //					( smoothDeltaNanoseconds - frameTimeNanoseconds ) * 1e-6f );
    window->lastSwapTime = newTimeNanoseconds;
}

ksNanoseconds ksGpuWindow_GetNextSwapTimeNanoseconds(ksGpuWindow *window) {
    const float frameTimeNanoseconds = 1000.0f * 1000.0f * 1000.0f / window->windowRefreshRate;
    return window->lastSwapTime + (ksNanoseconds) (frameTimeNanoseconds);
}

ksNanoseconds ksGpuWindow_GetFrameTimeNanoseconds(ksGpuWindow *window) {
    const float frameTimeNanoseconds = 1000.0f * 1000.0f * 1000.0f / window->windowRefreshRate;
    return (ksNanoseconds) (frameTimeNanoseconds);
}

void ksGpuWindow_DelayBeforeSwap(ksGpuWindow *window, const ksNanoseconds delay) {
    UNUSED_PARM(window);
    UNUSED_PARM(delay);

    // FIXME: this appears to not only stall the calling context but also other contexts.
    /*
    #if defined( OS_WINDOWS )
            if ( wglDelayBeforeSwapNV != NULL )
            {
                    wglDelayBeforeSwapNV( window->hDC, delay * 1e-6f );
            }
    #elif defined( OS_LINUX_XLIB )
            if ( glXDelayBeforeSwapNV != NULL )
            {
                    glXDelayBeforeSwapNV( window->hDC, delay * 1e-6f );
            }
    #endif
    */
}

static bool ksGpuWindowInput_ConsumeKeyboardKey(ksGpuWindowInput *input, const ksKeyboardKey key) {
    if (input->keyInput[key]) {
        input->keyInput[key] = false;
        return true;
    }
    return false;
}

static bool
ksGpuWindowInput_ConsumeMouseButton(ksGpuWindowInput *input, const ksMouseButton button) {
    if (input->mouseInput[button]) {
        input->mouseInput[button] = false;
        return true;
    }
    return false;
}

static bool ksGpuWindowInput_CheckKeyboardKey(ksGpuWindowInput *input, const ksKeyboardKey key) {
    return (input->keyInput[key] != false);
}

/*
================================================================================================================================

GPU timer.

================================================================================================================================
*/

void ksGpuTimer_Create(ksGpuContext *context, ksGpuTimer *timer) {
    UNUSED_PARM(context);

    if (glExtensions.timer_query) {
        GL(glGenQueries(KS_GPU_TIMER_FRAMES_DELAYED, timer->beginQueries));
        GL(glGenQueries(KS_GPU_TIMER_FRAMES_DELAYED, timer->endQueries));
        timer->queryIndex = 0;
        timer->gpuTime = 0;
    }
}

void ksGpuTimer_Destroy(ksGpuContext *context, ksGpuTimer *timer) {
    UNUSED_PARM(context);

    if (glExtensions.timer_query) {
        GL(glDeleteQueries(KS_GPU_TIMER_FRAMES_DELAYED, timer->beginQueries));
        GL(glDeleteQueries(KS_GPU_TIMER_FRAMES_DELAYED, timer->endQueries));
    }
}

ksNanoseconds ksGpuTimer_GetNanoseconds(ksGpuTimer *timer) {
    if (glExtensions.timer_query) {
        return timer->gpuTime;
    } else {
        return 0;
    }
}
