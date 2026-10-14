# PS2CF — Release 1.0

CompactFlash and MicroSD-through-CF support for early Japanese PlayStation 2 consoles
with a PC CARD slot. Tested on SCPH-10000; SCPH-15000/18000 are target models awaiting
separate hardware validation.

- **PS2CF-1.0-release.zip**: OPL CF v1, wLaunchELF CF v1, a customized FMCB 1.966
  installer, and the ready-to-copy SCPH-10000 PSxMemCard Gen2 image.
- **PS2CF-1.0-source.zip / .tar.gz**: matching source trees with portable build scripts,
  dependency provenance and English/Russian build instructions.
- FAT32/exFAT read/write, single ISO files over 4 GiB on exFAT, validated faster PIO reads.
- OPL software VMC on CF is not supported; saves to normal cards and Gen2 work separately.
- See README and VALIDATION.md for hardware observations, measurements and limitations.
- Verify downloads using **SHA256SUMS**. Automatic GitHub source archives are also
  available; the explicitly named source archives are the validated packaged copies.

---

Поддержка CompactFlash и MicroSD через CF для ранних японских PS2 со слотом PC CARD.
Проверено на SCPH-10000; SCPH-15000/18000 — целевые модели без отдельного аппаратного прогона.

- **PS2CF-1.0-release.zip**: OPL CF v1, wLaunchELF CF v1, подготовленный установщик
  FMCB 1.966 и готовый образ PSxMemCard Gen2 для SCPH-10000.
- **PS2CF-1.0-source.zip / .tar.gz**: одинаковые исходники, переносимые сценарии сборки,
  происхождение зависимостей и инструкция на двух языках.
- FAT32/exFAT, цельные ISO больше 4 ГиБ на exFAT, ускоренное проверяемое PIO-чтение.
- VMC OPL на CF отключены; обычные карты памяти и Gen2 сохраняют игры независимо от них.
- Замеры, аппаратные наблюдения и ограничения описаны в README и VALIDATION.md.
- Контрольные суммы загрузок — в **SHA256SUMS**.
