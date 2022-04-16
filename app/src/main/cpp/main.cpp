//BEGIN_INCLUDE(all)
#include <android_native_app_glue.h>
#include "log.h"

ANativeWindow *NativeWindow;

#define CASE(x) case x: return #x

const char *appCmdToString(int32_t cmd) {
    switch (cmd) {
        CASE(APP_CMD_SAVE_STATE);
        CASE(APP_CMD_START);
        CASE(APP_CMD_RESUME);
        CASE(APP_CMD_PAUSE);
        CASE(APP_CMD_STOP);
        CASE(APP_CMD_DESTROY);
        CASE(APP_CMD_INIT_WINDOW);
        CASE(APP_CMD_TERM_WINDOW);
        default:
            return "Other cmd";
    }
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app *app, int32_t cmd) {
    ALOGD("engine_handle_cmd %s", appCmdToString(cmd));
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            NativeWindow = app->window;
            break;
        case APP_CMD_DESTROY:
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            NativeWindow = nullptr;
            break;
        default:
            break;
    }
}

#include <memory>
#include <string>
#include <openxr/openxr_platform.h>
#include <chrono>
#include <thread>
#include "platformplugin.h"
#include "graphicsplugin.h"
#include "openxr_program.h"

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app *state) {
    try {
        JNIEnv *Env;
        state->activity->vm->AttachCurrentThread(&Env, nullptr);

        state->onAppCmd = engine_handle_cmd;

        std::shared_ptr<Options> options = std::make_shared<Options>();
        std::shared_ptr<PlatformData> data = std::make_shared<PlatformData>();
        data->applicationVM = state->activity->vm;
        data->applicationActivity = state->activity->clazz;

        std::shared_ptr<IPlatformPlugin> platformPlugin = CreatePlatformPlugin(options, data);
        std::shared_ptr<IGraphicsPlugin> graphicsPlugin = CreateGraphicsPlugin(options,
                                                                               platformPlugin);
        std::shared_ptr<IOpenXrProgram> program = CreateOpenXrProgram(options, platformPlugin,
                                                                      graphicsPlugin);
        // Initialize the loader for this platform
        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        if (XR_SUCCEEDED(
                xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                      (PFN_xrVoidFunction *) (&initializeLoader)))) {
            XrLoaderInitInfoAndroidKHR loaderInitInfoAndroid;
            memset(&loaderInitInfoAndroid, 0, sizeof(loaderInitInfoAndroid));
            loaderInitInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
            loaderInitInfoAndroid.next = XR_NULL_HANDLE;
            loaderInitInfoAndroid.applicationVM = state->activity->vm;
            loaderInitInfoAndroid.applicationContext = state->activity->clazz;
            initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *) &loaderInitInfoAndroid);
        }
        program->CreateInstance();
        program->InitializeSystem();
        program->InitializeSession();
        program->CreateSwapchains();
        bool requestRestart = false;
        bool exitRenderLoop = false;
        while (state->destroyRequested == 0) {
            for (;;) {
                int events;
                struct android_poll_source *source;
                // If the timeout is zero, returns immediately without blocking.
                // If the timeout is negative, waits indefinitely until an event appears.
                const int timeoutMilliseconds =
                        (NativeWindow == nullptr && !program->IsSessionRunning() &&
                         state->destroyRequested == 0) ? -1 : 0;
                if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void **) &source) < 0) {
                    break;
                }
                // Process this event.
                if (source != nullptr) {
                    source->process(state, source);
                }
            }

            program->PollEvents(&exitRenderLoop, &requestRestart);
            if (!program->IsSessionRunning()) {
                // Throttle loop since xrWaitFrame won't be called.
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                continue;
            }

            program->PollActions();
            program->RenderFrame();
        }
    } catch (const std::exception &ex) {
        ALOGE("exception:%s", ex.what());
    } catch (...) {
        ALOGE("exception:Unknown Error");
    }
}
//END_INCLUDE(all)