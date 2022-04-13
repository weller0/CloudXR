//
// Created by arena on 2022-4-13.
//

#include "GraphicRender.h"

namespace ssnwt {
    void GraphicRender::clear() {
        glClearColor(0.2, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}