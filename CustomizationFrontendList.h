#pragma once
#include <cstdint>
#include <vector>

namespace CustomizationFrontendList {
// PC FEObjectCallback ABI: deleting destructor at slot 0, Visit at slot 1.
// Walk every matching state; FindObjectByHash stops at the first match.
template<class Word> class GroupsByName {
public:
    explicit GroupsByName(uint32_t name):name_(name){static_assert(sizeof(Word)==4,"PC frontend words");}
    virtual ~GroupsByName()=default;
    virtual bool Visit(Word* object) {
        if(object[0x18/4]==5&&object[0x10/4]==name_&&groups.size()<16)groups.push_back(object);
        return true;
    }
    std::vector<Word*> groups;
private:
    uint32_t name_;
};
// Inspected PC FEMinList::AddNode takes (new node, predecessor). Its console
// declaration uses the opposite order; do not infer this ABI from that source.
template<class Word,class AddNode>
inline void AppendChild(Word* group,Word* child,AddNode add) {
    static_assert(sizeof(Word)==4,"PC frontend uses 32-bit words");
    add(group+0x5C/4,child,reinterpret_cast<Word*>(group[0x68/4]));
}
}
