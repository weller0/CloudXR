#ifndef CLOUDXR_GRAPHICRENDER_H
#define CLOUDXR_GRAPHICRENDER_H

#include <GLES3/gl32.h>
#include <unordered_map>
#include <openxr/openxr_platform.h>

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
    constexpr float PositionVertexL[] = {
            // X, Y, Z
            -1.0f, 1.0f, 0,
            0.0f, 1.0f, 0,
            -1.0f, -1.0f, 0,
            0.0f, -1.0f, 0,
    };
    constexpr float PositionVertexR[] = {
            // X, Y, Z
            0.0f, 1.0f, 0,
            1.0f, 1.0f, 0,
            0.0f, -1.0f, 0,
            1.0f, -1.0f, 0,
    };
    constexpr float TextureVertex[] = {
            // U, V
            0.f, 0.f,
            1.f, 0.f,
            0.f, 1.f,
            1.f, 1.f,
    };

    class GraphicRender {
        typedef std::unordered_map<GLuint, GLuint> IDMap;
    public:
        void clear();

        void initialize();

        void draw(const XrPosef pose);

    private:
        void createTexture();

        void draw(const float position[], const float uv[]);

        bool setupFrameBuffer(int texture, int width, int height);

        void bindDefaultFrameBuffer();

        void drawFbo();

        void createProgram(const char *vertexSource, const char *fragmentSource);

        void checkShader(GLuint shader);

        void checkProgram(GLuint prog);

        void checkGlError(const char *op);

    private:
        float mMVPMatrix[16] = {1, 0, 0, 0,
                                0, 1, 0, 0,
                                0, 0, 1, 0,
                                0, 0, 0, 1,};
        int mProgram;
        int mTextureIDL = -12345;
        int mTextureIDR = -12345;
        int muMVPMatrixHandle;
        int maPositionHandle;
        int maTextureHandle;
        IDMap frameBuffers;
    };
}

#endif //CLOUDXR_GRAPHICRENDER_H
