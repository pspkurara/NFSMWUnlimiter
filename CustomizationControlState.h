#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

namespace CustomizationControls {
// POD file schema: no pointers, device GUIDs, profile pointers, or input samples.
struct State {
    uint32_t version=5, enabled=0, controller=0, yawAxis=3, pitchAxis=4;
    // R3 already changes the native camera view on common Xbox mappings.
    uint32_t resetButtons=0, invertX=0, invertY=0;
    uint32_t keys[5]{}; // left, right, up, down, reset; Windows VK, 0=unbound
    float deadzone=.22f, speed=100.f, pitchLimit=60.f, returnDelay=-1.f, returnSpeed=120.f;
    uint32_t orbitMode=0; // 0 = rate orbit, 1 = stick direction (horizontal 360 degrees)
    uint32_t mouseEnabled=1;
    float mouseSensitivity=.3f,mouseZoomStep=.1f;
    uint32_t vehicleHeading=0,hideMirror=0; // Opt-in; preserve existing profile behavior.
    uint32_t directionInvertX=0,directionInvertY=0,mouseInvertX=0,mouseInvertY=0;
    uint32_t showRadarOnMirrorHide=0;
};
static_assert(sizeof(State)==116,"Controls sidecar state version 5");
constexpr uint32_t LegacyStateBytes=72,Version2StateBytes=76,Version3StateBytes=88,Version4StateBytes=96;
// Runtime-only evidence from a successfully composed main-view camera pose.
// Expiration releases the mirror if the mover disappears or stops updating.
struct MirrorPose {
    uint64_t sampledAt=0;bool rotated=false;
    void Clear() {sampledAt=0;rotated=false;}
    void Observe(uint64_t now,float yaw,float pitch) {
        sampledAt=now;
        rotated=std::isfinite(yaw)&&std::isfinite(pitch)&&
            (std::fabs(std::remainder(yaw,360.f))>.001f||std::fabs(pitch)>.001f);
    }
    bool Hide(const State& state,int viewId,bool racing,bool foreground,uint64_t now) const {
        return state.enabled&&state.hideMirror&&viewId==1&&racing&&foreground&&rotated&&
            sampledAt&&now>=sampledAt&&now-sampledAt<=250;
    }
    bool HideRadar(const State& state,bool nativeMirror,bool racing,bool foreground,uint64_t now) const {
        // A user's saved mirror-OFF setting must keep the native no-mirror HUD.
        return nativeMirror&&!state.showRadarOnMirrorHide&&Hide(state,1,racing,foreground,now);
    }
};
// Garage axes have their own stick choice, but can share the active profile's
// orientation/threshold without depending on the driving feature being enabled.
struct AxisTuning {float deadzone;bool invertX,invertY;};
inline AxisTuning GarageTuning(const State& driving,bool inheritInversion,bool inheritDeadzone,AxisTuning local) {
    if(inheritInversion){local.invertX=driving.invertX!=0;local.invertY=driving.invertY!=0;}
    if(inheritDeadzone)local.deadzone=driving.deadzone;
    return local;
}
inline bool Equal(const State& a,const State& b) { return std::memcmp(&a,&b,sizeof(State))==0; }
inline bool Valid(const State& s) {
    if(s.version!=5||s.showRadarOnMirrorHide>1||s.directionInvertX>1||s.directionInvertY>1||s.mouseInvertX>1||s.mouseInvertY>1||
       s.vehicleHeading>1||s.hideMirror>1||s.mouseEnabled>1||s.orbitMode>1||s.enabled>1||s.controller>3||s.yawAxis>4||s.pitchAxis>4||
       s.invertX>1||s.invertY>1||(s.resetButtons&~0xF3FFu)) return false;
    for(auto key:s.keys) if(key>255) return false;
    return std::isfinite(s.mouseSensitivity)&&s.mouseSensitivity>=.01f&&s.mouseSensitivity<=10&&
        std::isfinite(s.mouseZoomStep)&&s.mouseZoomStep>=.01f&&s.mouseZoomStep<=1&&
        std::isfinite(s.deadzone)&&s.deadzone>=0&&s.deadzone<=.95f&&
        std::isfinite(s.speed)&&s.speed>=1&&s.speed<=720&&
        std::isfinite(s.pitchLimit)&&s.pitchLimit>=0&&s.pitchLimit<=80&&
        std::isfinite(s.returnDelay)&&s.returnDelay>=-1&&s.returnDelay<=60&&
        std::isfinite(s.returnSpeed)&&s.returnSpeed>=1&&s.returnSpeed<=720;
}
// Validate the old payload before adding defaults; file CRC covers its original bytes.
inline bool DecodeState(const void* bytes,uint32_t size,State& output) {
    if(size!=sizeof(State)&&size!=Version4StateBytes&&size!=Version3StateBytes&&size!=Version2StateBytes&&size!=LegacyStateBytes)return false;
    State candidate;
    std::memcpy(&candidate,bytes,size);
    if(size==LegacyStateBytes) {
        if(candidate.version!=1)return false;
        candidate.version=5;candidate.orbitMode=0;
    }
    if(size==Version2StateBytes) {
        if(candidate.version!=2)return false;
        candidate.version=5;
    }
    if(size==Version3StateBytes) {
        if(candidate.version!=3)return false;
        candidate.version=5;
    }
    if(size==Version4StateBytes) {
        if(candidate.version!=4)return false;
        candidate.version=5;
    }
    if(size!=sizeof(State)) {
        // Preserve the legacy shared inversion until the user edits each source.
        candidate.directionInvertX=candidate.mouseInvertX=candidate.invertX;
        candidate.directionInvertY=candidate.mouseInvertY=candidate.invertY;
    }
    if(!Valid(candidate))return false;
    output=candidate;return true;
}
inline void InvertStick(const State& state,float& x,float& y) {
    if(state.orbitMode==1) {if(state.directionInvertX)x=-x;if(state.directionInvertY)y=-y;}
    else {if(state.invertX)x=-x;if(state.invertY)y=-y;}
}
inline void ApplyAxes(const State& s,float x,float y,float dt,float& yaw,float& pitch) {
    if(s.orbitMode==1) {
        // The chosen X/Y pair describes an azimuth: forward=0, right=+90, back=180.
        yaw=(x!=0||y!=0)?std::atan2(x,y)*57.29577951308232f:0;
        pitch=0;
    } else {
        yaw=std::remainder(yaw+x*s.speed*dt,360.f);
        pitch=std::fmax(-s.pitchLimit,std::fmin(s.pitchLimit,pitch+y*s.speed*dt));
    }
}
inline void ResetDevice(State& s,bool controller) {
    State defaults;
    if(controller) {
        s.controller=defaults.controller;s.yawAxis=defaults.yawAxis;s.pitchAxis=defaults.pitchAxis;
        s.orbitMode=defaults.orbitMode;s.resetButtons=defaults.resetButtons;s.invertX=s.invertY=0;s.deadzone=defaults.deadzone;
        s.directionInvertX=s.directionInvertY=0;
    } else {for(auto& key:s.keys) key=0;s.mouseInvertX=s.mouseInvertY=0;}
}
// Native comparison/restore and the additional dialog share the same transaction.
struct Session {
    State active{},before{};
    bool editing=false;
    void Begin() {before=active;editing=true;}
    bool Dirty() const {return editing&&!Equal(active,before);}
    bool Apply(const State& next) {if(!Valid(next)) return false;active=next;return true;}
    void Restore() {if(editing)active=before;}
    void End() {editing=false;}
};
}
