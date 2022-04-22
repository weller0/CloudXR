//
// Created by arena on 2022-4-9.
//

#ifndef CLOUDXRDEMO_SENSORUTILS_H
#define CLOUDXRDEMO_SENSORUTILS_H

#include <cstdio>
#include "log.h"


namespace ssnwt {

#define SAVE_COUNT 10000
    FILE *imuFile = nullptr;
    int64_t imuCount = 0;

    void writeImu(cxrTrackedDevicePose pose) {
        imuCount++;
        if (imuCount > SAVE_COUNT) {
            if (imuFile) {
                ALOGE("writeImu fclose");
                fclose(imuFile);
                imuFile = nullptr;
            }
            return;
        }
        if (imuFile == nullptr) {
            ALOGE("writeImu fopen");
            imuFile = fopen("/sdcard/Download/imu.txt", "w");
        }
        if (imuCount < 5) {
            ALOGE("------ isValid:%d, isConnected=%d", pose.poseIsValid, pose.deviceIsConnected);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[0][0],
                  pose.deviceToAbsoluteTracking.m[0][1],
                  pose.deviceToAbsoluteTracking.m[0][2],
                  pose.deviceToAbsoluteTracking.m[0][3]);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[1][0],
                  pose.deviceToAbsoluteTracking.m[1][1],
                  pose.deviceToAbsoluteTracking.m[1][2],
                  pose.deviceToAbsoluteTracking.m[1][3]);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[2][0],
                  pose.deviceToAbsoluteTracking.m[2][1],
                  pose.deviceToAbsoluteTracking.m[2][2],
                  pose.deviceToAbsoluteTracking.m[2][3]);
            ALOGE("velocity: %0.3f, %0.3f, %0.3f",
                  pose.velocity.v[0], pose.velocity.v[1], pose.velocity.v[2]);
            ALOGE("angularVelocity: %0.3f, %0.3f, %0.3f",
                  pose.angularVelocity.v[0], pose.angularVelocity.v[1], pose.angularVelocity.v[2]);
        }
        fwrite(&pose, 1, sizeof(cxrTrackedDevicePose), imuFile);
    }

    cxrTrackedDevicePose readImu() {
        cxrTrackedDevicePose pose;
        imuCount++;
        if (imuCount > SAVE_COUNT) {
            if (imuFile) {
                ALOGE("readImu fclose");
                fclose(imuFile);
                imuFile = nullptr;
                imuCount = 0;
            }
            return pose;
        }
        if (imuFile == nullptr) {
            ALOGE("readImu fopen");
            imuFile = fopen(
//                    "/sdcard/Download/imu.txt",
                    "/storage/emulated/0/Android/data/com.ssnwt.cloudxr/files/Movies/imu.txt",
                    "r");
            if (imuFile == nullptr) {
                ALOGE("readImu fopen failed!");
            }
        }
        if (fread(&pose, 1, sizeof(cxrTrackedDevicePose), imuFile) <= 0) {
            imuCount = SAVE_COUNT + 1;
        } else if (imuCount < 5) {
            ALOGE("------ isValid:%d, isConnected=%d", pose.poseIsValid, pose.deviceIsConnected);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[0][0],
                  pose.deviceToAbsoluteTracking.m[0][1],
                  pose.deviceToAbsoluteTracking.m[0][2],
                  pose.deviceToAbsoluteTracking.m[0][3]);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[1][0],
                  pose.deviceToAbsoluteTracking.m[1][1],
                  pose.deviceToAbsoluteTracking.m[1][2],
                  pose.deviceToAbsoluteTracking.m[1][3]);
            ALOGE("deviceToAbsoluteTracking: %0.3f, %0.3f, %0.3f, %0.3f",
                  pose.deviceToAbsoluteTracking.m[2][0],
                  pose.deviceToAbsoluteTracking.m[2][1],
                  pose.deviceToAbsoluteTracking.m[2][2],
                  pose.deviceToAbsoluteTracking.m[2][3]);
            ALOGE("velocity: %0.3f, %0.3f, %0.3f",
                  pose.velocity.v[0], pose.velocity.v[1], pose.velocity.v[2]);
            ALOGE("angularVelocity: %0.3f, %0.3f, %0.3f",
                  pose.angularVelocity.v[0], pose.angularVelocity.v[1], pose.angularVelocity.v[2]);
        }
        return pose;
    }
}


#endif //CLOUDXRDEMO_SENSORUTILS_H
