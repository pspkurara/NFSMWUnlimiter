#pragma once
#include <cmath>

namespace CustomizationOrbit {
struct Vec {float x,y,z;};
struct Pose {Vec eye,look;};
// Outside views orbit the native target. First-person views rotate about the
// native eye so a long look-ahead distance cannot pull the camera out of the car.
inline bool Rotate(const Pose& native,float yaw,float pitch,bool fixedEye,Pose& result) {
    const Vec center=fixedEye?native.eye:native.look;
    const Vec endpoint=fixedEye?native.look:native.eye;
    const float dx=endpoint.x-center.x,dy=endpoint.y-center.y,dz=endpoint.z-center.z;
    const float distance=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(distance)||distance<.05f||!std::isfinite(yaw)||!std::isfinite(pitch))return false;
    constexpr float radians=.01745329251994329577f;
    const float azimuth=std::atan2(dy,dx)+yaw*radians;
    const float rawElevation=std::atan2(dz,std::sqrt(dx*dx+dy*dy))+pitch*radians;
    const float elevation=std::fmax(-1.4f,std::fmin(1.4f,rawElevation));
    const float horizontal=distance*std::cos(elevation);
    const Vec rotated={center.x+horizontal*std::cos(azimuth),center.y+horizontal*std::sin(azimuth),
        center.z+distance*std::sin(elevation)};
    if(!std::isfinite(rotated.x)||!std::isfinite(rotated.y)||!std::isfinite(rotated.z))return false;
    result=fixedEye?Pose{center,rotated}:Pose{rotated,center};
    return true;
}
}
