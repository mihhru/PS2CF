"""Build and verify binary/source distributions from this release tree."""
from pathlib import Path
import gzip
import hashlib
import io
import json
import shutil
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / 'dist'
PREFIX = 'PS2CF-1.0'
sys.path.insert(0, str(ROOT / 'tools/vendor'))
from mymcplusplus import ps2mc


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_files():
    roots = ['README.md', 'README_RU.md', 'BUILDING.md', 'BUILDING_RU.md', 'LICENSE',
             'THIRD_PARTY_NOTICES.md', 'CHANGES.md', 'VALIDATION.md', 'RELEASE_NOTES.md', '.gitignore', '.gitattributes', 'build.sh',
             'src', 'tools', 'images', 'deps']
    for base in roots:
        p = ROOT / base
        for f in ([p] if p.is_file() else sorted(p.rglob('*'))):
            rel = f.relative_to(ROOT)
            if not f.is_file() or any(n in ('.git', '__pycache__', 'obj', 'obj.gui', 'obj.game', 'asm') for n in rel.parts):
                continue
            if f.suffix.lower() in ('.o', '.d', '.a', '.elf', '.map', '.tmp'):
                continue
            if f.suffix.lower() == '.irx' and '__precompiled' not in rel.parts:
                continue
            if f.name.endswith('.ps2cf-original'):
                continue
            if f.name in ('build-imports.c', 'build-exports.c'):
                continue
            yield rel.as_posix(), f.read_bytes()


def write_zip(path, files):
    with zipfile.ZipFile(path, 'w') as archive:
        for name, data in files.items():
            info = zipfile.ZipInfo(name, (2026, 10, 14, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o100755 if name.endswith('.sh') else 0o100644) << 16
            archive.writestr(info, data)
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert {n: sha(archive.read(n)) for n in archive.namelist()} == {n: sha(b) for n, b in files.items()}


def main():
    DIST.mkdir(exist_ok=True)
    files = dict(source_files())
    assert files and not any('user-original' in n or 'release-tools' in n for n in files)
    for name, expected in json.loads(files['deps/manifest.json']).items():
        assert sha(files['deps/' + name]) == expected['sha256']
    sources = {PREFIX + '/' + name: data for name, data in files.items()}
    write_zip(DIST / (PREFIX + '-source.zip'), sources)
    with open(DIST / (PREFIX + '-source.tar.gz'), 'wb') as raw:
        with gzip.GzipFile(fileobj=raw, mode='wb', mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w') as archive:
                for name, data in sources.items():
                    info = tarfile.TarInfo(name)
                    info.size = len(data)
                    info.mode = 0o755 if name.endswith('.sh') else 0o644
                    info.mtime = 1791936000
                    archive.addfile(info, io.BytesIO(data))
    with tarfile.open(DIST / (PREFIX + '-source.tar.gz')) as archive:
        assert {m.name: sha(archive.extractfile(m).read()) for m in archive if m.isfile()} == {n: sha(b) for n, b in sources.items()}
    binary = {}
    for name, data in files.items():
        if name.startswith('images/') or ('/' not in name and name.endswith('.md')) or name == 'LICENSE':
            binary[name] = data
    for name in ('OPL_CF_v1.ELF', 'wLaunchELF_CF_v1.ELF'):
        binary['ELF/' + name] = (ROOT / 'output' / name).read_bytes()
    with zipfile.ZipFile(ROOT / 'output/FMCBInstaller.zip') as archive:
        for name in archive.namelist():
            if not name.endswith('/'):
                binary[name] = archive.read(name)
    image = (ROOT / 'output/PS2CF-1.0-SCPH10000-Gen2.mcd').read_bytes()
    assert len(image) == 8388608
    card = ps2mc.ps2mc(io.BytesIO(image))
    try:
        assert card.check()
        for path, name in [('/APPS/OPNPS2LD.ELF', 'OPL_CF_v1.ELF'), ('/BOOT/BOOT.ELF', 'wLaunchELF_CF_v1.ELF')]:
            f = card.open(path, 'rb')
            assert f.read() == binary['ELF/' + name]
            f.close()
        f = card.open('/SYS-CONF/FREEMCB.CNF', 'rb')
        config = f.read()
        f.close()
        assert b'\x07' not in config and b'OPL CF v1' in config and b'wLaunchELF CF v1' in config
        assert b'RC1' not in config
    finally:
        card.close()
    binary['Gen2/MemoryCards/PS2/BOOT/BootCard-1.mcd'] = image
    binary['Gen2/manifest.json'] = (ROOT / 'output/PS2CF-1.0-SCPH10000-Gen2.manifest.json').read_bytes()
    binary['FREEMCB.CNF'] = config
    for path in ('src/opl/LICENSE', 'src/opl/CREDITS', 'src/opl/modules/pccf/PS2SDK_LICENSE.txt',
                 'src/wlaunchelf/PS2SDK_LICENSE.txt', 'tools/vendor/LICENSE.mymcplusplus'):
        binary['licenses/' + path.replace('/', '_')] = files[path]
    binary['manifest.json'] = (json.dumps({'version': '1.0', 'source_files': len(files),
        'files': {n: sha(b) for n, b in sorted(binary.items())}}, indent=2) + '\n').encode()
    binary['SHA256SUMS'] = ''.join(sha(b) + '  ' + n + '\n' for n, b in sorted(binary.items())).encode()
    write_zip(DIST / (PREFIX + '-release.zip'), binary)
    sums = ''.join(sha(p.read_bytes()) + '  ' + p.name + '\n' for p in sorted(DIST.glob(PREFIX + '-*')) if p.is_file())
    (DIST / 'SHA256SUMS').write_text(sums)
    print('PASS source ZIP/tar.gz file equality:', len(files), 'files')
    print('PASS binary package, Gen2 filesystem, v1 menu and embedded ELF identity')
    for p in sorted(DIST.iterdir()):
        if p.is_file():
            print(p.name, p.stat().st_size)


if __name__ == '__main__':
    main()
