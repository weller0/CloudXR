#include "GraphicRender.h"
#include "log.h"

namespace ssnwt {
    void GraphicRender::clear() {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void GraphicRender::initialize(int32_t width, int32_t height) {
        mWidth = width;
        mHeight = height;
        createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
        createTexture();
    }

    void GraphicRender::draw(const uint32_t eye) {
        glUseProgram(mProgram);
        checkGlError("glUseProgram");
        glBindTexture(GL_TEXTURE_2D, mTextureID[eye]);
        draw(PositionVertex[eye], TextureVertex);
        glUseProgram(0);
    }

    void GraphicRender::draw(const float *position, const float *uv) {
        glVertexAttribPointer(maPositionHandle, 3, GL_FLOAT, false, 3 * 4, position);
        checkGlError("glVertexAttribPointer maPosition");
        glEnableVertexAttribArray(maPositionHandle);
        checkGlError("glEnableVertexAttribArray maPositionHandle");

        glVertexAttribPointer(maTextureHandle, 2, GL_FLOAT, false, 2 * 4, uv);
        checkGlError("glVertexAttribPointer maTextureHandle");
        glEnableVertexAttribArray(maTextureHandle);
        checkGlError("glEnableVertexAttribArray maTextureHandle");

        glUniformMatrix4fv(muMVPMatrixHandle, 1, false, mMVPMatrix);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        checkGlError("glDrawArrays");
    }

    bool GraphicRender::setupFrameBuffer(int32_t eye, int32_t width, int32_t height) {
        GLuint texture = mTextureID[eye];
        auto it = frameBuffers.find(texture);

        if (it == frameBuffers.end()) {
            GLuint framebuffer, rbo;
            glGenFramebuffers(1, &framebuffer);
            glGenRenderbuffers(1, &rbo);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, rbo);

            GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                ALOGV("[GraphicRender]Incomplete frame buffer object!");
                return false;
            }

            frameBuffers[texture] = framebuffer;
            it = frameBuffers.find(texture);

            ALOGV("[GraphicRender]Created FBO %d for texture:%d, size(%d, %d).",
                  framebuffer, texture, width, height);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, it->second);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        return true;
    }

    void GraphicRender::bindDefaultFrameBuffer() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    void GraphicRender::drawFbo() {
        for (int eye = 0; eye < 2; eye++) {
            if (setupFrameBuffer(eye, mWidth / 2, mHeight)) {
                glClearColor(0.0f, eye == 0 ? 0.3f : 0, eye == 0 ? 0 : 0.3f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            bindDefaultFrameBuffer();
        }
    }

    void GraphicRender::drawFbo(const uint32_t eye) {
        if (setupFrameBuffer(eye, mWidth / 2, mHeight)) {
            glClearColor(0.0f, eye == 0 ? 0.3f : 0, eye == 0 ? 0 : 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        bindDefaultFrameBuffer();
    }

    void GraphicRender::createTexture() {
        glGenTextures(2, mTextureID);

        // tex L
        glBindTexture(GL_TEXTURE_2D, mTextureID[0]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mWidth / 2, mHeight,
                     0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        checkGlError("create l");

        // tex R
        glBindTexture(GL_TEXTURE_2D, mTextureID[1]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mWidth / 2, mHeight,
                     0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        checkGlError("create r");
    }

    void GraphicRender::createProgram(const char *vertexSource, const char *fragmentSource) {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, nullptr);
        glCompileShader(vertexShader);
        checkShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
        glCompileShader(fragmentShader);
        checkShader(fragmentShader);

        mProgram = glCreateProgram();
        glAttachShader(mProgram, vertexShader);
        glAttachShader(mProgram, fragmentShader);
        glLinkProgram(mProgram);
        checkProgram(mProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        muMVPMatrixHandle = glGetUniformLocation(mProgram, "uMVPMatrix");
        checkGlError("glGetUniformLocation uMVPMatrix");
        maPositionHandle = glGetAttribLocation(mProgram, "aPosition");
        checkGlError("glGetAttribLocation aPosition");
        maTextureHandle = glGetAttribLocation(mProgram, "aTextureCoord");
        checkGlError("glGetAttribLocation aTextureCoord");
    }

    void GraphicRender::checkShader(GLuint shader) {
        GLint r = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetShaderInfoLog(shader, sizeof(msg), &length, msg);
            ALOGE("[GraphicRender]Compile shader failed: %s", msg);
        }
    }

    void GraphicRender::checkProgram(GLuint prog) {
        GLint r = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &r);
        if (r == GL_FALSE) {
            GLchar msg[4096] = {};
            GLsizei length;
            glGetProgramInfoLog(prog, sizeof(msg), &length, msg);
            ALOGE("[GraphicRender]Link program failed: %s", msg);
        }
    }

    void GraphicRender::checkGlError(const char *op) {
        int error;
        while ((error = glGetError()) != GL_NO_ERROR) {
            ALOGE("[GraphicRender]%s : glError %d", op, error);
        }
    }
}