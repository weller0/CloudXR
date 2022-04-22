#include "AudioRender.h"
#include "CloudXRCommon.h"
#include "log.h"

namespace ssnwt {
    AudioRender::AudioRender() {
        AAudioStreamBuilder *builder;
        AAudio_createStreamBuilder(&builder);
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_EXCLUSIVE);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setChannelCount(builder, 2);
        AAudioStreamBuilder_setSampleRate(builder, CXR_AUDIO_SAMPLING_RATE);
        AAudioStreamBuilder_openStream(builder, &stream);
        AAudioStream_setBufferSizeInFrames(stream, AAudioStream_getFramesPerBurst(stream) * 2);
        AAudioStream_requestStart(stream);
        ALOGE("Create AudioRender");
    }

    AudioRender::~AudioRender() {
        if (stream) AAudioStream_close(stream);
        stream = nullptr;
        ALOGE("Delete AudioRender");
    }

    void AudioRender::write(const void *buffer, int32_t numFrames, int64_t timeoutNanoseconds) {
        if (stream) {
            AAudioStream_write(stream, buffer, numFrames, timeoutNanoseconds);
        }
    }
}