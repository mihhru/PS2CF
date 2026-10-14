# Сборка PS2CF 1.0

[English](BUILDING.md) | [Русский](BUILDING_RU.md)

Нужна Linux x86-64 или WSL. Релиз проверен в Ubuntu 26.04/WSL.
Распакуйте любой архив исходников и перейдите в `PS2CF-1.0/`.
В ZIP и tar.gz одинаковые файлы. Запускайте сценарии через `bash`, чтобы
права исполнения после распаковки ZIP не влияли на запуск.

## Окружение

Используются две совместимые среды. Сценарий `tools/toolchains.sh` переключает
компиляторы, заголовки и библиотеки вместе, без смешивания разных SDK.

| Переменная | Каталог по умолчанию | Назначение |
| --- | --- | --- |
| `PS2CF_MODERN` | `$HOME/ps2dev-v2.0.0` | PS2DEV 2.0.0: OPL и современные CF/BDM/FatFs-модули IOP |
| `PS2CF_LEGACY_SDK` | `$HOME/ps2dev-v1.0` | SDK, ports и gsKit для EE-части wLaunchELF |
| `PS2CF_LEGACY_TOOLS` | `$HOME/ps2dev-v1.1` | Старые EE/IOP-компиляторы и утилиты SDK |

Для Debian/Ubuntu:

```sh
sudo apt-get update
sudo apt-get install build-essential git python3 python3-yaml curl xz-utils unzip p7zip-full skopeo
bash tools/setup-toolchains.sh
```

Сценарий скачивает официальные Linux-сборки PS2DEV 2.0.0 и 1.1, извлекает SDK 1.0
с ports и gsKit из `docker.io/ps2dev/ps2dev:v1.0` через skopeo (Docker не нужен),
затем согласует specs старого GCC с SDK 1.0. Существующие каталоги SDK не заменяются.
Если SDK уже установлен, он должен иметь совместимые библиотеки и ту же правку specs.
Сами SDK в архив исходников не входят.

Источники: [PS2DEV 2.0.0](https://github.com/ps2dev/ps2dev/releases/tag/v2.0.0),
[PS2DEV 1.1](https://github.com/ps2dev/ps2dev/releases/tag/v1.1),
[PS2DEV](https://github.com/ps2dev/ps2dev).
Чтобы использовать другие пути, экспортируйте переменные из таблицы.

## Сборка приложений

```sh
bash build.sh
```

Результат:

```text
output/OPL_CF_v1.ELF
output/wLaunchELF_CF_v1.ELF
output/SHA256SUMS.ELF
```

Сборка не требует объектных файлов из рабочего дерева автора. Проверяются реальные
встроенные CF-модули, ABI BDM, отсутствие удалённого CF-логгера и сегменты/точки входа
упакованных ELF. Старые IRX-ресурсы upstream wLaunchELF оставлены в
`src/wlaunchelf/iop/__precompiled/`; наши CF-модули собираются из исходников.

Проверки драйвера на ПК:

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

Проверки выполняют реальные функции драйвера на моделях оборудования;
они не заменяют аппаратную проверку и не гарантируют совместимость игр.

## Установщик FMCB и образ Gen2

После сборки двух ELF:

```sh
python3 tools/build-fmcb-installer.py
python3 tools/build-fmcb-gen2.py
```

В `deps/` включены неизменённый публичный дистрибутив FMCB 1.966 и эталон SD2PSX.
Источники и SHA256 указаны в `deps/manifest.json`. Личных дампов карт в комплекте нет.
Создаётся новая файловая система 8 МиБ; из эталонных KELF используются только
32-байтные поля привязки после проверки совпадения остального файла с дистрибутивом.
Библиотека mymc++ включена под GPLv3; её необязательный GUI не используется.

Результат: `output/FMCBInstaller.zip` и
`output/PS2CF-1.0-SCPH10000-Gen2.mcd`. Проверяются файловая система,
состав, содержимое пакетов и хэши. Полных исходников ядра FMCB в проекте нет.

## Архивы релиза

```sh
python3 tools/package.py
```

В `dist/` появятся ZIP релиза, одинаковые по составу ZIP/tar.gz исходников и SHA256SUMS.
Объектные файлы, SDK, Git-метаданные, приватные данные и локальные журналы в исходные
архивы не попадают. Сборщик сравнивает хэши файлов в обоих форматах исходников
и повторно проверяет вложенный образ Gen2.
