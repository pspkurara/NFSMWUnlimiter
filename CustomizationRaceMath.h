#pragma once
#include <cmath>
#include <initializer_list>
namespace CustomizationRace {
struct Wheel {float offset,width,radius;};
struct Adjustment {float positionY,radius,skidScale;};
// Render offset has the opposite sign to the simulation/render-connection Y.
// Apply only the change, retaining stock kit offsets and skid calibration.
inline bool Adjust(float positionY,float radius,const Wheel& before,const Wheel& after,Adjustment& result) {
    for(float value:{positionY,radius,before.offset,before.width,before.radius,after.offset,after.width,after.radius})
        if(!std::isfinite(value))return false;
    if(before.width<.005f||before.radius<.005f||after.width<.005f||after.width>5||after.radius<.005f||after.radius>5||
       std::fabs(after.offset)>5||radius<.005f)return false;
    // The connection's calibrated render radius can differ from mesh radius.
    // Preserve that native difference, including continuity around the default.
    const float adjustedRadius=radius+(after.radius-before.radius);
    if(!std::isfinite(adjustedRadius)||adjustedRadius<.005f||adjustedRadius>5)return false;
    result={positionY+before.offset-after.offset,adjustedRadius,after.width/before.width};
    return std::isfinite(result.positionY)&&std::isfinite(result.skidScale);
}
}
