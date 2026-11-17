#pragma once
#include <cstdint>

namespace CustomizationGarageInput {
inline const char* ButtonName(uint32_t binding) {
    switch(binding) {
    case 0:return "Off";case 0x10:return "Start";case 0x20:return "Select";
    case 0x40:return "L3";case 0x80:return "R3";case 0x100:return "LB";case 0x200:return "RB";
    case 0x1000:return "A";case 0x2000:return "B";case 0x4000:return "X";case 0x8000:return "Y";
    case 0x10000:return "LT";case 0x20000:return "RT";default:return "Custom";
    }
}
inline float ButtonAmount(uint32_t binding,uint16_t buttons,uint8_t left,uint8_t right) {
    if(binding&buttons&0xFFFFu)return 1.f;
    const auto trigger=[](uint8_t value){return value>30?float(value-30)/225.f:0.f;};
    float amount=(binding&0x10000u)?trigger(left):0.f;
    const float other=(binding&0x20000u)?trigger(right):0.f;
    return amount>other?amount:other;
}
struct FocusBoundary {
    uint64_t last=0;uintptr_t mover=0;bool focused=false;
    bool Observe(bool foreground,uint64_t now,uintptr_t currentMover) {
        // A minimized game can stop frontend updates entirely. In that case no
        // background frame ever observes focus loss; a time gap is a boundary too.
        const bool ready=foreground&&focused&&last&&now>=last&&now-last<=250&&mover==currentMover;
        focused=foreground;last=now;mover=currentMover;return ready;
    }
};
inline bool ForwardZoom(bool suppressed,int mode,float speed) {
    // Native zero-speed setters also switch cinematic/automatic movers to
    // manual. Releases only need forwarding once manual input actually owns it.
    return !suppressed||mode==1||speed!=0.f;
}
// Drain native mapped events while suppressing only their competing rotation.
// Native zoom remains live; only focus loss disables all manual camera events.
template<class Word,class Invoke>
void Drain(Word* garage,uint8_t& showcase,bool inputReady,bool& suppressed,Invoke invoke) {
    static_assert(sizeof(Word)==4,"PC garage words");
    auto& rotate=*(reinterpret_cast<uint8_t*>(garage)+0x78);
    const auto savedRotate=rotate,savedShowcase=showcase;
    const uint32_t vertical=garage[0x7C/4],horizontal=garage[0x80/4];
    const bool previous=suppressed;
    suppressed=true;
    if(!inputReady)rotate=showcase=0;
    invoke();
    suppressed=previous;
    rotate=savedRotate;showcase=savedShowcase;
    garage[0x7C/4]=vertical;garage[0x80/4]=horizontal;
}
}
