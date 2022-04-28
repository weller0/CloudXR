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
        mTextureID[0] = createTexture();
        mTextureID[1] = createTexture();
        mFrameBuffer[0] = createFrameBuffer(width / 2, height);
        mFrameBuffer[1] = createFrameBuffer(width / 2, height);
        checkGlError("initialize");
    }

    void GraphicRender::release() {
        if (mTextureID[0] > 0 && mTextureID[1] > 0) {
            glDeleteTextures(2, mTextureID);
        }
        if (mFrameBuffer[0] > 0 && mFrameBuffer[1] > 0) {
            glDeleteFramebuffers(2, mFrameBuffer);
        }
        if (mProgram > 0) {
            glDeleteProgram(mProgram);
        }
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

    bool GraphicRender::setupFrameBuffer(int32_t eye) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFrameBuffer[eye]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextureID[eye], 0);
        return true;
    }

    void GraphicRender::bindDefaultFrameBuffer() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    GLuint GraphicRender::createFrameBuffer(int32_t width, int32_t height) {
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
            return 0;
        }
        return framebuffer;
    }

    GLuint GraphicRender::createTexture() const {
        GLuint texture = 0;
        glGenTextures(1, &texture);

        // tex L
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mWidth / 2, mHeight,
                     0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        checkGlError("createTexture");
        return texture;
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
        uint32_t error;
        while ((error = glGetError()) != GL_NO_ERROR) {
            ALOGE("[GraphicRender]%s : glError %d", op, error);
        }
    }
}