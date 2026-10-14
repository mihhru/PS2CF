"""Check the actual linked ELF's embedded IRX payloads and BDM ABI."""
from pathlib import Path
import struct

root=Path(__file__).resolve().parents[2]
def sections(data):
    assert data[:6]==b'\x7fELF\x01\x01'
    off=struct.unpack_from('<I',data,32)[0]
    size,num=struct.unpack_from('<HH',data,46)
    return [struct.unpack_from('<10I',data,off+i*size) for i in range(num)]
elf=(root/'opl.elf').read_bytes()
sh=sections(elf)
symbols={}
for sec in sh:
    if sec[1]!=2: continue
    strings=sh[sec[6]]
    names=elf[strings[4]:strings[4]+strings[5]]
    for off in range(sec[4],sec[4]+sec[5],sec[9]):
        name,value,size,info,other,index=struct.unpack_from('<IIIBBH',elf,off)
        label=names[name:].split(b'\0',1)[0].decode()
        if index and index<len(sh): symbols[label]=(sh[index][4]+value-sh[index][3],size)
for label,path in [('eecore_elf','ee_core/ee_core.elf'),('pccf_bd_irx','modules/pccf/pccf_bd.irx'),('pccf_game_irx','modules/pccf/pccf_game.irx'),('pccf_cdvdman_irx','modules/iopcore/cdvdman/pccf_cdvdman.irx')]:
    blob=(root/path).read_bytes(); off,_=symbols[label]
    size_off,_=symbols['size_'+label]
    assert struct.unpack_from('<I',elf,size_off)[0]==len(blob)
    assert elf[off:off+len(blob)]==blob
    print('PASS embedded IRX:',label,len(blob))
game=(root/'modules/pccf/pccf_game.irx').read_bytes()
imports={}
for off in range(0,len(game)-20,4):
    if game[off:off+4]==struct.pack('<I',0x41e00000):
        imports[game[off+12:off+20].split(b'\0')[0].decode()]=struct.unpack_from('<H',game,off+8)[0]
assert imports['bdm']==0x101
assert set(imports)=={'bdm','sysclib','thbase','thsemap'},imports
assert b'BDM_CF_MODE\0' in (root/'ee_core/ee_core.elf').read_bytes()
print('PASS game IRX imports (no iomanX/FatFs/USB/DEV9/logger):',imports)
packed=(root.parents[1]/'output/OPL_CF_v1.ELF').read_bytes()
assert packed[:6]==b'\x7fELF\x01\x01' and len(packed)>1000000
phoff=struct.unpack_from('<I',packed,28)[0]
phsize,phnum=struct.unpack_from('<HH',packed,42)
entry=struct.unpack_from('<I',packed,24)[0]
valid=False
for i in range(phnum):
    typ,off,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',packed,phoff+i*phsize)
    if typ==1:
        assert off+filesz<=len(packed) and filesz<=memsz and va+memsz<=32*1024*1024
        valid |= va<=entry<va+memsz
assert valid
print('PASS packed EE ELF: valid load segments and entry within 32 MiB')


for marker in (b'pccfctl', b'OPL_CF_V5.LOG', b'cfLaunchInfo', b'cfLogSaved', b'cfSaveLog', b'cfStage', b'log_text', b'log_used', b'ctl_devctl'):
    assert marker not in elf, marker
menu=(root/'modules/pccf/pccf_bd.irx').read_bytes()
assert b'iomanX' not in menu and b'pccfctl' not in menu
print('PASS no CF logger: no buffers, diagnostic device, EE log writer or iomanX import')
