# Component origins and notices

PS2CF 1.0 preserves the source notices of the projects listed below. Changes are
identified by the PS2CF release and documented in [CHANGES.md](CHANGES.md).

| Component | Source | Included material / notice |
| --- | --- | --- |
| Open PS2 Loader | https://github.com/ps2homebrew/Open-PS2-Loader | `src/opl`, based on the supplied upstream archive; AFL 3.0 in `src/opl/LICENSE`, upstream CREDITS retained |
| wLaunchELF ISR | https://github.com/israpps/wLaunchELF_ISR | `src/wlaunchelf`, source headers and `PS2SDK_LICENSE.txt` retained; upstream precompiled IRX resources retained where needed |
| PS2SDK | https://github.com/ps2dev/ps2sdk | CF hardware reset/power sequence derives from DEV9 code by Marcus R. Brown; AFL 2.0 retained beside the driver. BDM/filesystem sources were taken at commit `329e2bb43287e16af16c4dde1c614e2662c24468` |
| FatFs | https://github.com/fjtrujy/FatFs/tree/iop-r0.16 | Commit `18cc3d9e07473a6aa3d783a66224b243a7b4974c`; ChaN notices retained in source |
| mymc++ | https://github.com/Adubbz/mymcplusplus | Python library in `tools/vendor/mymcplusplus`; GPLv3 in `tools/vendor/LICENSE.mymcplusplus`; derived from mymc+ and mymc as credited in its source |
| FMCB installer/distribution | https://github.com/israpps/FreeMcBoot-Installer | Unmodified public `FMCB-1966.7z` in `deps`; upstream installer, documentation and system files. Installer source is available upstream; full FMCB core source is not provided |
| SD2PSX reference | https://github.com/sd2psXtd/sd2psXtd.github.io/releases/download/latest/FMCB-1966.mcd | Unmodified public reference in `deps`; used only to supply the 32-byte KELF binding fields after payload comparison, never as a cloned user filesystem |
| gsKit / PS2-Packer / toolchains | https://github.com/ps2dev | External build dependencies; obtained through the official PS2DEV releases |

The local archive origins are recorded in `deps/manifest.json`. The public source
tree contains no console BIOS, game images, game saves, private user card dumps,
credentials, or SDK installation directories.

Public source for the binaries and PS2CF modifications is distributed alongside
Release 1.0 in both ZIP and tar.gz form and in this repository. The components are
independent works with their own notices; do not replace them with one blanket license.
