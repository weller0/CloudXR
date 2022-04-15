/*
================================================================================================

Description	:	Time in nanoseconds.
Author		:	J.M.P. van Waveren
Date		:	12/10/2016
Language	:	C99
Format		:	Real tabs with the tab size equal to 4 spaces.
Copyright	:	Copyright (c) 2016 Oculus VR, LLC. All Rights reserved.


LICENSE
=======

Copyright (c) 2016 Oculus VR, LLC.

SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

================================================================================================
*/

#if !defined( KSNANOSECONDS_H )
#define KSNANOSECONDS_H

#include <stdint.h>

typedef uint64_t ksNanoseconds;

static ksNanoseconds GetTimeNanoseconds() {
    static ksNanoseconds timeBase = 0;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (timeBase == 0) {
        timeBase = (ksNanoseconds) ts.tv_sec * 1000ULL * 1000ULL * 1000ULL + ts.tv_nsec;
    }

    return (ksNanoseconds) ts.tv_sec * 1000ULL * 1000ULL * 1000ULL + ts.tv_nsec - timeBase;
}

#endif // !KSNANOSECONDS_H
