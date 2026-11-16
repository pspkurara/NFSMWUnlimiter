#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace CustomizationStance {
struct Values {uint32_t size=sizeof(Values),mask=0;float values[12]{};};
struct MenuInfo {
    uint32_t size=sizeof(MenuInfo),version=1;uint64_t token=0;uint32_t enabled=0,baselineMask=0;
    float inherited[12]{},minimum[6]{},maximum[6]{},step[6]{};
};
static_assert(sizeof(Values)==56&&sizeof(MenuInfo)==144,"Native stance ABI 1");
struct Session {
    MenuInfo info{};Values before{},active{};unsigned axle=0,referenceAxle=0;bool enabled=false;
    bool Begin(const MenuInfo& settings,const Values& initial) {
        if(settings.size!=sizeof(MenuInfo)||settings.version!=1||!settings.token||settings.enabled>1||settings.baselineMask!=0xFFF||
           initial.size!=sizeof(Values)||(initial.mask&~0xFFFu))return false;
        for(unsigned c=0;c<6;++c) {
            if(!std::isfinite(settings.minimum[c])||!std::isfinite(settings.maximum[c])||!std::isfinite(settings.step[c])||
               settings.minimum[c]>=settings.maximum[c]||settings.step[c]<=0)return false;
            for(unsigned a=0;a<2;++a)if(!std::isfinite(settings.inherited[c*2+a])||
                ((initial.mask&(1u<<(c*2+a)))&&!std::isfinite(initial.values[c*2+a])))return false;
        }
        info=settings;before=active=initial;axle=referenceAxle=0;enabled=settings.enabled!=0;return true;
    }
    float Value(unsigned c,unsigned a) const {const auto i=c*2+a;return active.mask&(1u<<i)?active.values[i]:info.inherited[i];}
    bool Includes(unsigned a) const {return !axle||axle==a+1;}
    void SelectAxle(unsigned next) {if(next>2)return;axle=next;if(next)referenceAxle=next-1;}
    bool Step(unsigned c,int direction,float multiplier=1) {
        if(c>=6||(direction!=-1&&direction!=1)||!std::isfinite(multiplier)||multiplier<=0)return false;
        const double next=double(Value(c,axle?axle-1:referenceAxle))+double(info.step[c])*direction*multiplier;
        const float value=float(std::clamp(next,double(info.minimum[c]),double(info.maximum[c])));
        for(unsigned a=0;a<2;++a)if(Includes(a)) {
            const auto i=c*2+a;
            active.values[i]=value;
            active.mask|=1u<<i;
        }
        return true;
    }
    bool CopyToOther(unsigned c) {
        if(c>=6||!axle||axle>2)return false;
        const unsigned source=axle-1,target=c*2+(1-source);
        // Copy the displayed absolute value, including an inherited source.
        // Never clamp a copied saved value to a subsequently narrowed UI range.
        active.values[target]=Value(c,source);active.mask|=1u<<target;return true;
    }
    void Inherit(unsigned c) {if(c<6)for(unsigned a=0;a<2;++a)if(Includes(a))active.mask&=~(1u<<(c*2+a));}
    bool Dirty() const {
        if(enabled!=(info.enabled!=0)||before.mask!=active.mask)return true;
        for(unsigned i=0;i<12;++i)if((active.mask&(1u<<i))&&active.values[i]!=before.values[i])return true;
        return false;
    }
};
}
