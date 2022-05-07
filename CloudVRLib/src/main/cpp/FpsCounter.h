#ifndef CLOUDXR_FPSCOUNTER_H
#define CLOUDXR_FPSCOUNTER_H

namespace ssnwt {
#define PRINT_FPS_TIME_MS   1000
#ifdef PRINT_FPS_TIME_MS
    uint32_t frameCount = 0;
    uint64_t lastLogTimeMs = 0;
#endif // PRINT_FPS_TIME_MS

    static uint64_t getTimeMs() {
        struct timeval time;
        gettimeofday(&time, nullptr);
        return time.tv_sec * 1000 + time.tv_usec / 1000;
    }

    void frameStart() {
#ifdef PRINT_FPS_TIME_MS
        frameCount++;
#endif // PRINT_FPS_TIME_MS
    }

    void frameEnd() {
#ifdef PRINT_FPS_TIME_MS
        uint64_t time = getTimeMs();
        if (time - lastLogTimeMs > PRINT_FPS_TIME_MS) {
            ALOGD("fps : %f", frameCount * 1000.0 / (time - lastLogTimeMs));
            lastLogTimeMs = time;
            frameCount = 0;
        }
#endif // PRINT_FPS_TIME_MS
    }
}

#endif //CLOUDXR_FPSCOUNTER_H
