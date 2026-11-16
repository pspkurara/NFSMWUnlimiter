#include "../CustomizationControlState.h"
#include "../CustomizationOrbitMath.h"
#include "../CustomizationMouseOrbit.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace CustomizationControls;
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(){try{
    Session session;
    require(Valid(session.active),"valid defaults");
    require(session.active.resetButtons==0,"defaults must not shadow native R3 camera-view action");
    State shared;shared.enabled=0;shared.invertX=1;shared.deadzone=.4f;
    const AxisTuning local{.1f,false,true};
    auto garage=GarageTuning(shared,true,true,local);
    require(garage.invertX&&!garage.invertY&&garage.deadzone==.4f,"garage inherits active tuning even with race orbit disabled");
    garage=GarageTuning(shared,false,false,local);
    require(!garage.invertX&&garage.invertY&&garage.deadzone==.1f,"garage explicit policy preserves its independent settings");
    garage=GarageTuning(shared,true,false,local);
    require(garage.invertX&&!garage.invertY&&garage.deadzone==.1f,"garage orientation and threshold inheritance are independent");
    session.Begin();auto next=session.active;next.enabled=1;next.keys[0]='J';
    require(session.Apply(next)&&session.Dirty(),"camera-only edit makes native controls dirty");
    session.Restore();require(!session.Dirty()&&!session.active.enabled,"native discard restores extension");
    next.yawAxis=99;require(!session.Apply(next)&&!session.Dirty(),"invalid axes do not mutate active state");
    next=State{};next.speed=std::numeric_limits<float>::quiet_NaN();require(!Valid(next),"NaN rejected");
    next=State{};next.keys[4]=256;require(!Valid(next),"VK upper bound");
    next=State{};next.keys[0]='A';next.yawAxis=1;ResetDevice(next,false);
    require(next.keys[0]==0&&next.yawAxis==1,"keyboard reset preserves controller mapping");
    next.keys[0]='A';ResetDevice(next,true);require(next.keys[0]=='A'&&next.yawAxis==3,"controller reset preserves keyboard mapping");
    session.active=next;session.End();require(!session.Dirty(),"new profile without native input reader preserves controls");
    next=State{};next.orbitMode=1;float yaw=17,pitch=12;
    ApplyAxes(next,1,0,.001f,yaw,pitch);require(std::fabs(yaw-90)<.001f&&pitch==0,"direction right is independent of frame duration");
    ApplyAxes(next,1,0,1.f,yaw,pitch);require(std::fabs(yaw-90)<.001f,"holding direction cannot accumulate spin");
    ApplyAxes(next,0,-1,.01f,yaw,pitch);require(std::fabs(std::fabs(yaw)-180)<.001f,"back direction spans full circle");
    ApplyAxes(next,0,0,.01f,yaw,pitch);require(yaw==0&&pitch==0,"neutral direction restores native view");
    next.orbitMode=0;ApplyAxes(next,.5f,1,.1f,yaw,pitch);require(std::fabs(yaw-5)<.001f&&std::fabs(pitch-10)<.001f,"rate mode retains speed and vertical rotation");
    next.orbitMode=2;require(!Valid(next),"unknown orbit mode rejected");
    next=State{};next.version=1;next.speed=101;State migrated;
    require(DecodeState(&next,LegacyStateBytes,migrated)&&migrated.version==4&&migrated.speed==101&&migrated.orbitMode==0,"legacy bindings preserve rate mode and tuning");
    require(!DecodeState(&next,sizeof(next),migrated),"old version cannot masquerade as new payload");
    next=State{};next.version=3;next.mouseSensitivity=.7f;
    require(DecodeState(&next,Version3StateBytes,migrated)&&migrated.version==4&&migrated.mouseSensitivity==.7f&&
        !migrated.vehicleHeading&&!migrated.hideMirror,"version 3 retains tuning and leaves new behavior opt-in");
    next=State{};next.vehicleHeading=2;require(!Valid(next),"unknown heading policy rejected");
    next=State{};next.hideMirror=2;require(!Valid(next),"unknown mirror policy rejected");
    session.Begin();next=State{};next.vehicleHeading=next.hideMirror=1;
    require(session.Apply(next)&&session.Dirty(),"new camera policies participate in native controls confirmation");
    session.Restore();require(!session.active.vehicleHeading&&!session.active.hideMirror,"discard restores camera policies");
    using namespace CustomizationOrbit;
    const Pose native={{1,2,3},{11,2,3}};Pose result;
    require(Rotate(native,90,0,true,result)&&result.eye.x==1&&result.eye.y==2&&result.eye.z==3&&
        std::fabs(result.look.x-1)<.001f&&std::fabs(result.look.y-12)<.001f,"first-person rotation retains exact eye position");
    require(Rotate(native,90,0,false,result)&&result.look.x==11&&result.look.y==2&&result.look.z==3,
        "outside orbit retains native target");
    require(!Rotate({{0,0,0},{0,0,0}},90,0,true,result),"degenerate pose falls back to native camera");
    const Pose drifting={{3,-4,2},{0,0,1}};
    require(VehicleHeading(drifting,{1,0,0},false,false,result)&&std::fabs(result.eye.x+5)<.001f&&
        result.eye.y==0&&result.eye.z==2&&result.look.z==1,"vehicle-relative chase bearing ignores native yaw lag, keeps height and distance");
    require(VehicleHeading(drifting,{0,1,0},false,false,result)&&result.eye.x==0&&std::fabs(result.eye.y+5)<.001f,
        "vehicle rotation changes the locked bearing without orbit accumulation");
    require(VehicleHeading(drifting,{1,0,0},false,true,result)&&std::fabs(result.eye.x-5)<.001f,
        "native look-back composes with the vehicle heading policy");
    require(VehicleHeading(native,{0,1,0},true,false,result)&&result.eye.x==native.eye.x&&
        std::fabs(result.look.y-12)<.001f,"fixed-eye vehicle bearing never relocates the driver camera");
    require(!VehicleHeading(native,{0,0,1},false,false,result),"vertical or invalid heading falls back to native pose");
    next=State{};MouseOrbit mouse;yaw=pitch=0;
    require(!mouse.Update({1,true,100,100,120},next,false,yaw,pitch)&&yaw==0&&mouse.zoom==1,"first frame discards stale mouse deltas");
    require(mouse.Update({2,true,10,-10,120},next,false,yaw,pitch)&&std::fabs(yaw-3)<.001f&&std::fabs(pitch-3)<.001f&&mouse.zoom<1,"native drag and wheel rotate and zoom");
    const float once=yaw;
    require(!mouse.Update({2,true,10,-10,120},next,false,yaw,pitch)&&yaw==once,"same input frame cannot rotate twice");
    require(!mouse.Update({3,false,20,20,0},next,false,yaw,pitch)&&mouse.owns&&yaw==once,"release holds mouse view without adding cursor movement");
    mouse.Update({4,true,10,10,120},next,true,yaw,pitch);
    require(!mouse.owns&&mouse.zoom==1&&yaw==0&&pitch==0,"controller camera input reclaims mouse view and zoom");
    mouse.Update({5,true,10,10,0},next,false,yaw,pitch);require(!mouse.owns,"held mouse cannot immediately reclaim after controller takeover");
    mouse.Update({6,false,0,0,0},next,false,yaw,pitch);mouse.Update({7,true,100,100,0},next,false,yaw,pitch);
    require(!mouse.owns,"fresh click does not apply prior movement");
    require(mouse.Update({8,true,10,0,0},next,false,yaw,pitch)&&mouse.owns,"new drag after release reclaims mouse");
    next.invertX=next.invertY=1;MouseOrbit inverted;float invertedYaw=0,invertedPitch=0;
    inverted.Update({1,true,0,0,0},next,false,invertedYaw,invertedPitch);
    inverted.Update({2,true,10,-10,0},next,false,invertedYaw,invertedPitch);
    require(std::fabs(invertedYaw+3)<.001f&&std::fabs(invertedPitch+3)<.001f,"mouse orbit respects both active inversion settings");
    next.mouseEnabled=0;mouse.Update({9,true,10,10,120},next,false,yaw,pitch);require(!mouse.owns&&mouse.zoom==1,"mouse option clears runtime ownership");
    require(Rotate(native,0,0,false,result,.5f)&&std::fabs(result.eye.x-6)<.001f,"wheel zoom changes external radius");
    require(Rotate(native,90,0,true,result,.5f)&&result.eye.x==1&&result.eye.y==2&&result.eye.z==3,"zoom never moves a first-person eye");
    std::cout<<"Control lifecycle state checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
