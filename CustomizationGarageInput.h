#pragma once
#include <cstdint>

namespace CustomizationGarageInput {
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
