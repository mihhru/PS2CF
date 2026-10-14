# Building PS2CF 1.0

[English](BUILDING.md) | [Русский](BUILDING_RU.md)

Use Linux x86-64 or WSL. The release was checked with Ubuntu 26.04/WSL.
Download either source archive, extract it, and work inside `PS2CF-1.0/`.
The ZIP and tar.gz contain the same files; use `bash build.sh` so ZIP executable-bit
handling does not matter.

## Toolchains

Two generations of the PS2 toolchain are required. Mixing their headers and libraries
causes incompatible objects. `tools/toolchains.sh` switches the full environment.

| Variable | Default directory | Purpose |
| --- | --- | --- |
| `PS2CF_MODERN` | `$HOME/ps2dev-v2.0.0` | PS2DEV 2.0.0, OPL and current CF/BDM/FatFs IOP modules |
| `PS2CF_LEGACY_SDK` | `$HOME/ps2dev-v1.0` | PS2DEV 1.0 SDK, ports and gsKit for wLaunchELF EE |
| `PS2CF_LEGACY_TOOLS` | `$HOME/ps2dev-v1.1` | PS2DEV 1.1 legacy EE/IOP compilers and SDK utilities |

Host packages (Debian/Ubuntu):

```sh
sudo apt-get update
sudo apt-get install build-essential git python3 python3-yaml curl xz-utils unzip p7zip-full skopeo
bash tools/setup-toolchains.sh
```

The setup script downloads the official pinned PS2DEV 2.0.0 and 1.1 Linux releases,
extracts the 1.0 SDK/ports/gsKit from `docker.io/ps2dev/ps2dev:v1.0` using skopeo
(Docker is not required), and adjusts legacy GCC specs to match the 1.0 SDK.
It does not overwrite an existing SDK directory. Existing installations must have
the same compatible libraries and compiler-specs adjustment.

Toolchain origins:

- [PS2DEV 2.0.0](https://github.com/ps2dev/ps2dev/releases/tag/v2.0.0)
- [PS2DEV 1.1](https://github.com/ps2dev/ps2dev/releases/tag/v1.1)
- [PS2DEV Docker](https://github.com/ps2dev/ps2dev)

Override any directory by exporting its variable before running the scripts.
The SDK downloads are not included in the source archive.

## Build and verify ELF files

```sh
bash build.sh
```

Results:

```text
output/OPL_CF_v1.ELF
output/wLaunchELF_CF_v1.ELF
output/SHA256SUMS.ELF
```

`build.sh` uses the included sources; no generated objects from the developer's
working copy are needed. It verifies the actual embedded CF modules, the game BDM
ABI, the absence of the removed CF logger, and packed ELF segments/entry points.
Older upstream IRX resources in `src/wlaunchelf/iop/__precompiled/` are retained
where the upstream wLaunchELF build requires them. Our CF modules are built from source.

Run the CF host regressions separately:

```sh
source tools/toolchains.sh
modern_env
cd src/opl
bash tests/cf/run.sh
python3 tests/cf/test_mode.py
python3 tests/cf/test_bd_write.py
python3 tests/cf/test_rw_hw.py
python3 tests/cf/test_build_dependencies.py
```

These tests check real driver routines on host models. They do not replace a test
on a PS2 and do not prove game compatibility.

## Build FMCB installer and Gen2 image

After building both ELF files:

```sh
python3 tools/build-fmcb-installer.py
python3 tools/build-fmcb-gen2.py
```

Inputs in `deps/` are the unmodified public FMCB 1.966 distribution and SD2PSX
reference. Their hashes and URLs are in `deps/manifest.json`. No private user card
dump is included. The image builder formats a new 8 MiB filesystem and uses only
the reference KELFs' 32-byte binding fields after checking the rest of each KELF
against the distribution. The bundled mymc++ Python library is GPLv3 and is used
without its optional GUI.

The output installer is `output/FMCBInstaller.zip`; the Gen2 image is
`output/PS2CF-1.0-SCPH10000-Gen2.mcd`. The scripts validate the filesystem, inventory,
package contents, and hashes. Full FMCB core sources are not part of this project.

## Package release assets

```sh
python3 tools/package.py
```

This produces the binary release ZIP, matching source ZIP/tar.gz, and SHA256SUMS
in `dist/`. It excludes build products, SDK installations, private credentials,
Git metadata and local logs from source archives. It also compares the file hashes
in both source formats and rechecks the embedded Gen2 image.
