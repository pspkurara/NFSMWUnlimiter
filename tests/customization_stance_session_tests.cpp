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
    if(s.Value(1,0)!=1.5f||s.Value(1,1)!=2.5f||s.active.mask!=12)return 5;
    s.Inherit(1);if(s.Dirty())return 6;
    s.enabled=true;if(!s.Dirty())return 7;
    info.step[0]=0;if(s.Begin(info,{}))return 8;
    std::cout<<"Native stance session: inheritance, linked deltas, ranges and grouped changes passed\n";
    return 0;
}
