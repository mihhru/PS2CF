"""Prepare a clean FMCB installation from the distribution, never from a card dump."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'deps/FMCB-1966.7z'
OUTPUT = ROOT / 'output'
NAME = 'FMCBInstaller'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def configure(data):
    text = data.decode('ascii').replace('\r', '')
    # Browser and System Configuration are native OSDSYS entries, not ELF apps.
    text = re.sub(r'^(?:name|path[123])_OSDSYS_ITEM_\d+\s*=.*\n?', '', text, flags=re.M)
    text = re.sub(r'^LK_.*\n?', '', text, flags=re.M)
    text = re.sub(r'^ESR_Path_.*\n?', '', text, flags=re.M)
    text = re.sub(r'^FastBoot\s*=.*$', 'FastBoot = 0', text, flags=re.M)
    # SCPH-10000 ROM OSD displays later OSD's escape commands literally.
    plain_text = {
        'OSDSYS_left_cursor': '>',
        'OSDSYS_right_cursor': '<',
        'OSDSYS_menu_top_delimiter': 'Free McBoot %VER%',
        'OSDSYS_menu_bottom_delimiter': 'Up/Down: select item',
    }
    for key, value in plain_text.items():
        text, count = re.subn(r'^' + key + r'\s*=.*$', key + ' = ' + value, text, flags=re.M)
        assert count == 1, key
    assert '\x07' not in text
    text += '\nLK_Auto_E1 = OSDSYS\nLK_R1_E1 = mc?:/BOOT/BOOT.ELF\n'
    text += 'LK_Start_E1 = mc?:/SYS-CONF/FMCB_CFG.ELF\n'
    items = [
        ('OPL CF v1', 'mc?:/APPS/OPNPS2LD.ELF'),
        ('wLaunchELF CF v1', 'mc?:/BOOT/BOOT.ELF'),
        ('Launch Disc', 'FASTBOOT'),
        ('Free MCBoot Configurator', 'mc?:/SYS-CONF/FMCB_CFG.ELF'),
        ('Restart System', 'OSDSYS'),
        ('Shutdown System', 'POWEROFF'),
    ]
    for i, (label, target) in enumerate(items, 1):
        text += f'name_OSDSYS_ITEM_{i} = {label}\npath1_OSDSYS_ITEM_{i} = {target}\n'
    return text.replace('\n', '\r\n').encode('ascii')


def main():
    sevenzip = shutil.which('7z') or shutil.which('7zz') or r'C:\Program Files\7-Zip\7z.exe'
    opl = OUTPUT / 'OPL_CF_v1.ELF'
    wle = OUTPUT / 'wLaunchELF_CF_v1.ELF'
    assert 'gEnableWrite = 1;' in (ROOT / 'src/opl/src/opl.c').read_text()
    with tempfile.TemporaryDirectory(prefix='fmcb-clean-', dir=OUTPUT) as work:
        work = Path(work)
        subprocess.run([sevenzip, 'x', str(ARCHIVE), '-o' + str(work / 'source'), '-y'],
                       check=True, stdout=subprocess.DEVNULL)
        source, = (work / 'source').iterdir()
        stage = work / NAME
        # Preserve the installer's resources. Replace customizable application dirs.
        def ignore(directory, names):
            if Path(directory).name in ('APPS', 'BOOT', 'APPS-HDD', 'BOOT-HDD'):
                return names
            return []
        shutil.copytree(source, stage, ignore=ignore)
        for folder in ('APPS', 'BOOT'):
            for icon in (source / 'INSTALL' / folder).iterdir():
                if icon.suffix.lower() in ('.icn', '.sys'):
                    shutil.copy2(icon, stage / 'INSTALL' / folder / icon.name)
        shutil.copy2(opl, stage / 'INSTALL/APPS/OPNPS2LD.ELF')
        shutil.copy2(wle, stage / 'INSTALL/BOOT/BOOT.ELF')
        config = configure((source / 'INSTALL/SYS-CONF/FREEMCB.CNF').read_bytes())
        (stage / 'INSTALL/SYS-CONF/FREEMCB.CNF').write_bytes(config)
        (OUTPUT / 'FREEMCB.CNF').write_bytes(config)
        readme = (ROOT / 'README_RU.md').read_bytes()
        (stage / 'README_CF_RC1_RU.md').write_bytes(readme)
        files = {p.relative_to(stage).as_posix(): sha(p.read_bytes())
                 for p in sorted(stage.rglob('*')) if p.is_file()}
        apps = sorted(p.relative_to(stage).as_posix()
                      for folder in ('APPS', 'BOOT', 'APPS-HDD', 'BOOT-HDD')
                      for p in (stage / 'INSTALL' / folder).rglob('*')
                      if p.is_file() and p.suffix.lower() == '.elf')
        assert apps == ['INSTALL/APPS/OPNPS2LD.ELF', 'INSTALL/BOOT/BOOT.ELF'], apps
        assert len(re.findall(rb'^name_OSDSYS_ITEM_', config, re.M)) == 6
        assert not re.search(rb'^\w+\s*=.*mass:', config, re.M)
        report = dict(source_archive=ARCHIVE.name, source_sha256=sha(ARCHIVE.read_bytes()),
                      based_on_card_image=False, fmcb='1.966', write_operations_default=True,
                      status='physical_card_installer_ready; Gen2_image_built_separately',
                      applications=apps, files=files)
        manifest = json.dumps(report, indent=2) + '\n'
        (stage / 'manifest.json').write_text(manifest, encoding='utf-8')
        output = OUTPUT / (NAME + '.zip')
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
            for p in sorted(stage.rglob('*')):
                if p.is_file():
                    entry = zipfile.ZipInfo(p.relative_to(work).as_posix(), (2026, 10, 14, 0, 0, 0))
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    z.writestr(entry, p.read_bytes())
        with zipfile.ZipFile(output) as z:
            assert z.testzip() is None
            for path, digest in files.items():
                assert sha(z.read(NAME + '/' + path)) == digest, path
        (OUTPUT / (NAME + '.manifest.json')).write_text(manifest, encoding='utf-8')
        for p in (output, opl):
            p.with_suffix(p.suffix + '.sha256').write_text(sha(p.read_bytes()) + '  ' + p.name + '\n')
        print(json.dumps({k: v for k, v in report.items() if k != 'files'}, indent=2))
        print(output)


if __name__ == '__main__':
    main()
