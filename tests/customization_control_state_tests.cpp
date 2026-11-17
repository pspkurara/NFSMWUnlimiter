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
    State mirrorSettings;mirrorSettings.enabled=mirrorSettings.hideMirror=1;
    MirrorPose mirror;mirror.Observe(1000,180,0);
    require(mirror.Hide(mirrorSettings,1,true,true,1001),"rotated main view hides mirror");
    require(mirror.HideRadar(mirrorSettings,true,true,true,1001),"camera auto-hide suppresses the replacement radar panel by default");
    require(!mirror.HideRadar(mirrorSettings,false,true,true,1001),"native mirror OFF retains native radar policy");
    mirrorSettings.showRadarOnMirrorHide=1;
    require(mirror.Hide(mirrorSettings,1,true,true,1001)&&!mirror.HideRadar(mirrorSettings,true,true,true,1001),
        "radar opt-in is independent of camera mirror auto-hide");
    mirrorSettings.showRadarOnMirrorHide=0;
    require(!mirror.Hide(mirrorSettings,3,true,true,1001),"mirror and other views retain native policy");
    require(!mirror.Hide(mirrorSettings,1,false,true,1001)&&!mirror.Hide(mirrorSettings,1,true,false,1001),"flow/focus changes release mirror policy");
    require(!mirror.Hide(mirrorSettings,1,true,true,1251),"stale mover cannot retain hidden mirror");
    mirror.Observe(1300,360,0);require(!mirror.Hide(mirrorSettings,1,true,true,1300),"full-circle neutral restores native policy");
    require(!mirror.HideRadar(mirrorSettings,true,true,true,1300),"neutral restores native radar policy");
    mirror.Observe(1400,0,5);require(mirror.Hide(mirrorSettings,1,true,true,1400),"vertical orbit hides mirror");
    mirrorSettings.hideMirror=0;require(!mirror.Hide(mirrorSettings,1,true,true,1400),"mirror option is independent");
    mirrorSettings.hideMirror=1;mirror.Clear();require(!mirror.Hide(mirrorSettings,1,true,true,1400),"reset releases mirror immediately");
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
    require(DecodeState(&next,LegacyStateBytes,migrated)&&migrated.version==5&&migrated.speed==101&&migrated.orbitMode==0,"legacy bindings preserve rate mode and tuning");
    require(!DecodeState(&next,sizeof(next),migrated),"old version cannot masquerade as new payload");
    next=State{};next.version=3;next.mouseSensitivity=.7f;
    require(DecodeState(&next,Version3StateBytes,migrated)&&migrated.version==5&&migrated.mouseSensitivity==.7f&&
        !migrated.vehicleHeading&&!migrated.hideMirror,"version 3 retains tuning and leaves new behavior opt-in");
    next=State{};next.version=4;next.invertX=1;
    require(DecodeState(&next,Version4StateBytes,migrated)&&migrated.version==5&&migrated.invertX&&
        migrated.directionInvertX&&migrated.mouseInvertX&&!migrated.directionInvertY&&!migrated.mouseInvertY,
        "version 4 preserves shared inversion separately for every input source");
    next=State{};next.invertX=1;float ix=1,iy=1;InvertStick(next,ix,iy);
    require(ix==-1&&iy==1,"rate inversion uses only rate options");
    next.orbitMode=1;ix=iy=1;InvertStick(next,ix,iy);
    require(ix==1&&iy==1,"rate inversion cannot change direction input");
    next.directionInvertY=1;ix=iy=1;InvertStick(next,ix,iy);
    require(ix==1&&iy==-1,"direction input has its own inversion");
    next=State{};next.vehicleHeading=2;require(!Valid(next),"unknown heading policy rejected");
    next=State{};next.hideMirror=2;require(!Valid(next),"unknown mirror policy rejected");
    next=State{};next.showRadarOnMirrorHide=2;require(!Valid(next),"unknown radar policy rejected");
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
    Frame flat,tilted;
    require(VehicleFrame({0,0,0},{1,0,0},{0,0,1},flat)&&
        VehicleFrame({10,20,30},{.8660254f,0,.5f},{-.5f,0,.8660254f},tilted),"valid complete vehicle frames");
    VehicleOrbit locked;
    require(locked.Compose(drifting,flat,false,false,0,0,1,result),"capture local camera reference");
    const auto firstLocal=flat.Local(result.eye);
    const Pose disturbed={tilted.World({-10,0,8}),tilted.World({0,0,4})};
    require(locked.Compose(disturbed,tilted,false,false,0,0,1,result),"compose vehicle-fixed camera after native elevation changes");
    const auto fixedLocal=tilted.Local(result.eye),fixedLook=tilted.Local(result.look);
    require(std::fabs(fixedLocal.x-firstLocal.x)<.001f&&std::fabs(fixedLocal.z-firstLocal.z)<.001f&&
        std::fabs(fixedLook.z-1)<.001f,"native elevation and target animation cannot change the vehicle-local pose");
    require(locked.Compose(disturbed,tilted,false,true,0,0,1,result)&&
        std::fabs(tilted.Local(result.eye).x+firstLocal.x)<.001f,"native back view reverses bearing without changing the locked elevation");
    locked.Clear();require(!locked.valid,"POV/mover reset releases local reference");
    require(!VehicleFrame({0,0,0},{0,0,1},{0,0,1},flat),"degenerate vehicle frame rejected");
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
    next.invertX=next.invertY=next.directionInvertX=next.directionInvertY=1;
    MouseOrbit isolated;float isolatedYaw=0,isolatedPitch=0;
    isolated.Update({1,true,0,0,0},next,false,isolatedYaw,isolatedPitch);
    isolated.Update({2,true,10,-10,0},next,false,isolatedYaw,isolatedPitch);
    require(std::fabs(isolatedYaw-3)<.001f&&std::fabs(isolatedPitch-3)<.001f,"stick inversion cannot affect mouse rotation");
    next.mouseInvertX=next.mouseInvertY=1;MouseOrbit inverted;float invertedYaw=0,invertedPitch=0;
    inverted.Update({1,true,0,0,0},next,false,invertedYaw,invertedPitch);
    inverted.Update({2,true,10,-10,0},next,false,invertedYaw,invertedPitch);
    require(std::fabs(invertedYaw+3)<.001f&&std::fabs(invertedPitch+3)<.001f,"mouse orbit respects both active inversion settings");
    next.mouseEnabled=0;mouse.Update({9,true,10,10,120},next,false,yaw,pitch);require(!mouse.owns&&mouse.zoom==1,"mouse option clears runtime ownership");
    require(Rotate(native,0,0,false,result,.5f)&&std::fabs(result.eye.x-6)<.001f,"wheel zoom changes external radius");
    require(Rotate(native,90,0,true,result,.5f)&&result.eye.x==1&&result.eye.y==2&&result.eye.z==3,"zoom never moves a first-person eye");
    std::cout<<"Control lifecycle state checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
