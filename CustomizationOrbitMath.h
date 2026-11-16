#pragma once
#include <cmath>

namespace CustomizationOrbit {
struct Vec {float x,y,z;};
struct Pose {Vec eye,look;};
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
}
