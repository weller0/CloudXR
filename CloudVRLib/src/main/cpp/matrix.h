//
// Created by Weller on 2022/4/28.
//

#ifndef CLOUDXR_MATRIX_H
#define CLOUDXR_MATRIX_H
typedef struct matrix4f_ {
    float M[4][4];
} matrix4f;

matrix4f multiply(const matrix4f *a, const matrix4f *b) {
    matrix4f out;
    out.M[0][0] = a->M[0][0] * b->M[0][0] + a->M[0][1] * b->M[1][0] + a->M[0][2] * b->M[2][0] +
                  a->M[0][3] * b->M[3][0];
    out.M[1][0] = a->M[1][0] * b->M[0][0] + a->M[1][1] * b->M[1][0] + a->M[1][2] * b->M[2][0] +
                  a->M[1][3] * b->M[3][0];
    out.M[2][0] = a->M[2][0] * b->M[0][0] + a->M[2][1] * b->M[1][0] + a->M[2][2] * b->M[2][0] +
                  a->M[2][3] * b->M[3][0];
    out.M[3][0] = a->M[3][0] * b->M[0][0] + a->M[3][1] * b->M[1][0] + a->M[3][2] * b->M[2][0] +
                  a->M[3][3] * b->M[3][0];

    out.M[0][1] = a->M[0][0] * b->M[0][1] + a->M[0][1] * b->M[1][1] + a->M[0][2] * b->M[2][1] +
                  a->M[0][3] * b->M[3][1];
    out.M[1][1] = a->M[1][0] * b->M[0][1] + a->M[1][1] * b->M[1][1] + a->M[1][2] * b->M[2][1] +
                  a->M[1][3] * b->M[3][1];
    out.M[2][1] = a->M[2][0] * b->M[0][1] + a->M[2][1] * b->M[1][1] + a->M[2][2] * b->M[2][1] +
                  a->M[2][3] * b->M[3][1];
    out.M[3][1] = a->M[3][0] * b->M[0][1] + a->M[3][1] * b->M[1][1] + a->M[3][2] * b->M[2][1] +
                  a->M[3][3] * b->M[3][1];

    out.M[0][2] = a->M[0][0] * b->M[0][2] + a->M[0][1] * b->M[1][2] + a->M[0][2] * b->M[2][2] +
                  a->M[0][3] * b->M[3][2];
    out.M[1][2] = a->M[1][0] * b->M[0][2] + a->M[1][1] * b->M[1][2] + a->M[1][2] * b->M[2][2] +
                  a->M[1][3] * b->M[3][2];
    out.M[2][2] = a->M[2][0] * b->M[0][2] + a->M[2][1] * b->M[1][2] + a->M[2][2] * b->M[2][2] +
                  a->M[2][3] * b->M[3][2];
    out.M[3][2] = a->M[3][0] * b->M[0][2] + a->M[3][1] * b->M[1][2] + a->M[3][2] * b->M[2][2] +
                  a->M[3][3] * b->M[3][2];

    out.M[0][3] = a->M[0][0] * b->M[0][3] + a->M[0][1] * b->M[1][3] + a->M[0][2] * b->M[2][3] +
                  a->M[0][3] * b->M[3][3];
    out.M[1][3] = a->M[1][0] * b->M[0][3] + a->M[1][1] * b->M[1][3] + a->M[1][2] * b->M[2][3] +
                  a->M[1][3] * b->M[3][3];
    out.M[2][3] = a->M[2][0] * b->M[0][3] + a->M[2][1] * b->M[1][3] + a->M[2][2] * b->M[2][3] +
                  a->M[2][3] * b->M[3][3];
    out.M[3][3] = a->M[3][0] * b->M[0][3] + a->M[3][1] * b->M[1][3] + a->M[3][2] * b->M[2][3] +
                  a->M[3][3] * b->M[3][3];
    return out;
}

matrix4f createFromQuaternion(float x, float y, float z, float w) {
    const float ww = w * w;
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;

    matrix4f out;
    out.M[0][0] = ww + xx - yy - zz;
    out.M[0][1] = 2 * (x * y - w * z);
    out.M[0][2] = 2 * (x * z + w * y);
    out.M[0][3] = 0;

    out.M[1][0] = 2 * (x * y + w * z);
    out.M[1][1] = ww - xx + yy - zz;
    out.M[1][2] = 2 * (y * z - w * x);
    out.M[1][3] = 0;

    out.M[2][0] = 2 * (x * z - w * y);
    out.M[2][1] = 2 * (y * z + w * x);
    out.M[2][2] = ww - xx - yy + zz;
    out.M[2][3] = 0;

    out.M[3][0] = 0;
    out.M[3][1] = 0;
    out.M[3][2] = 0;
    out.M[3][3] = 1;
    return out;
}

matrix4f createTranslation(const float x, const float y, const float z) {
    matrix4f out;
    out.M[0][0] = 1.0f;
    out.M[0][1] = 0.0f;
    out.M[0][2] = 0.0f;
    out.M[0][3] = x;
    out.M[1][0] = 0.0f;
    out.M[1][1] = 1.0f;
    out.M[1][2] = 0.0f;
    out.M[1][3] = y;
    out.M[2][0] = 0.0f;
    out.M[2][1] = 0.0f;
    out.M[2][2] = 1.0f;
    out.M[2][3] = z;
    out.M[3][0] = 0.0f;
    out.M[3][1] = 0.0f;
    out.M[3][2] = 0.0f;
    out.M[3][3] = 1.0f;
    return out;
}

#endif //CLOUDXR_MATRIX_H
