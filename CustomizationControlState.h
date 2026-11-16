#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>

namespace CustomizationControls {
// POD file schema: no pointers, device GUIDs, profile pointers, or input samples.
struct State {
    uint32_t version=1, enabled=0, controller=0, yawAxis=3, pitchAxis=4;
    uint32_t resetButtons=0x80, invertX=0, invertY=0;
    uint32_t keys[5]{}; // left, right, up, down, reset; Windows VK, 0=unbound
    float deadzone=.22f, speed=100.f, pitchLimit=60.f, returnDelay=-1.f, returnSpeed=120.f;
};
static_assert(sizeof(State)==72,"Controls sidecar state version 1");
inline bool Equal(const State& a,const State& b) { return std::memcmp(&a,&b,sizeof(State))==0; }
inline bool Valid(const State& s) {
    if(s.version!=1||s.enabled>1||s.controller>3||s.yawAxis>4||s.pitchAxis>4||
       s.invertX>1||s.invertY>1||(s.resetButtons&~0xF3FFu)) return false;
    for(auto key:s.keys) if(key>255) return false;
    return std::isfinite(s.deadzone)&&s.deadzone>=0&&s.deadzone<=.95f&&
        std::isfinite(s.speed)&&s.speed>=1&&s.speed<=720&&
        std::isfinite(s.pitchLimit)&&s.pitchLimit>=0&&s.pitchLimit<=80&&
        std::isfinite(s.returnDelay)&&s.returnDelay>=-1&&s.returnDelay<=60&&
        std::isfinite(s.returnSpeed)&&s.returnSpeed>=1&&s.returnSpeed<=720;
}
inline void ResetDevice(State& s,bool controller) {
    State defaults;
    if(controller) {
        s.controller=defaults.controller;s.yawAxis=defaults.yawAxis;s.pitchAxis=defaults.pitchAxis;
        s.resetButtons=defaults.resetButtons;s.invertX=s.invertY=0;s.deadzone=defaults.deadzone;
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
