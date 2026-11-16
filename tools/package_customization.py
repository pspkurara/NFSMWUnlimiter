"""Build, test and package a development snapshot without touching a game install."""
from pathlib import Path
from datetime import datetime, timezone, timedelta
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def run(args, cwd=None):
    # Windows/MSBuild rejects duplicate PATH/Path entries from some host runtimes.
    env = {key.upper(): value for key, value in os.environ.items()}
    env['VSLANG'] = '1033'
    subprocess.run([str(arg) for arg in args], cwd=cwd, env=env, check=True)

def git_text(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()

def cmake_path(explicit):
    if explicit:
        return Path(explicit).resolve(strict=True)
    found = shutil.which('cmake')
    if found:
        return Path(found)
    installer = Path(os.environ.get('PROGRAMFILES(X86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installation = subprocess.check_output([str(installer), '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
    return (Path(installation) / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe').resolve(strict=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--companion', type=Path, default=ROOT.parent / 'NFSMW-CustomizationLab')
    parser.add_argument('--cmake')
    args = parser.parse_args()
    companion = args.companion.resolve(strict=True)
    for repo in (ROOT, companion):
        if git_text(repo, 'status', '--porcelain'):
            raise SystemExit(f'Commit the source changes before packaging: {repo.name}')
    cmake = cmake_path(args.cmake)
    ctest = cmake.with_name('ctest.exe' if os.name == 'nt' else 'ctest')
    for repo, build_name in ((ROOT, 'build-customization'), (companion, 'build')):
        build = repo / build_name
        run([cmake, '-S', repo, '-B', build, '-A', 'Win32'])
        run([cmake, '--build', build, '--config', 'Release'])
        run([ctest, '--test-dir', build, '-C', 'Release', '--output-on-failure'])
    revisions = {'unlimiter': git_text(ROOT, 'rev-parse', 'HEAD'), 'companion': git_text(companion, 'rev-parse', 'HEAD')}
    stamp = datetime.now(timezone(timedelta(hours=9))).strftime('%Y%m%d-%H%M%S')
    name = f'NFSMWCustomizationLab-development-{stamp}-{revisions["unlimiter"][:8]}'
    dist = ROOT / 'dist'
    stage = dist / name
    (stage / 'scripts').mkdir(parents=True, exist_ok=False)
    payload = [
        (ROOT / 'build-customization/Release/NFSMWUnlimiter.asi', 'NFSMWUnlimiter.asi'),
        (companion / 'build/Release/NFSMWCustomizationLab.asi', 'NFSMWCustomizationLab.asi'),
        (ROOT / 'NFSMWCustomizationCamera.ini', 'NFSMWCustomizationCamera.ini'),
        (ROOT / 'NFSMWCustomizationClip.ini', 'NFSMWCustomizationClip.ini'),
        (companion / 'NFSMWCustomizationLab.ini', 'NFSMWCustomizationLab.ini'),
    ]
    for source, target in payload:
        shutil.copy2(source, stage / 'scripts' / target)
    shutil.copy2(ROOT / 'distribution/README.txt', stage / 'README.txt')
    shutil.copy2(ROOT / 'tools/install_customization.ps1', stage / 'install_customization.ps1')
    shutil.copytree(ROOT / 'docs', stage / 'docs')
    for file in ('CustomizationCamera.md', 'CustomizationClip.md', 'CustomizationControls.md'):
        shutil.copy2(ROOT / file, stage / 'docs' / file)
    shutil.copy2(companion / 'README.md', stage / 'docs/Companion.md')
    shutil.copy2(ROOT / 'vendor/minhook/LICENSE.txt', stage / 'MinHook-LICENSE.txt')
    shutil.copy2(companion / 'LICENSE', stage / 'Companion-LICENSE.txt')
    for repo, label in ((ROOT, 'unlimiter'), (companion, 'companion')):
        run(['git', '-C', repo, 'archive', '--format=zip', f'--output={stage / (label + "-source.zip")}', 'HEAD'])
    files = {p.relative_to(stage).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in sorted(stage.rglob('*')) if p.is_file()}
    manifest = {'schema': 1, 'kind': 'unfinished-development-snapshot', 'sourceCommits': revisions,
                'nativeRuntimeQA': False, 'activeKeyConfigIntegration': True,
                'activeKeyConfigRuntimeQA': False,
                'inspectedExecutableSHA256': '0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342', 'files': files}
    (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    archive = dist / (name + '.zip')
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as out:
        for file in sorted(stage.rglob('*')):
            if file.is_file(): out.write(file, name + '/' + file.relative_to(stage).as_posix())
    with zipfile.ZipFile(archive) as check:
        for path, digest in files.items():
            if hashlib.sha256(check.read(name + '/' + path)).hexdigest() != digest:
                raise RuntimeError('Archive verification failed: ' + path)
    print('Verified development snapshot:', archive)

if __name__ == '__main__':
    main()
