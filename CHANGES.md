# PS2CF 1.0

- Integrate PC CARD CompactFlash into the OPL BDM menu and in-game CD/DVD backend.
- Support real CF and compatible SD-to-CF bridges, including the tested INIC-2051.
- Validate accelerated PIO read profiles against conservative reads; retain fallback.
- Support ATA LBA48 and check request bounds before narrowing addresses.
- Add conservative menu/file-manager writes, cache flush and sector read-back verification.
- Expose FAT32/exFAT read/write as `cf:/` in wLaunchELF using modern BDM/FatFs.
- Keep in-game CF writes and OPL CF VMC disabled.
- Disable competing network/HDD paths in the dedicated CF session.
- Remove CF diagnostic log collection from the release build.
- Enable OPL Write Operations by default, retaining saved settings precedence.
- Ship a minimal FMCB installer and a freshly formatted SCPH-10000 Gen2 image.
- Use plain ASCII FMCB cursors/header for the early console OSD.
- Publish bilingual documentation, original equipment photographs, source archives,
  portable build scripts and artifact checks.
- Replace the legacy vmc_fs module's build-time-only log timestamp with a fixed
  release label so separate source builds produce identical release ELF files.

This release promotes the tested CF RC1 behavior to v1 naming. Game compatibility
still depends on the title, settings and hardware. Hardware test observations are
described in the README; host test passes are not presented as console test results.
