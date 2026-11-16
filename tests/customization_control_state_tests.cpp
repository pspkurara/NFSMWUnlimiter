#include "../CustomizationControlState.h"
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
    std::cout<<"Control lifecycle state checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
