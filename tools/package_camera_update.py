"""Rebuild the installed baseline with the current camera implementation.

This intentionally excludes the experimental stance menu and race ownership hooks.
The exported sources retain the baseline's profile lifecycle implementation.
"""
from pathlib import Path
import hashlib
import io
import json
import os
import subprocess
import zipfile
from datetime import datetime
from package_customization import ROOT, cmake_path, run

BASE = '18894c9'
FILES = (
    'CustomizationCamera.h', 'CustomizationControlState.h', 'CustomizationControls.h',
    'CustomizationOrbitMath.h', 'CustomizationMouseOrbit.h', 'CustomizationRuntimePeer.h',
    'CustomizationCamera.md', 'CustomizationControls.md',
    'NFSMWCustomizationCamera.ini', 'tests/customization_control_state_tests.cpp',
    'tests/customization_controls_save_tests.cpp',
)


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise RuntimeError('Baseline controls adapter no longer matches: ' + before[:80])
    return text.replace(before, after)


def baseline_controls(text):
    # Keep the installed save host and its callback lifecycle. Do not install the
    # independent profile host over the baseline's existing native save hooks.
    text = replace_once(text, '#include "CustomizationProfileStorage.h"\n#include "CustomizationProfileHost.h"',
                        '#include "CustomizationSave.h"')
    text = text.replace('CustomizationProfileStorage::', 'CustomizationSave::')
    start = text.index('static const CustomizationProfileHost::Observer ControlsObserver=')
    end = text.index('static std::wstring Text(', start)
    text = text[:start] + text[end:]
    text = replace_once(text, 'static bool Install(bool nativeLifecycleReady)', 'static bool Install()')
    text = replace_once(text, 'if(!nativeLifecycleReady)return false;', 'if(!CustomizationSave::HooksReady)return false;')
    text = replace_once(text, 'if(CustomizationProfileHost::Register(ControlsObserver)&&PublishRuntime()) {',
        'if(PublishRuntime()) {\n'
        '                CustomizationSave::ControlsBeginSave=&OnBeginSave;CustomizationSave::ControlsEndSave=&OnEndSave;\n'
        '                CustomizationSave::ControlsDone=&OnSaveDone;CustomizationSave::ControlsBeginLoad=&OnBeginLoad;\n'
        '                CustomizationSave::ControlsEndLoad=&OnEndLoad;')
    return text


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    if git('status', '--porcelain').strip():
        raise SystemExit('Commit the camera update sources before packaging.')
    destination = ROOT / 'dist' / ('camera-update-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    source = destination / 'source'
    source.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(io.BytesIO(git('archive', '--format=zip', BASE))) as archive:
        archive.extractall(source)
    revision = git('rev-parse', 'HEAD').decode().strip()
    for name in FILES:
        text = git('show', 'HEAD:' + name).decode('utf-8').replace('\r\n', '\n')
        if name == 'CustomizationControls.h':
            text = baseline_controls(text)
        elif name == 'tests/customization_controls_save_tests.cpp':
            text = text.replace('CustomizationProfileStorage', 'CustomizationSave')
        (source / name).write_text(text, encoding='utf-8', newline='\n')
    # Assert that no stance, random-car, owned-save or native menu code changed.
    changed = set(FILES)
    with zipfile.ZipFile(io.BytesIO(git('archive', '--format=zip', BASE))) as baseline:
        for name in baseline.namelist():
            if not name.endswith('/') and name not in changed:
                if (source / name).read_bytes() != baseline.read(name):
                    raise RuntimeError('Non-camera baseline file changed: ' + name)
    cmake = cmake_path(None)
    build = source / 'build-customization'
    run([cmake, '-S', source, '-B', build, '-A', 'Win32'])
    run([cmake, '--build', build, '--config', 'Release', '--parallel', '4'])
    run([cmake.with_name('ctest.exe'), '--test-dir', build, '-C', 'Release', '--output-on-failure'])
    binary = build / 'Release/NFSMWUnlimiter.asi'
    target = destination / binary.name
    target.write_bytes(binary.read_bytes())
    manifest = dict(kind='camera-only-baseline-update', baseline=git('rev-parse', BASE).decode().strip(),
                    cameraSourceCommit=revision, runtimeQA=False,
                    sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                    preserved='Baseline stance, save lifecycle and companion binary; existing user INIs.')
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    with zipfile.ZipFile(destination / 'source.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in source.rglob('*'):
            if path.is_file() and build not in path.parents:
                archive.write(path, path.relative_to(source))
    print(destination)


if __name__ == '__main__':
    main()
