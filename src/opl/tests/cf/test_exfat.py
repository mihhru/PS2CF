from pathlib import Path
import re,subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
up=root/'tests/cf/fatfs-upstream/source'
with tempfile.TemporaryDirectory() as d:
 d=Path(d)
 for p in (up/'include').glob('*.h'): shutil.copy(p,d/p.name)
 p=d/'ffconf.h';s=p.read_text()
 for key,val in {'FF_USE_MKFS':1,'FF_FS_NORTC':1,'FF_FS_REENTRANT':0,'FF_VOLUMES':1,'FF_STR_VOLUME_ID':0}.items():
  s=re.sub(r'(#define\s+'+key+r'\s+)\d+',lambda m:m[1]+str(val),s)
 p.write_text(s)
 (d/'module_debug.h').write_text('typedef unsigned long long u64;\n#define M_DEBUG(...) ((void)0)\n#define DEBUG_U64_2XU32(x) ((void)(x))\n')
 exe=d/'exfat'
 subprocess.run(['cc','-std=c99','-O1','-g','-fsanitize=address,undefined','-I'+str(d),str(root/'tests/cf/test_exfat.c'),str(up/'ff.c'),str(up/'ffunicode.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
