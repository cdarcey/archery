#ifndef AY_MATH_H
#define AY_MATH_H

#include <math.h>

typedef union _ayVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} ayVec2;

typedef union _ayVec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { float u, v, __; };
    struct { ayVec2 xy; float ignore0_; };
    struct { ayVec2 rg; float ignore1_; };
    struct { ayVec2 uv; float ignore2_; };
    struct { float ignore3_; ayVec2 yz; };
    struct { float ignore4_; ayVec2 gb; };
    struct { float ignore5_; ayVec2 v__; };
    float d[3];
} ayVec3;

typedef union _ayVec4
{
    struct
    {
        union
        {
            ayVec3 xyz;
            struct{ float x, y, z;};
        };
        float w;
    };
    struct
    {
        union
        {
            ayVec3 rgb;
            struct{ float r, g, b;};
        };
        float a;
    };
    struct
    {
        ayVec2 xy;
        float ignored0_, ignored1_;
    };
    struct
    {
        float ignored2_;
        ayVec2 yz;
        float ignored3_;
    };
    struct
    {
        float ignored4_, ignored5_;
        ayVec2 zw;
    };
    float d[4];
} ayVec4;


typedef union _ayMat4 
{
    float m[4][4];
    float d[16];
} ayMat4;

static inline ayMat4 
ay_mat4_identity(void)
{
    ayMat4 tResult = {0};
    tResult.m[0][0] = 1.0f;
    tResult.m[1][1] = 1.0f;
    tResult.m[2][2] = 1.0f;
    tResult.m[3][3] = 1.0f;
    return tResult;
}

static inline ayMat4 
ay_mat4_multiply(ayMat4 a, ayMat4 b)
{
    ayMat4 tResult = {0};
    for(int iRow = 0; iRow < 4; iRow++)
    {
        for(int iCol = 0; iCol < 4; iCol++)
        {
            tResult.m[iRow][iCol] = 
                a.m[iRow][0] * b.m[0][iCol] +
                a.m[iRow][1] * b.m[1][iCol] +
                a.m[iRow][2] * b.m[2][iCol] +
                a.m[iRow][3] * b.m[3][iCol];
        }
    }
    return tResult;
}

static inline ayMat4
ay_mat4_rotate_y(float fAngleRadians)
{
    ayMat4 tResult = ay_mat4_identity();
    float fCos = cosf(fAngleRadians);
    float fSin = sinf(fAngleRadians);

    tResult.m[0][0] =  fCos;
    tResult.m[0][2] =  fSin;
    tResult.m[2][0] = -fSin;
    tResult.m[2][2] =  fCos;
    
    return tResult;
}

static inline ayMat4
ay_mat4_perspective(float fFovRadians, float fAspectRatio, float fNearPlane, float fFarPlane)
{
    ayMat4 tResult = {0};
    float fTanHalfFov = tanf(fFovRadians / 2.0f);
    
    tResult.m[0][0] = 1.0f / (fAspectRatio * fTanHalfFov);
    tResult.m[1][1] = 1.0f / fTanHalfFov;
    tResult.m[2][2] = -(fFarPlane + fNearPlane) / (fFarPlane - fNearPlane);
    tResult.m[3][2] = -1.0f; 
    tResult.m[2][3] = -(2.0f * fFarPlane * fNearPlane) / (fFarPlane - fNearPlane);

    return tResult;
}

static inline ayVec4 
ay_mat4_mul_vec4(ayMat4 tMatrix, ayVec4 tVector)
{
    ayVec4 result;
    
    result.x = tMatrix.m[0][0] * tVector.x + tMatrix.m[0][1] * tVector.y + tMatrix.m[0][2] * tVector.z + tMatrix.m[0][3] * tVector.w;
    result.y = tMatrix.m[1][0] * tVector.x + tMatrix.m[1][1] * tVector.y + tMatrix.m[1][2] * tVector.z + tMatrix.m[1][3] * tVector.w;
    result.z = tMatrix.m[2][0] * tVector.x + tMatrix.m[2][1] * tVector.y + tMatrix.m[2][2] * tVector.z + tMatrix.m[2][3] * tVector.w;
    result.w = tMatrix.m[3][0] * tVector.x + tMatrix.m[3][1] * tVector.y + tMatrix.m[3][2] * tVector.z + tMatrix.m[3][3] * tVector.w;
    
    return result;
}

static inline 
ayMat4 ay_mat4_translate(float x, float y, float z)
{
    ayMat4 tResult = ay_mat4_identity();
    tResult.m[0][3] = x;
    tResult.m[1][3] = y;
    tResult.m[2][3] = z;
    return tResult;
}

#endif