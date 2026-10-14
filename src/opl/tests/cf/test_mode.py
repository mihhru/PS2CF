"""Regression against real EE mode dispatch and string helpers, compiled on host."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
util=(root/'ee_core/src/util.c').read_text()
helpers='\n'.join(re.search(r'int '+name+r'\(.*?\n}',util,re.S).group() for name in ['_strncmp','_strcmp'])
enum=re.search(r'enum GAME_MODE \{.*?\};',(root/'ee_core/include/ee_core.h').read_text(),re.S).group()
s=(root/'ee_core/src/main.c').read_text()
start=s.index('    if (!_');end=s.index('    DPRINTF("Game Mode',start)
dispatch=s[start:end]
test=r'''
int main(void) {
 struct Config c; struct Config *config=&c;
 const char *names[]={"BDM_CF_MODE","BDM_USB_MODE","BDM_ILK_MODE","BDM_M4S_MODE","BDM_ATA_MODE","ETH_MODE","HDD_MODE","BDM_CF_MODE_X","BDM_CF"};
 int modes[]={BDM_CF_MODE,BDM_USB_MODE,BDM_ILK_MODE,BDM_M4S_MODE,BDM_HDD_MODE,ETH_MODE,HDD_MODE,-1,-1};
 /* Demonstrate the v2 failure with the actual OPL helper. */
 assert(_strncmp("BDM_CF_MODE","BDM_CF_MODE",12)!=0);
 for(unsigned i=0;i<sizeof(modes)/sizeof(*modes);i++) {
  strcpy(c.GameModeDesc,names[i]); c.GameMode=-1;
  DISPATCH
  assert(c.GameMode==modes[i]);
 }
 return 0;
}
'''.replace('DISPATCH',dispatch)
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'mode.c';exe=Path(d)/'mode'
 src.write_text('#include <assert.h>\n#include <string.h>\n'+enum+'\nstruct Config { char GameModeDesc[32]; int GameMode; };\n'+helpers+test)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: reproduced v2 CF mode mismatch; v3 selects CF and all existing modes, rejects non-exact CF strings')
