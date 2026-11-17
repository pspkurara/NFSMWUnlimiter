#pragma once
#include "CustomizationControlState.h"
#include <algorithm>

namespace CustomizationControls {
struct MouseSample {uint32_t frame=0;bool held=false;int dx=0,dy=0,wheel=0;};
// Pure runtime state: never serialize cursor samples, camera pose or ownership.
struct MouseOrbit {
    bool owns=false,primed=false,wasHeld=false,blocked=false;
    uint32_t frame=0;
    float zoom=1;
    void Reset() {*this=MouseOrbit{};}
    bool Update(const MouseSample& sample,const State& settings,bool takeover,float& yaw,float& pitch) {
        if(!settings.mouseEnabled){Reset();return false;}
        if(takeover) {
            if(owns)yaw=pitch=0;
            owns=false;zoom=1;blocked=sample.held;
            frame=sample.frame;primed=true;wasHeld=sample.held;
            return false;
        }
        if(!primed){primed=true;frame=sample.frame;wasHeld=sample.held;return false;}
        if(frame==sample.frame)return false; // Multiple camera updates must not repeat an input packet.
        frame=sample.frame;
        const bool drag=sample.held&&wasHeld;wasHeld=sample.held;
        if(blocked){if(!sample.held)blocked=false;return false;}
        const bool rotate=drag&&(sample.dx||sample.dy);
        if(rotate||sample.wheel) {
            owns=true;
            if(rotate) {
                yaw=std::remainder(yaw+sample.dx*settings.mouseSensitivity*(settings.mouseInvertX?-1.f:1.f),360.f);
                pitch=std::clamp(pitch-sample.dy*settings.mouseSensitivity*(settings.mouseInvertY?-1.f:1.f),
                    -settings.pitchLimit,settings.pitchLimit);
            }
            const float notches=std::clamp(sample.wheel/120.f,-20.f,20.f);
            zoom=std::clamp(zoom*std::exp(-notches*settings.mouseZoomStep),.25f,4.f);
            return true;
        }
        return false;
    }
};
}
