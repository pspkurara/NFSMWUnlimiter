#pragma once
#include <cmath>
#include <cstring>

// Row-vector matrices, matching bMatrix4 on the PC executable.
namespace CustomizationMath {
struct Matrix { float m[4][4]; };
inline Matrix Identity() {
    Matrix r{};
    for (int i=0;i<4;++i) r.m[i][i]=1;
    return r;
}
inline Matrix Multiply(const Matrix& a,const Matrix& b) {
    Matrix r{};
    for(int i=0;i<4;++i) for(int j=0;j<4;++j)
        for(int k=0;k<4;++k) r.m[i][j]+=a.m[i][k]*b.m[k][j];
    return r;
}
inline bool RigidInverse(const Matrix& a,Matrix& out) {
    // Reject scaled or degenerate transforms rather than corrupting animated parts.
    for(int i=0;i<4;++i) for(int j=0;j<4;++j)
        if(!std::isfinite(a.m[i][j])) return false;
    for(int i=0;i<3;++i) if(std::fabs(a.m[i][3])>.0001f) return false;
    if(std::fabs(a.m[3][3]-1.f)>.0001f) return false;
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) {
        float dot=0;
        for(int k=0;k<3;++k) dot+=a.m[i][k]*a.m[j][k];
        if(!std::isfinite(dot)||std::fabs(dot-(i==j?1.f:0.f))>.01f) return false;
    }
    out=Identity();
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) out.m[i][j]=a.m[j][i];
    for(int j=0;j<3;++j) for(int k=0;k<3;++k) out.m[3][j]-=a.m[3][k]*out.m[k][j];
    return true;
}
inline Matrix HeightTransform(float frontX,float rearX,float frontHeight,float rearHeight) {
    Matrix r=Identity();
    const float length=frontX-rearX;
    if(!std::isfinite(length)||std::fabs(length)<.1f) return r;
    const float slope=(frontHeight-rearHeight)/length;
    if(!std::isfinite(slope)||std::fabs(slope)>=.95f) return r;
    const float angle=std::asin(slope);
    const float c=std::cos(angle),s=std::sin(angle);
    r.m[0][0]=c;r.m[0][2]=s;r.m[2][0]=-s;r.m[2][2]=c;
    // The two axle references define the rigid body's rake and centre height.
    r.m[3][2]=frontHeight-frontX*s;
    return r;
}
inline Matrix MoveBodyPart(const Matrix& part,const Matrix& body,const Matrix& delta) {
    Matrix inverse;
    if(!RigidInverse(body,inverse)) return part;
    return Multiply(Multiply(Multiply(part,inverse),delta),body);
}
} // namespace CustomizationMath
