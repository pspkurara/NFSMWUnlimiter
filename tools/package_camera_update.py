"""Rebuild the installed baseline with only the three reviewed camera changes.

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
CHANGES = ('ed59cb5', '7d19940', '9ff541d')
FILES = (
    'CustomizationCamera.h', 'CustomizationControlState.h', 'CustomizationControls.h',
    'CustomizationOrbitMath.h', 'CustomizationCamera.md', 'CustomizationControls.md',
    'NFSMWCustomizationCamera.ini', 'tests/customization_control_state_tests.cpp',
    'tests/customization_controls_save_tests.cpp',
)


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    destination = ROOT / 'dist' / ('camera-update-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
    source = destination / 'source'
    source.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(io.BytesIO(git('archive', '--format=zip', BASE))) as archive:
        archive.extractall(source)
    for name in FILES:
        path = source / name
        path.write_bytes(path.read_bytes().replace(b'\r\n', b'\n'))
    commits = []
    patch_env = {key.upper(): value for key, value in os.environ.items()}
    # The export is nested below the real repository. Prevent Git from treating
    # this directory as a path prefix and silently skipping every patch.
    patch_env['GIT_CEILING_DIRECTORIES'] = str(source.parent)
    for revision in CHANGES:
        commits.append(git('rev-parse', revision).decode().strip())
        patch = git('diff', revision + '^', revision, '--', *FILES)
        # The camera commit was made after storage extraction. Translate only
        # its patch context back to the installed baseline's equivalent helpers.
        patch = patch.replace(b'CustomizationProfileStorage::', b'CustomizationSave::')
        subprocess.run(['git', 'apply', '--check', '-'], input=patch, cwd=source, env=patch_env, check=True)
        subprocess.run(['git', 'apply', '-'], input=patch, cwd=source, env=patch_env, check=True)
    for name in ('CustomizationControlState.h', 'CustomizationOrbitMath.h'):
        if (source / name).read_bytes() != git('show', CHANGES[-1] + ':' + name).replace(b'\r\n', b'\n'):
            raise RuntimeError('Camera patch export mismatch: ' + name)
    cmake = cmake_path(None)
    build = source / 'build-customization'
    run([cmake, '-S', source, '-B', build, '-A', 'Win32'])
    run([cmake, '--build', build, '--config', 'Release', '--parallel', '4'])
    run([cmake.with_name('ctest.exe'), '--test-dir', build, '-C', 'Release', '--output-on-failure'])
    binary = build / 'Release/NFSMWUnlimiter.asi'
    target = destination / binary.name
    target.write_bytes(binary.read_bytes())
    manifest = dict(kind='camera-only-baseline-update', baseline=git('rev-parse', BASE).decode().strip(),
                    cameraCommits=commits, runtimeQA=False,
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
