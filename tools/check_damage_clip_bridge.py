"""Execute the exact production x86 cave with an isolated native-call fixture."""
from pathlib import Path
import os
import subprocess
from package_customization import cmake_path

root = Path(__file__).resolve().parents[1]
source = (root / 'CustomizationBridge.h').read_text()
start = source.index('static void __declspec(naked) DamageHiddenCave() {')
end = source.index('\n}\n', start) + 3
cave = source[start:end]
assert cave.count('0x74EA86') == 1
# Relocate only the native resume PC; Windows can reserve the original range
# for the fixture process's heap. Instructions and calling convention stay exact.
cave = cave.replace('0x74EA86', '0x50000000')
work = root / 'build-clip-check'
work.mkdir(exist_ok=True)
fixture = r'''
#include <windows.h>
#include <cstdio>
static unsigned hiddenObserved=0,calls=0;
static void __stdcall ObserveHiddenPanel(unsigned hidden) {++calls;hiddenObserved=hidden&255;}
static unsigned __declspec(naked) NativeQuery() {
    __asm {mov eax,[esp+4]
        ret 4}
}
'''+cave+r'''
int main() {
    auto memory=VirtualAlloc(reinterpret_cast<void*>(0x50000000),0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(memory!=reinterpret_cast<void*>(0x50000000))return 2;
    *reinterpret_cast<unsigned char*>(0x50000000)=0xC3;
    FlushInstructionCache(GetCurrentProcess(),nullptr,0);
    void* table[8]{};table[7]=reinterpret_cast<void*>(&NativeQuery);
    auto vtable=table;
    for(unsigned input: {0x101u,0x100u}) {
        unsigned before=0,after=0,result=0,zero=0;
        __asm {
            mov before,esp
            lea ecx,resumed
            push ecx
            mov eax,vtable
            push input
            // The production site enters via JMP with the native argument
            // already on top of the stack, not a newly pushed CALL return PC.
            jmp DamageHiddenCave
        resumed:
            sete cl
            movzx ecx,cl
            mov zero,ecx
            mov result,eax
            mov after,esp
        }
        if(before!=after||result!=input||hiddenObserved!=(input&255)||zero!=((input&255)==0))return 3;
    }
    VirtualFree(memory,0,MEM_RELEASE);
    if(calls!=2)return 4;
    std::puts("Production damage-visibility cave preserves EAX, TEST flags and native argument/stack cleanup");
}
'''
(work / 'fixture.cpp').write_text(fixture.replace('#include <cstdio>', '#include <cstdio>\n#include <initializer_list>'))
(work / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(DamageClipCheck LANGUAGES CXX)
add_executable(check fixture.cpp)
target_link_options(check PRIVATE /DYNAMICBASE:NO /BASE:0x400000)
enable_testing()
add_test(NAME production_damage_clip_cave COMMAND check)
''')
cmake = cmake_path(None)
env = {k.upper(): v for k, v in os.environ.items()}
for command in ([cmake, '-S', work, '-B', work / 'build', '-A', 'Win32'],
                [cmake, '--build', work / 'build', '--config', 'Release'],
                [cmake.with_name('ctest.exe'), '--test-dir', work / 'build', '-C', 'Release', '--output-on-failure']):
    subprocess.run([str(x) for x in command], env=env, check=True)
