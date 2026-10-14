"""Format a new Gen2 filesystem; install distribution files and reference binding bytes.

No filesystem, allocation tables, applications or configuration are cloned from a dump.
The public SD2PSX reference supplies only the 32-byte card binding of each KELF.
"""
from pathlib import Path
import hashlib
import json
import runpy
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output'
sys.path.insert(0, str(ROOT / 'tools/vendor'))
from mymcplusplus import ps2mc

REFERENCE_URL = 'https://github.com/sd2psXtd/sd2psXtd.github.io/releases/download/latest/FMCB-1966.mcd'
REFERENCE = ROOT / 'deps/FMCB1966.sd2psxtd-reference.mcd'
ARCHIVE = ROOT / 'deps/FMCB-1966.7z'
NAME = 'PS2CF-1.0-SCPH10000-Gen2'


def sha(b):
    return hashlib.sha256(b).hexdigest()


def inventory(mc):
    result = {}
    def walk(path):
        for entry in mc.dir_open(path):
            name = entry[8].decode('ascii')
            if name in ('.', '..') or not entry[0] & 0x8000:
                continue
            child = path.rstrip('/') + '/' + name
            if ps2mc.mode_is_dir(entry[0]):
                walk(child)
            else:
                f = mc.open(child, 'rb')
                result[child] = sha(f.read())
                f.close()
    walk('/')
    return result


def main():
    configure = runpy.run_path(str(ROOT / 'tools/build-fmcb-installer.py'))['configure']
    reference = ps2mc.ps2mc(open(REFERENCE, 'rb'))
    binding_report = {}
    try:
        with tempfile.TemporaryDirectory(prefix='fmcb-gen2-', dir=OUT) as temp:
            temp = Path(temp)
            sevenzip = shutil.which('7z') or shutil.which('7zz') or r'C:\Program Files\7-Zip\7z.exe'
            subprocess.run([sevenzip, 'x', str(ARCHIVE), '-o' + str(temp / 'dist'), '-y'],
                           check=True, stdout=subprocess.DEVNULL)
            dist, = (temp / 'dist').iterdir()
            install = dist / 'INSTALL'
            files = {}
            for folder in ('APPS', 'BOOT', 'SYS-CONF'):
                for p in (install / folder).iterdir():
                    if p.is_file() and (p.suffix.lower() in ('.icn', '.sys', '.irx')
                                        or p.name == 'FMCB_CFG.ELF'):
                        files['/' + folder + '/' + p.name] = p.read_bytes()
            files['/APPS/OPNPS2LD.ELF'] = (OUT / 'OPL_CF_v1.ELF').read_bytes()
            files['/BOOT/BOOT.ELF'] = (OUT / 'wLaunchELF_CF_v1.ELF').read_bytes()
            files['/SYS-CONF/FREEMCB.CNF'] = configure((install / 'SYS-CONF/FREEMCB.CNF').read_bytes())
            files['/SYS-CONF/endvdpl.irx'] = (install / 'SYSTEM/ENDVDPL.XRX').read_bytes()
            # Both SCPH-10000 ROMs, plus the entry used after the kernel redirect.
            for source_name, targets in (
                ('FMCB.XLF', ('osdmain.elf', 'osd130.elf')),
                ('OSDSYS.XLF', ('osdsys.elf',)),
                ('OSD110.XLF', ('osd110.elf',)),
            ):
                original = (install / 'SYSTEM' / source_name).read_bytes()
                f = reference.open('/BIEXEC-SYSTEM/' + targets[0], 'rb')
                signed = f.read()
                f.close()
                # Reject a different payload/header. Borrow card binding only.
                assert len(original) == len(signed)
                assert original[:40] == signed[:40] and original[72:] == signed[72:], source_name
                bound = original[:40] + signed[40:72] + original[72:]
                assert bound == signed
                binding_report[source_name] = dict(offset=40, length=32,
                    source_payload_sha256=sha(original), result_sha256=sha(bound))
                for name in targets:
                    files['/BIEXEC-SYSTEM/' + name] = bound
            for source_name, target in (
                ('DEV9.IRX', 'dev9.irx'), ('ATAD.IRX', 'atad.irx'),
                ('HDDLOAD.IRX', 'hddload.irx'), ('FMCB.ICN', 'FMCB.icn'),
                ('BIICON.SYS', 'icon.sys'),
            ):
                files['/BIEXEC-SYSTEM/' + target] = (install / 'SYSTEM' / source_name).read_bytes()
            image = temp / (NAME + '.mcd')
            card = ps2mc.ps2mc(open(image, 'w+b'), params=(False, 512, 16, 16384))
            for folder in ('APPS', 'BOOT', 'SYS-CONF', 'BIEXEC-SYSTEM'):
                card.mkdir('/' + folder)
            for path, data in sorted(files.items()):
                f = card.open(path, 'wb')
                f.write(data)
                f.close()
                entry = list(card.get_dirent(path))
                entry[0] = 0x8497
                card.set_dirent(path, entry)
            entry = list(card.get_dirent('/BIEXEC-SYSTEM'))
            entry[0] = 0x842f
            card.set_dirent('/BIEXEC-SYSTEM', entry)
            card.close()
            card = ps2mc.ps2mc(open(image, 'rb'))
            try:
                assert card.spare_size == 0 and card.raw_page_size == 512
                assert card.check()
                actual = inventory(card)
                assert actual == {p: sha(b) for p, b in files.items()}
                free = card.get_free_space()
            finally:
                card.close()
            assert image.stat().st_size == 8388608
            report = dict(fmcb='1.966', target='PSxMemCard Gen2 / compatible SD2PSX retail binding',
                console='SCPH-10000, ROM 1.00J and 1.01J', filesystem_formatted_from_empty=True,
                cloned_image=False, user_card_used=False, source_archive_sha256=sha(ARCHIVE.read_bytes()),
                binding_reference_url=REFERENCE_URL, binding_reference_sha256=sha(REFERENCE.read_bytes()),
                binding_bytes_per_kelf=binding_report, image_sha256=sha(image.read_bytes()),
                bytes=image.stat().st_size, free_bytes=free, files=actual,
                hardware_validation='pending; filesystem, contents and binding/payload identity verified on PC',
                physical_memory_card='Use FMCB installer; this Gen2 binding is not universal for physical cards')
            final = OUT / image.name
            shutil.copyfile(image, final)
            manifest = json.dumps(report, indent=2) + '\n'
            (OUT / (NAME + '.manifest.json')).write_text(manifest, encoding='utf-8')
            final.with_suffix('.mcd.sha256').write_text(report['image_sha256'] + '  ' + final.name + '\n')
            archive = OUT / (NAME + '.zip')
            with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
                z.write(final, 'MemoryCards/PS2/BOOT/BootCard-1.mcd')
                z.writestr('manifest.json', manifest)
                z.write(ROOT / 'README_RU.md', 'README_RU.md')
            with zipfile.ZipFile(archive) as z:
                assert z.testzip() is None
                assert sha(z.read('MemoryCards/PS2/BOOT/BootCard-1.mcd')) == report['image_sha256']
            archive.with_suffix('.zip.sha256').write_text(sha(archive.read_bytes()) + '  ' + archive.name + '\n')
            print(final)
            print('PASS freshly formatted filesystem; all', len(files), 'files verified; free bytes:', free)
            print('PASS all KELF payloads equal distribution; only 32-byte Gen2 binding replaced')
    finally:
        reference.close()


if __name__ == '__main__':
    main()
