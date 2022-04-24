#ifndef CLOUDXR_GRAPHICRENDER_H
#define CLOUDXR_GRAPHICRENDER_H

#include <GLES3/gl32.h>
#include <unordered_map>

namespace ssnwt {
    static const char *VERTEX_SHADER = R"_(
        uniform mat4 uMVPMatrix;
        attribute vec4 aPosition;
        attribute vec4 aTextureCoord;
        varying vec2 vTextureCoord;
        void main() {
            gl_Position = uMVPMatrix * aPosition;
            vTextureCoord = aTextureCoord.xy;
        }
    )_";

    static const char *FRAGMENT_SHADER = R"_(
        precision mediump float;
        varying vec2 vTextureCoord;
        uniform sampler2D sTexture;
        void main() {
            gl_FragColor = texture2D(sTexture, vTextureCoord);
        }
    )_";
    constexpr float PositionVertex[2][12] = {
            {
                    // X, Y, Z
                    -1.0f, 1.0f, 0,
                    0.0f, 1.0f, 0,
                    -1.0f, -1.0f, 0,
                    0.0f, -1.0f, 0,
            },
            {
                    // X, Y, Z
                    0.0f,  1.0f, 0,
                    1.0f, 1.0f, 0,
                    0.0f,  -1.0f, 0,
                    1.0f, -1.0f, 0,
            }
    };
    constexpr float TextureVertex[] = {
            // U, V
            0.f, 0.f,
            1.f, 0.f,
            0.f, 1.f,
            1.f, 1.f,
    };

    class GraphicRender {
    public:
        void clear();

        void initialize(int32_t width, int32_t height);

        void draw(const uint32_t eye);

        void drawFbo(const uint32_t eye);

        void drawFbo();

        bool setupFrameBuffer(int32_t eye, int32_t width, int32_t height);

        void bindDefaultFrameBuffer();

        void updateTexture(GLuint texL, GLuint texR) {
            mTextureID[0] = texL;
            mTextureID[1] = texR;
        }

        GLuint getTextureIdL() { return mTextureID[0]; }

        GLuint getTextureIdR() { return mTextureID[1]; }

    private:
        GLuint createTexture();

        GLuint createFrameBuffer(int32_t width, int32_t height);

        void draw(const float position[], const float uv[]);

        void createProgram(const char *vertexSource, const char *fragmentSource);

        void checkShader(GLuint shader);

        void checkProgram(GLuint prog);

        void checkGlError(const char *op);

    private:
        float mMVPMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1,};
        int32_t mProgram;
        GLuint mTextureID[2] = {0, 0};
        GLuint mFrameBuffer[2] = {0, 0};
        int32_t muMVPMatrixHandle;
        int32_t maPositionHandle;
        int32_t maTextureHandle;
        int32_t mWidth, mHeight;
    };
}

#endif //CLOUDXR_GRAPHICRENDER_H
