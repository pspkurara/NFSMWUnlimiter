#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

namespace CustomizationControls {
// POD file schema: no pointers, device GUIDs, profile pointers, or input samples.
struct State {
    uint32_t version=4, enabled=0, controller=0, yawAxis=3, pitchAxis=4;
    // R3 already changes the native camera view on common Xbox mappings.
    uint32_t resetButtons=0, invertX=0, invertY=0;
    uint32_t keys[5]{}; // left, right, up, down, reset; Windows VK, 0=unbound
    float deadzone=.22f, speed=100.f, pitchLimit=60.f, returnDelay=-1.f, returnSpeed=120.f;
    uint32_t orbitMode=0; // 0 = rate orbit, 1 = stick direction (horizontal 360 degrees)
    uint32_t mouseEnabled=1;
    float mouseSensitivity=.3f,mouseZoomStep=.1f;
    uint32_t vehicleHeading=0,hideMirror=0; // Opt-in; preserve existing profile behavior.
};
static_assert(sizeof(State)==96,"Controls sidecar state version 4");
constexpr uint32_t LegacyStateBytes=72,Version2StateBytes=76,Version3StateBytes=88;
inline bool Equal(const State& a,const State& b) { return std::memcmp(&a,&b,sizeof(State))==0; }
inline bool Valid(const State& s) {
    if(s.version!=4||s.vehicleHeading>1||s.hideMirror>1||s.mouseEnabled>1||s.orbitMode>1||s.enabled>1||s.controller>3||s.yawAxis>4||s.pitchAxis>4||
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
    if(size!=sizeof(State)&&size!=Version3StateBytes&&size!=Version2StateBytes&&size!=LegacyStateBytes)return false;
    State candidate;
    std::memcpy(&candidate,bytes,size);
    if(size==LegacyStateBytes) {
        if(candidate.version!=1)return false;
        candidate.version=4;candidate.orbitMode=0;
    }
    if(size==Version2StateBytes) {
        if(candidate.version!=2)return false;
        candidate.version=4;
    }
    if(size==Version3StateBytes) {
        if(candidate.version!=3)return false;
        candidate.version=4;
    }
    if(!Valid(candidate))return false;
    output=candidate;return true;
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
    } else for(auto& key:s.keys) key=0;
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
