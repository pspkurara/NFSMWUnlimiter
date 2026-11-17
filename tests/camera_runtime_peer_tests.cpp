#include <windows.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
static void Check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
template<class F> static F Symbol(HMODULE module,const char* name) {
    auto pointer=GetProcAddress(module,name);
    if(!pointer)pointer=GetProcAddress(module,(std::string("_")+name).c_str());
    Check(pointer!=nullptr,name);return reinterpret_cast<F>(pointer);
}
int main(int argc,char** argv) {try {
    Check(argc==3,"Two fixture ASI paths required");
    auto first=LoadLibraryA(argv[1]),second=LoadLibraryA(argv[2]);
    Check(first&&second,"Load both fixture modules");
    using U=uint32_t(__cdecl*)();using V=void(__cdecl*)();using Set=void(__cdecl*)(float);using Get=float(__cdecl*)();
    Check(Symbol<U>(first,"FixtureStart")()&&Symbol<U>(second,"FixtureStart")(),"Both modules initialize");
    Check(Symbol<U>(first,"FixtureOwners")()==3&&Symbol<U>(second,"FixtureOwners")()==0,"Only the first owns controls and camera hooks");
    Symbol<Set>(first,"FixtureSet")(137);
    Check(Symbol<Get>(second,"FixtureGet")()==137,"Second module reads the active owner state");
    Symbol<V>(first,"FixtureBegin")();Symbol<Set>(first,"FixtureSet")(211);
    Check(Symbol<Get>(second,"FixtureGet")()==211,"Live edits share one controls session");
    Symbol<V>(first,"FixtureRestore")();
    Check(Symbol<Get>(second,"FixtureGet")()==137,"Native discard restores the state seen by both modules");
    Symbol<void(__cdecl*)(uint32_t)>(second,"FixtureMirror")(1);
    Check(Symbol<U>(first,"FixtureMirrorState")()==1,"Camera capabilities reach the controls owner");
    Symbol<void(__cdecl*)(uint32_t)>(second,"FixtureMirror")(3);
    Check(Symbol<U>(first,"FixtureMirrorState")()==3,"Radar capability reaches the controls owner independently");
    Check(Symbol<U>(second,"FixtureRegisterTick")()&&Symbol<U>(second,"FixtureRegisterTick")(),"Peer garage observer registration is idempotent");
    Symbol<V>(first,"FixtureTick")();
    Check(Symbol<U>(second,"FixtureTicks")()==1,"Owner dispatches the peer's stance-menu update");
    Check(!Symbol<U>(first,"FixtureRegisterTick")(),"A different observer cannot replace the active menu owner");
    Check(Symbol<U>(second,"FixtureInput")()&&Symbol<U>(first,"FixtureInputState")(),"Input provider export forwards to the camera owner");
    Check(Symbol<U>(first,"FixtureStart")()&&Symbol<U>(second,"FixtureStart")(),"Repeated initialization is idempotent");
    Check(Symbol<U>(first,"FixtureGarageIdle")()&&Symbol<U>(second,"FixtureGarageIdle")(),
        "Neutral stick and release cannot switch an automatic garage camera to manual");
    std::cout<<"Cross-ASI runtime ownership, shared controls, cancel, mirror, menu and provider passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
