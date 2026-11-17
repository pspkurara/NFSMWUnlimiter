#pragma once
#include "CustomizationStanceSession.h"
#include <optional>

namespace CustomizationStance {
// One native shop owns one car at a time. This holds no native pointers and never
// writes a save: the adapter alone promotes a successful checkout to owned data.
class Shop {
public:
    struct Snapshot { Values values{}; bool enabled=false; };
    struct Item { uint64_t revision=0; Snapshot snapshot{}; bool active=true; };

    bool Enter(uint64_t owner,const Snapshot& purchased) {
        if(!owner||!Valid(purchased))return false;
        if(owner_==owner)return true; // Reopening a screen must retain its cart.
        Clear();owner_=owner;purchased_=purchased;return true;
    }
    bool Owns(uint64_t owner) const {return owner&&owner_==owner;}
    const Snapshot* Purchased(uint64_t owner) const {return Owns(owner)?&purchased_:nullptr;}
    const Snapshot* Initial(uint64_t owner) const {
        if(!Owns(owner))return nullptr;
        return cart_?&cart_->snapshot:&purchased_;
    }
    const Snapshot* Visible(uint64_t owner) const {
        if(!Owns(owner))return nullptr;
        if(preview_)return &*preview_;
        return cart_&&cart_->active?&cart_->snapshot:&purchased_;
    }
    const Item* Cart(uint64_t owner) const {return Owns(owner)&&cart_?&*cart_:nullptr;}
    bool CheckingOut(uint64_t owner) const {return Owns(owner)&&checkout_.has_value();}
    bool SetActive(uint64_t owner,uint64_t revision,bool active) {
        if(!Owns(owner)||!cart_||cart_->revision!=revision||checkout_)return false;
        cart_->active=active;return true;
    }
    bool Begin(uint64_t owner) {
        const auto initial=Initial(owner);if(!initial||editing_||checkout_)return false;
        preview_=*initial;editing_=true;return true;
    }
    bool Preview(uint64_t owner,const Snapshot& snapshot) {
        if(!Owns(owner)||!editing_||checkout_||!Valid(snapshot))return false;
        preview_=snapshot;return true;
    }
    bool Cancel(uint64_t owner) {
        if(!Owns(owner)||!editing_)return false;
        preview_.reset();editing_=false;return true;
    }
    // Native insertion must succeed BEFORE Accept is called. Replacing an item
    // creates a new revision, so a delayed remove callback cannot erase its replacement.
    uint64_t Accept(uint64_t owner) {
        if(!Owns(owner)||!editing_||checkout_||!preview_)return 0;
        if(++nextRevision_==0)++nextRevision_;
        cart_=Item{nextRevision_,*preview_};preview_.reset();editing_=false;
        return nextRevision_;
    }
    bool Remove(uint64_t owner,uint64_t revision) {
        if(!Owns(owner)||!cart_||cart_->revision!=revision||checkout_||editing_)return false;
        cart_.reset();return true;
    }
    bool BeginCheckout(uint64_t owner,uint64_t revision) {
        if(!Owns(owner)||!cart_||!cart_->active||cart_->revision!=revision||editing_||checkout_)return false;
        checkout_=*cart_;return true;
    }
    // A false result leaves the exact item queued. A true result returns the
    // absolute snapshot the adapter must persist, once; it never rebases values.
    std::optional<Snapshot> EndCheckout(uint64_t owner,bool succeeded) {
        if(!Owns(owner)||!checkout_)return std::nullopt;
        const auto item=*checkout_;checkout_.reset();
        if(!succeeded)return std::nullopt;
        purchased_=item.snapshot;
        if(cart_&&cart_->revision==item.revision)cart_.reset();
        return purchased_;
    }
    void Clear() {
        owner_=0;purchased_={};cart_.reset();preview_.reset();checkout_.reset();editing_=false;
        // Keep revisions monotonic across shop/car changes.
    }
private:
    static bool Valid(const Snapshot& snapshot) {
        if(snapshot.values.size!=sizeof(Values)||(snapshot.values.mask&~0xFFFu))return false;
        for(unsigned i=0;i<12;++i)if((snapshot.values.mask&(1u<<i))&&!std::isfinite(snapshot.values.values[i]))return false;
        return true;
    }
    uint64_t owner_=0,nextRevision_=0;
    Snapshot purchased_{};
    std::optional<Item> cart_,checkout_;
    std::optional<Snapshot> preview_;
    bool editing_=false;
};
}
