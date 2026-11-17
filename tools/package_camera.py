"""Build/test a camera-only distribution with independently buildable sources."""
from datetime import datetime, timezone, timedelta
from pathlib import Path
import hashlib
import json
import shutil
import zipfile
from package_customization import cmake_path, git_text, run

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    '.gitignore', 'standalone', 'CustomizationCamera.h', 'CustomizationControls.h',
    'CustomizationControlState.h', 'CustomizationOrbitMath.h', 'CustomizationGarageInput.h',
    'CustomizationMouseOrbit.h', 'CustomizationProfileHost.h',
    'CustomizationProfileStorage.h', 'CustomizationRuntimePeer.h',
    'NFSMWCustomizationCamera.ini', 'CustomizationCamera.md', 'CustomizationControls.md',
    'includes/injector', 'vendor/minhook',
    'tests/customization_control_state_tests.cpp',
    'tests/customization_controls_save_tests.cpp',
    'tests/customization_profile_host_tests.cpp',
    'tests/camera_runtime_fixture.cpp', 'tests/camera_runtime_peer_tests.cpp',
    'tools/check_driver_preview_bridge.py',
]


def main():
    if git_text(ROOT, 'status', '--porcelain'):
        raise SystemExit('Commit camera sources before packaging.')
    cmake = cmake_path(None)
    ctest = cmake.with_name('ctest.exe')
    build = ROOT / 'build-camera'
    run([cmake, '-S', ROOT / 'standalone', '-B', build, '-A', 'Win32'])
    run([cmake, '--build', build, '--config', 'Release', '--parallel', '4'])
    run([ctest, '--test-dir', build, '-C', 'Release', '--output-on-failure'])
    revision = git_text(ROOT, 'rev-parse', 'HEAD')
    stamp = datetime.now(timezone(timedelta(hours=9))).strftime('%Y%m%d-%H%M%S')
    name = f'NFSMWExtendedCamera-development-{stamp}-{revision[:8]}'
    stage = ROOT / 'dist' / name
    (stage / 'scripts').mkdir(parents=True, exist_ok=False)
    shutil.copy2(build / 'Release/NFSMWExtendedCamera.asi', stage / 'scripts')
    shutil.copy2(ROOT / 'NFSMWCustomizationCamera.ini', stage / 'scripts')
    shutil.copy2(ROOT / 'standalone/README.md', stage / 'README.md')
    shutil.copy2(ROOT / 'standalone/LICENSE', stage / 'LICENSE')
    (stage / 'docs').mkdir()
    for file in ('CustomizationCamera.md', 'CustomizationControls.md'):
        shutil.copy2(ROOT / file, stage / 'docs')
    shutil.copy2(ROOT / 'vendor/minhook/LICENSE.txt', stage / 'MinHook-LICENSE.txt')
    injector = (ROOT / 'includes/injector/injector.hpp').read_text(encoding='utf-8')
    (stage / 'Injector-NOTICE.txt').write_text(injector[:injector.index('*/')+2] + '\n', encoding='utf-8')
    run(['git', '-C', ROOT, 'archive', '--format=zip', f'--output={stage / "camera-source.zip"}', 'HEAD', *SOURCES])
    files = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(stage.rglob('*')) if p.is_file()}
    (stage / 'manifest.json').write_text(json.dumps({
        'schema': 1, 'kind': 'standalone-camera-development',
        'sourceCommit': revision, 'nativeRuntimeQA': False,
        'requiredMods': ['ordinary PC 1.3 ASI loader'], 'files': files,
    }, indent=2) + '\n', encoding='utf-8')
    archive = stage.with_suffix('.zip')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as output:
        for file in sorted(stage.rglob('*')):
            if file.is_file():
                output.write(file, name + '/' + file.relative_to(stage).as_posix())
    with zipfile.ZipFile(archive) as check:
        for file, digest in files.items():
            if hashlib.sha256(check.read(name + '/' + file)).hexdigest() != digest:
                raise RuntimeError('Archive verification failed: ' + file)
    print('Verified camera-only development snapshot:', archive)


if __name__ == '__main__':
    main()
