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
                    1.0f, 1.0f, 0,
                    -1.0f, -1.0f, 0,
                    1.0f, -1.0f, 0,
            },
            {
                    // X, Y, Z
                    -1.0f,  1.0f, 0,
                    1.0f, 1.0f, 0,
                    -1.0f,  -1.0f, 0,
                    1.0f, -1.0f, 0,
            }
    };
    constexpr float TextureVertex[] = {
            // U, V
            0.f, 1.f,
            1.f, 1.f,
            0.f, 0.f,
            1.f, 0.f,
    };

    class GraphicRender {
    public:
        static void clear();

        void initialize(int32_t width, int32_t height);

        void draw(const uint32_t eye);

        bool setupFrameBuffer(int32_t eye);

        static void bindDefaultFrameBuffer();

        void release();

    private:
        GLuint createTexture() const;

        static GLuint createFrameBuffer(int32_t width, int32_t height);

        void draw(const float position[], const float uv[]);

        void createProgram(const char *vertexSource, const char *fragmentSource);

        static void checkShader(GLuint shader);

        static void checkProgram(GLuint prog);

        static void checkGlError(const char *op);

    private:
        float mMVPMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1,};
        GLuint mProgram;
        GLuint mTextureID[2] = {0, 0};
        GLuint mFrameBuffer[2] = {0, 0};
        GLint muMVPMatrixHandle;
        GLint maPositionHandle;
        GLint maTextureHandle;
        GLint mWidth, mHeight;
    };
}

#endif //CLOUDXR_GRAPHICRENDER_H
