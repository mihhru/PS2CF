"""Check production Makefile schedules every CF C unit after a shared header changes."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
module=root/'modules/pccf'
for game in (0,1):
 for header in ('pccf_ata.h','pccf_pio_profile.inc','pccf_pio_bulk.inc'):
  text=subprocess.check_output(['make','-n','-W',header,f'GAME_DRIVER={game}','all'],cwd=module,text=True)
  for source in ('main.c','pccf_hw.c','pccf_ata.c','pcdiag_cis.c','pcdiag_cf.c'):
   assert any(' -c '+source+' ' in line for line in text.splitlines()),(game,header,source,text)
print('PASS build dependencies: header and both inline helpers rebuild all CF units in GUI and game variants')
