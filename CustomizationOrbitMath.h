#pragma once
#include <cmath>

namespace CustomizationOrbit {
struct Vec {float x,y,z;};
struct Pose {Vec eye,look;};
inline Vec Add(Vec a,Vec b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec Sub(Vec a,Vec b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec Scale(Vec a,float n) {return {a.x*n,a.y*n,a.z*n};}
inline float Dot(Vec a,Vec b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Normalize(Vec& vector) {
    const float length=std::sqrt(Dot(vector,vector));
    if(!std::isfinite(length)||length<.001f)return false;
    vector=Scale(vector,1/length);return true;
}
struct Frame {
    Vec origin,forward,right,up;
    Vec Local(Vec world) const {const auto delta=Sub(world,origin);return {Dot(delta,forward),Dot(delta,right),Dot(delta,up)};}
    Vec World(Vec local) const {return Add(origin,Add(Scale(forward,local.x),Add(Scale(right,local.y),Scale(up,local.z))));}
};
inline bool VehicleFrame(Vec origin,Vec forward,Vec up,Frame& result) {
    if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||!Normalize(forward))return false;
    auto right=Cross(up,forward);if(!Normalize(right))return false;
    up=Cross(forward,right);if(!Normalize(up))return false;
    result={origin,forward,right,up};return true;
}
// Keep the native target, elevation and distance, replacing only horizontal
// bearing with the vehicle's raw geometry heading (not its delayed camera yaw).
inline bool VehicleHeading(const Pose& native,const Vec& forward,bool fixedEye,bool lookBack,Pose& result) {
    const float length=std::hypot(forward.x,forward.y);
    if(!std::isfinite(length)||length<.001f)return false;
    const Vec center=fixedEye?native.eye:native.look;
    const Vec endpoint=fixedEye?native.look:native.eye;
    const float horizontal=std::hypot(endpoint.x-center.x,endpoint.y-center.y);
    const float sign=(fixedEye?1.f:-1.f)*(lookBack?-1.f:1.f);
    const Vec aligned={center.x+sign*horizontal*forward.x/length,
        center.y+sign*horizontal*forward.y/length,endpoint.z};
    if(!std::isfinite(aligned.x)||!std::isfinite(aligned.y)||!std::isfinite(aligned.z))return false;
    result=fixedEye?Pose{center,aligned}:Pose{aligned,center};return true;
}
// Outside views orbit the native target. First-person views rotate about the
// native eye so a long look-ahead distance cannot pull the camera out of the car.
inline bool Rotate(const Pose& native,float yaw,float pitch,bool fixedEye,Pose& result,float zoom=1) {
    const Vec center=fixedEye?native.eye:native.look;
    const Vec endpoint=fixedEye?native.look:native.eye;
    const float dx=endpoint.x-center.x,dy=endpoint.y-center.y,dz=endpoint.z-center.z;
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(zoom)||zoom<.25f||zoom>4||!std::isfinite(distance)||distance<.05f||!std::isfinite(yaw)||!std::isfinite(pitch))return false;
    constexpr float radians=.01745329251994329577f;
    const float azimuth=std::atan2(dy,dx)+yaw*radians;
    const float rawElevation=std::atan2(dz,std::sqrt(dx*dx+dy*dy))+pitch*radians;
    const float elevation=pitch==0?rawElevation:std::fmax(-1.4f,std::fmin(1.4f,rawElevation));
    const float radius=distance*(fixedEye?1.f:zoom);
    const float horizontal=radius*std::cos(elevation);
    const Vec rotated={center.x+horizontal*std::cos(azimuth),center.y+horizontal*std::sin(azimuth),
        center.z+radius*std::sin(elevation)};
    if(!std::isfinite(rotated.x)||!std::isfinite(rotated.y)||!std::isfinite(rotated.z))return false;
    result=fixedEye?Pose{center,rotated}:Pose{rotated,center};
    return true;
}
// One reference per mover/POV generation, expressed in the car's complete frame.
// Native camera lag/elevation changes cannot rewrite that reference each frame.
struct VehicleOrbit {
    bool valid=false;
    Pose local{};
    float distance=0;
    void Clear() {*this=VehicleOrbit{};}
    bool Compose(const Pose& native,const Frame& frame,bool fixedEye,bool lookBack,
                 float yaw,float pitch,float zoom,Pose& result) {
        const auto center=frame.Local(fixedEye?native.eye:native.look);
        const auto endpoint=frame.Local(fixedEye?native.look:native.eye);
        const auto delta=Sub(endpoint,center);
        const float nativeDistance=std::sqrt(Dot(delta,delta));
        if(!std::isfinite(nativeDistance)||nativeDistance<.05f)return false;
        if(!valid) {
            const float elevation=std::atan2(delta.z,std::hypot(delta.x,delta.y));
            const float sign=fixedEye?1.f:-1.f;
            const Vec aligned=Add(center,{sign*nativeDistance*std::cos(elevation),0,nativeDistance*std::sin(elevation)});
            local=fixedEye?Pose{center,aligned}:Pose{aligned,center};
            distance=nativeDistance;valid=true;
        }
        auto baseline=local;
        // Keep native pull-in from shortening the camera through a wall. Ordinary
        // native zoom-out and camera animation do not change the fixed reference.
        if(!fixedEye&&nativeDistance<distance)
            baseline.eye=Add(local.look,Scale(Sub(local.eye,local.look),nativeDistance/distance));
        Pose rotated;
        if(!Rotate(baseline,yaw+(lookBack?180.f:0.f),pitch,fixedEye,rotated,zoom))return false;
        result={frame.World(rotated.eye),frame.World(rotated.look)};return true;
    }
};
}
