#include "../CustomizationControlState.h"
#include "../CustomizationOrbitMath.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace CustomizationControls;
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
int main(){try{
    Session session;
    require(Valid(session.active),"valid defaults");
    require(session.active.resetButtons==0,"defaults must not shadow native R3 camera-view action");
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
    require(DecodeState(&next,LegacyStateBytes,migrated)&&migrated.version==2&&migrated.speed==101&&migrated.orbitMode==0,"legacy bindings preserve rate mode and tuning");
    require(!DecodeState(&next,sizeof(next),migrated),"old version cannot masquerade as new payload");
    using namespace CustomizationOrbit;
    const Pose native={{1,2,3},{11,2,3}};Pose result;
    require(Rotate(native,90,0,true,result)&&result.eye.x==1&&result.eye.y==2&&result.eye.z==3&&
        std::fabs(result.look.x-1)<.001f&&std::fabs(result.look.y-12)<.001f,"first-person rotation retains exact eye position");
    require(Rotate(native,90,0,false,result)&&result.look.x==11&&result.look.y==2&&result.look.z==3,
        "outside orbit retains native target");
    require(!Rotate({{0,0,0},{0,0,0}},90,0,true,result),"degenerate pose falls back to native camera");
    std::cout<<"Control lifecycle state checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
