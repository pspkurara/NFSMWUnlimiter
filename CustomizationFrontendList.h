#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

namespace CustomizationFrontendList {
// FEObject's native clone allocates a zeroed current draw-data block. Its
// animation states are cloned separately; a new static label needs the current
// color/transform too, especially nonzero scale. Keep the clone's own allocation.
template<class Word> bool CopyDrawData(Word* clone,const Word* source) {
    static_assert(sizeof(Word)==4,"PC frontend words");
    constexpr unsigned bytes=0x44;
    if(!clone||!source||!clone[0x2C/4]||!source[0x2C/4]||
       clone[0x2C/4]==source[0x2C/4]||clone[0x30/4]<bytes||source[0x30/4]<bytes)return false;
    std::memcpy(reinterpret_cast<void*>(clone[0x2C/4]),reinterpret_cast<const void*>(source[0x2C/4]),bytes);
    clone[0x1C/4]|=0x400000;return true;
}
// PC FEObjectCallback ABI: deleting destructor at slot 0, Visit at slot 1.
// Walk every matching state; FindObjectByHash stops at the first match.
template<class Word> class GroupsByName {
public:
    explicit GroupsByName(uint32_t name,uint32_t label=0):name_(name),label_(label){static_assert(sizeof(Word)==4,"PC frontend words");}
    virtual ~GroupsByName()=default;
    virtual bool Visit(Word* object) {
        if(object[0x18/4]!=5||groups.size()>=16)return true;
        bool matches=object[0x10/4]==name_;
        // PC controller prompts can live in a library-expanded group whose name
        // differs from HELP_BUTTON. Match its direct localized label as well.
        if(!matches&&label_&&object[0x60/4]<=32) {
            auto child=reinterpret_cast<Word*>(object[0x64/4]);
            for(unsigned i=0;i<object[0x60/4]&&child;++i,child=reinterpret_cast<Word*>(child[1]))
                if(child[0x18/4]==2&&child[0x60/4]==label_){matches=true;break;}
        }
        if(matches)groups.push_back(object);
        return true;
    }
    std::vector<Word*> groups;
private:
    uint32_t name_,label_;
};
template<class Word> class GroupContaining {
public:
    explicit GroupContaining(const Word* child):child_(child){}
    virtual ~GroupContaining()=default;
    virtual bool Visit(Word* object) {
        if(object[0x18/4]!=5||object[0x60/4]>32)return true;
        auto child=reinterpret_cast<Word*>(object[0x64/4]);
        for(unsigned i=0;i<object[0x60/4]&&child;++i,child=reinterpret_cast<Word*>(child[1]))
            if(child==child_){group=object;return false;}
        return true;
    }
    Word* group=nullptr;
private:
    const Word* child_;
};
// Inspected PC FEMinList::AddNode takes (new node, predecessor). Its console
// declaration uses the opposite order; do not infer this ABI from that source.
template<class Word,class AddNode>
inline void AppendChild(Word* group,Word* child,AddNode add) {
    static_assert(sizeof(Word)==4,"PC frontend uses 32-bit words");
    add(group+0x5C/4,child,reinterpret_cast<Word*>(group[0x68/4]));
}
}
