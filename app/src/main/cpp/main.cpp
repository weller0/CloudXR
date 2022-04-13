//BEGIN_INCLUDE(all)
#include <android_native_app_glue.h>
#include "GraphicRender.h"
#include "EGLHelper.h"
#include "log.h"

ssnwt::EGLHelper eglHelper;
ssnwt::GraphicRender graphicRender;

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
            if (app->window != nullptr) {
                eglHelper.initializeEGL(app->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            eglHelper.releaseEGL();
            break;
        default:
            break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app *state) {
    state->onAppCmd = engine_handle_cmd;

    // loop waiting for stuff to do.
    while (true) {
        // Read all pending events.
        int events;
        struct android_poll_source *source;
        while ((ALooper_pollAll(eglHelper.isReady() ? 0 : -1, nullptr, &events,
                                (void **) &source)) >= 0) {

            // Process this event.
            if (source != nullptr) {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                eglHelper.releaseEGL();
                return;
            }
        }

        if (eglHelper.isReady()) {
            graphicRender.clear();
            eglHelper.swapBuffer();
        }
    }
}
//END_INCLUDE(all)