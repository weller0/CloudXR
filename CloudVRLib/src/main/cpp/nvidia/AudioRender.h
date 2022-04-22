//
// Created by arena on 2022-4-12.
//

#ifndef CLOUDXRDEMO_AUDIORENDER_H
#define CLOUDXRDEMO_AUDIORENDER_H

#include <aaudio/AAudio.h>

namespace ssnwt {
    class AudioRender {
    public:
        AudioRender();

        void write(const void *buffer, int32_t numFrames, int64_t timeoutNanoseconds);

        ~AudioRender();

    private:
        AAudioStream *stream;
    };
}

#endif //CLOUDXRDEMO_AUDIORENDER_H
