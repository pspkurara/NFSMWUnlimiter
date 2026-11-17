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
inline bool AffineInverse(const Matrix& a,Matrix& out) {
    // Garage Z-sort bias and model scale produce valid non-rigid roots. Invert
    // their full linear basis instead of treating transpose as an inverse.
    double scale=0;
    for(int i=0;i<4;++i)for(int j=0;j<4;++j) {
        if(!std::isfinite(a.m[i][j]))return false;
        if(i<3&&j<3)scale=std::fmax(scale,std::fabs(double(a.m[i][j])));
    }
    for(int i=0;i<3;++i)if(std::fabs(a.m[i][3])>.0001f)return false;
    if(std::fabs(a.m[3][3]-1.f)>.0001f||scale==0)return false;
    const double x=a.m[0][0],y=a.m[0][1],z=a.m[0][2];
    const double d=a.m[1][0],e=a.m[1][1],f=a.m[1][2];
    const double g=a.m[2][0],h=a.m[2][1],k=a.m[2][2];
    const double det=x*(e*k-f*h)-y*(d*k-f*g)+z*(d*h-e*g);
    if(std::fabs(det)<=1e-6*scale*scale*scale)return false;
    const double inverse[3][3]={{e*k-f*h,z*h-y*k,y*f-z*e},
        {f*g-d*k,x*k-z*g,z*d-x*f},{d*h-e*g,y*g-x*h,x*e-y*d}};
    Matrix result=Identity();
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)result.m[i][j]=float(inverse[i][j]/det);
    for(int j=0;j<3;++j) {
        double translation=0;
        for(int i=0;i<3;++i)translation-=double(a.m[3][i])*result.m[i][j];
        result.m[3][j]=float(translation);
    }
    for(const auto& row:result.m)for(float value:row)if(!std::isfinite(value))return false;
    out=result;return true;
}
inline Matrix HeightTransform(float frontX,float rearX,float frontHeight,float rearHeight) {
    Matrix r=Identity();
    if(!std::isfinite(frontHeight)||!std::isfinite(rearHeight))return r;
    // Equal axle heights are a translation; no wheelbase or axle geometry is
    // needed. Incomplete MOD attributes must not disable ordinary lowering.
    if(frontHeight==rearHeight) {r.m[3][2]=frontHeight;return r;}
    if(!std::isfinite(frontX)||!std::isfinite(rearX))return r;
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
    if(!AffineInverse(body,inverse)) return part;
    return Multiply(Multiply(Multiply(part,inverse),delta),body);
}
} // namespace CustomizationMath
