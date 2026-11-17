#include "../CustomizationStanceSession.h"
#include <iostream>
using namespace CustomizationStance;
int main() {
    MenuInfo info;info.token=1;info.baselineMask=0xFFF;
    for(int c=0;c<6;++c){info.minimum[c]=-10;info.maximum[c]=10;info.step[c]=.5f;info.inherited[c*2]=1;info.inherited[c*2+1]=2;}
    Session s;Values values;
    values.mask=1;values.values[0]=20; // Previously saved absolute value outside the current UI range.
    if(!s.Begin(info,values)||s.Dirty()||s.Value(0,0)!=20)return 1;
    s.axle=2;if(s.Dirty())return 2;
    if(!s.Step(0,1)||s.Value(0,0)!=20||s.Value(0,1)!=2.5f)return 3;
    s.Inherit(0);if(s.Dirty()||s.active.mask!=1||s.Value(0,0)!=20)return 4;
    s.Begin(info,{});s.Step(1,1);
    if(s.Value(1,0)!=1.5f||s.Value(1,1)!=1.5f||s.active.mask!=12)return 5;
    s.Inherit(1);if(s.Dirty())return 6;
    s.enabled=true;if(!s.Dirty())return 7;
    s.Begin(info,{});s.axle=1;s.Step(2,1,5);
    if(s.Value(2,0)!=3.5f||s.Value(2,1)!=2)return 9;
    s.Step(2,-1,.1f);if(std::fabs(s.Value(2,0)-3.45f)>.00001f)return 10;
    s.Step(2,1,1000);if(s.Value(2,0)!=10||s.Value(2,1)!=2)return 11;
    const auto unchanged=s.active;
    if(s.Step(2,1,0)||s.Step(2,1,NAN)||std::memcmp(&unchanged,&s.active,sizeof(unchanged)))return 12;
    s.Begin(info,{});s.SelectAxle(2);s.Step(3,1);s.SelectAxle(0);
    if(s.Value(3,0)!=1||s.Value(3,1)!=2.5f)return 13; // Switching never synchronizes.
    s.Step(3,1);if(s.Value(3,0)!=3||s.Value(3,1)!=3||s.Value(4,0)!=1||s.Value(4,1)!=2)return 14;
    s.SelectAxle(1);if(!s.CopyToOther(4)||s.Value(4,1)!=1||s.Value(3,1)!=3)return 15;
    s.SelectAxle(2);s.Step(4,-1);if(!s.CopyToOther(4)||s.Value(4,0)!=.5f)return 16;
    s.SelectAxle(0);const auto noCopy=s.active;
    if(s.CopyToOther(4)||std::memcmp(&noCopy,&s.active,sizeof(noCopy)))return 17;
    s.Begin(info,values);s.SelectAxle(1);
    if(!s.CopyToOther(0)||s.Value(0,0)!=20||s.Value(0,1)!=20||s.Value(1,1)!=2)return 18;
    s.enabled=true;s.SelectAxle(2);s.Step(1,1);s.InheritAll();
    if(s.active.mask||s.axle!=2||!s.enabled||s.Value(0,0)!=1||s.Value(1,1)!=2)return 19;
    info.step[0]=.005f;info.inherited[0]=.833f;
    s.Begin(info,{});s.SelectAxle(1);
    if(s.Dirty()||s.Value(0,0)!=.833f)return 20;
    s.Step(0,1,5,true);
    if(std::fabs(s.Value(0,0)-.85f)>1e-6)return 21;
    s.Step(0,-1,5,true);const float grid=s.Value(0,0);
    for(unsigned i=0;i<10000;++i){s.Step(0,1,5,true);s.Step(0,-1,5,true);}
    if(s.Value(0,0)!=grid||std::fabs(grid-.825f)>1e-6)return 22;
    s.Step(0,1,.1f,false);
    if(std::fabs(s.Value(0,0)-.8255f)>1e-6)return 23;
    const auto precise=s.active;s.SelectAxle(0);s.SelectAxle(1);
    if(std::memcmp(&precise,&s.active,sizeof(precise)))return 24;
    info.step[0]=0;if(s.Begin(info,{}))return 8;
    std::cout<<"Native stance session: inheritance, linked deltas, ranges and grouped changes passed\n";
    return 0;
}
