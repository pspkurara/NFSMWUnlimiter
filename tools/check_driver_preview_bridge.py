"""Execute the production driver-preview cave with relocated native resumes."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cmake', default='cmake')
args = parser.parse_args()
cmake = shutil.which(args.cmake)
if not cmake:
    raise SystemExit('CMake not found; supply --cmake with its executable path.')
root = Path(__file__).resolve().parents[1]
text = (root / 'CustomizationCamera.h').read_text()
start = text.index('static void __declspec(naked) DriverVisibilityCave()')
end = text.index('\n    }\n', start)+7
cave = text[start:end]
for before, after in (('[0x925E90]', '[GameFlow]'),
                      ('push 0x74EA1E', 'push offset PreviewReturn'),
                      ('push 0x74EC55', 'push offset NativeReturn')):
    if cave.count(before) != 1:
        raise SystemExit('Production cave changed; revalidate fixture relocation: '+before)
    cave = cave.replace(before, after)
fixture = r'''
#include <cstdio>
#include <initializer_list>
static unsigned GameFlow=0,NativeVisibility=0;
static bool DriverPreviewActive=false;
static void __declspec(naked) NativeReturn() {
    __asm {
        and eax,1
        cmp ecx,3
        jne nativeFlags
        xor eax,eax
    nativeFlags:
        and eax,dword ptr [NativeVisibility]
        ret
    }
}
static void __declspec(naked) PreviewReturn() {
    __asm {test eax,eax
        ret}
}
'''+cave+r'''
int main() {
    unsigned cases=0;
    for(unsigned flow:{0u,3u,6u})for(bool active:{false,true})
    for(unsigned visibility:{0u,1u})for(unsigned partFlags:{0u,1u,0x100u,0x101u}) {
        GameFlow=flow;DriverPreviewActive=active;NativeVisibility=visibility;
        unsigned before=0,after=0,result=0;
        __asm {
            mov before,esp
            mov eax,partFlags
            call DriverVisibilityCave
            mov result,eax
            mov after,esp
        }
        const unsigned expected=flow==3?(active?(partFlags&1u):0u):((partFlags&1u)&visibility);
        if(before!=after||result!=expected||GameFlow!=flow||DriverPreviewActive!=active||NativeVisibility!=visibility)return 1;
        ++cases;
    }
    std::printf("Driver preview: %u production-cave cases passed; native flags and stack preserved\n",cases);
}
'''
work = root / 'build-driver-check'
work.mkdir(exist_ok=True)
(work / 'fixture.cpp').write_text(fixture)
(work / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(DriverPreviewCheck LANGUAGES CXX)
add_executable(driver_preview fixture.cpp)
enable_testing()
add_test(NAME production_driver_preview COMMAND driver_preview)
''')
env = {key.upper(): value for key, value in os.environ.items()}
for command in ([cmake, '-S', work, '-B', work / 'build', '-A', 'Win32'],
                [cmake, '--build', work / 'build', '--config', 'Release'],
                [str(Path(cmake).with_name('ctest.exe')), '--test-dir', work / 'build', '-C', 'Release', '--output-on-failure']):
    subprocess.run([str(value) for value in command], env=env, check=True)
