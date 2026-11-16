#pragma once
#include <cstdint>
#include <map>

// Transient native-object lineage. Equipment equality alone never creates a link.
// The host supplies fingerprints only while the native objects are alive.
namespace CustomizationRandomState {
template<class Values> class Table {
public:
    struct Entry { uint64_t fingerprint{}, generation{}; Values values{}; };
    void Clear() { entries.clear(); }
    void Erase(const void* object) { entries.erase(object); }
    const Entry* Find(const void* object,uint64_t fingerprint) {
        auto found=entries.find(object);
        if(found==entries.end())return nullptr;
        if(found->second.fingerprint!=fingerprint) {entries.erase(found);return nullptr;}
        return &found->second;
    }
    void Put(const void* object,const Entry& entry) {
        if(!object)return;
        // Refuse new associations at the cap rather than evicting live racers.
        if(entries.size()<4096||entries.count(object))entries[object]=entry;
    }
    void Copy(const void* source,uint64_t fingerprint,const void* destination) {
        const auto found=Find(source,fingerprint);
        const bool known=found!=nullptr;
        const Entry value=found?*found:Entry{};
        Erase(destination);
        if(known)Put(destination,value);
    }
private:
    std::map<const void*,Entry> entries;
};
}
