"""Verify the linked ELF modules, not merely source text."""
from pathlib import Path
import runpy
import struct

root = Path(__file__).resolve().parents[1]
runpy.run_path(str(root / 'src/opl/tests/cf/check_artifacts.py'))
wle = root / 'src/wlaunchelf'
ee = (wle / 'UNC-BOOT-CF_EXFAT_RW-EXFAT-NO_NETWORK.ELF').read_bytes()
for name in ('pccf', 'cf_bdm', 'cf_bdmfs'):
    blob = (wle / 'iop' / (name + '.irx')).read_bytes()
    assert ee.count(blob) == 1, name
    print('PASS embedded wLaunchELF IRX:', name, len(blob))
for name in ('pccf_ata.c', 'pccf_ata.h', 'pccf_hw.c', 'pccf_pio_bulk.inc', 'pccf_pio_profile.inc'):
    assert (wle / 'iop/pccf' / name).read_bytes() == (root / 'src/opl/modules/pccf' / name).read_bytes(), name
for name in ('OPL_CF_v1.ELF', 'wLaunchELF_CF_v1.ELF'):
    path = root / 'output' / name
    data = path.read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01', path.name
    phoff = struct.unpack_from('<I', data, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    entry = struct.unpack_from('<I', data, 24)[0]
    loads = [struct.unpack_from('<8I', data, phoff + n * phsize) for n in range(phnum)]
    assert any(t == 1 and va <= entry < va + mem for t, off, va, pa, size, mem, flags, align in loads)
    for t, off, va, pa, size, mem, flags, align in loads:
        if t == 1:
            assert size <= mem and off + size <= len(data) and va + mem <= 32 * 1024 * 1024
print('PASS packed ELF segments and common CF hardware sources')
