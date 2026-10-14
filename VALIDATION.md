# Release validation

Validated on 2026-10-14:

- Built OPL CF v1 and wLaunchELF CF v1 using the separate modern/legacy toolchains.
- Verified exact embedded IOP module bytes, game BDM ABI and packed ELF entry/segments.
- Confirmed the release has no removed CF log buffers, log writer or diagnostic device.
- CF host tests passed under ASan/UBSan: block request splitting and bounds,
  read profiles/fallback, ATA IDENTIFY, LBA48, read/write, flush/read-back and failures.
- Real MMIO routine tests passed, including LBA48 register order and conservative writes.
- Mode-dispatch and build-header dependency regressions passed.
- FatFs/exFAT test passed through the real ATA read/write routines on a modeled
  high-address volume with a file larger than 5 GiB, remount, rename and deletion.
- Verified freshly formatted Gen2 filesystem and exact ELF identity between the
  standalone files, FMCB installer and image. Menu uses v1 names and ASCII cursors.
- Source ZIP and tar.gz are checked for identical file hashes by the packager.

Hardware game observations and speed measurements came from the preceding RC1/PIO
diagnostic builds, as identified in the README. The v1 naming rebuild does not change
CF transfer logic or timing. No new claim of a full hardware game sweep of the renamed
binaries is made. The new source archive is also rebuilt in a separate extraction
directory before publishing.
