#pragma once
#include <cstdint>

namespace CustomizationFrontendList {
// Inspected PC FEMinList::AddNode takes (new node, predecessor). Its console
// declaration uses the opposite order; do not infer this ABI from that source.
template<class Word,class AddNode>
inline void AppendChild(Word* group,Word* child,AddNode add) {
    static_assert(sizeof(Word)==4,"PC frontend uses 32-bit words");
    add(group+0x5C/4,child,reinterpret_cast<Word*>(group[0x68/4]));
}
}
