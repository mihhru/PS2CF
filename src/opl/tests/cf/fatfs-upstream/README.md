# FatFs - Generic FAT Filesystem Module

---

![layer](./assets/layers.png)

FatFs is a generic FAT/exFAT filesystem module for small embedded systems. The FatFs module is written in compliance with ANSI C (C89) and completely separated from the disk I/O layer. Therefore, it is independent of the platform. It can be incorporated into small microcontrollers with limited resources, such as 8051, PIC, AVR, ARM, Z80, RX, and etc. Also, Petit FatFs module for tiny microcontrollers is available [here](http://elm-chan.org/fsw/ff/00index_p.html).

### Features
- DOS/Windows Compatible FAT/exFAT Filesystem.
- Platform Independent. [Easy to port](http://elm-chan.org/fsw/ff/doc/appnote.html#port).
- Very Small [Footprint](http://elm-chan.org/fsw/ff/doc/appnote.html#memory) for Program Code and Work Area.
- Various [Configuration Options](http://elm-chan.org/fsw/ff/doc/config.html) to Support for:
  - Long File Name in ANSI/OEM or Unicode.
  - exFAT Filesystem, 64-bit LBA and GPT for Huge Storages.
  - Thread Safe for RTOS.
  - Multiple Volumes. (Physical Drives and Partitions)
  - Variable Sector Size.
  - Multiple Code Pages Including DBCS.
  - Read-only, Optional APIs, I/O Buffer, and etc...

---

## Application Interface

![layer](./assets/layers1.png)

FatFs provides various filesystem functions for the applications as shown below.

- **File Access**
  - [f_open](http://elm-chan.org/fsw/ff/doc/open.html) - Open/Create a file
  - [f_close](http://elm-chan.org/fsw/ff/doc/close.html) - Close an open file
  - [f_read](http://elm-chan.org/fsw/ff/doc/read.html) - Read data from the file
  - [f_write](http://elm-chan.org/fsw/ff/doc/write.html) - Write data to the file
  - [f_lseek](http://elm-chan.org/fsw/ff/doc/lseek.html) - Move read/write pointer, Expand size
  - [f_truncate](http://elm-chan.org/fsw/ff/doc/truncate.html) - Truncate file size
  - [f_sync](http://elm-chan.org/fsw/ff/doc/sync.html) - Flush cached data
  - [f_forward](http://elm-chan.org/fsw/ff/doc/forward.html) - Forward data to the stream
  - [f_expand](http://elm-chan.org/fsw/ff/doc/expand.html) - Allocate a contiguous block to the file
  - [f_gets](http://elm-chan.org/fsw/ff/doc/gets.html) - Read a string
  - [f_putc](http://elm-chan.org/fsw/ff/doc/putc.html) - Write a character
  - [f_puts](http://elm-chan.org/fsw/ff/doc/puts.html) - Write a string
  - [f_printf](http://elm-chan.org/fsw/ff/doc/printf.html) - Write a formatted string
  - [f_tell](http://elm-chan.org/fsw/ff/doc/tell.html) - Get current read/write pointer
  - [f_eof](http://elm-chan.org/fsw/ff/doc/eof.html) - Test for end-of-file
  - [f_size](http://elm-chan.org/fsw/ff/doc/size.html) - Get size
  - [f_error](http://elm-chan.org/fsw/ff/doc/error.html) - Test for an error

- **Directory Access**
  - [f_opendir](http://elm-chan.org/fsw/ff/doc/opendir.html) - Open a directory
  - [f_closedir](http://elm-chan.org/fsw/ff/doc/closedir.html) - Close an open directory
  - [f_readdir](http://elm-chan.org/fsw/ff/doc/readdir.html) - Read a directory item
  - [f_findfirst](http://elm-chan.org/fsw/ff/doc/findfirst.html) - Open a directory and read the first item matched
  - [f_findnext](http://elm-chan.org/fsw/ff/doc/findnext.html) - Read a next item matched

- **File and Directory Management**
  - [f_stat](http://elm-chan.org/fsw/ff/doc/stat.html) - Check existence of a file or sub-directory
  - [f_unlink](http://elm-chan.org/fsw/ff/doc/unlink.html) - Remove a file or sub-directory
  - [f_rename](http://elm-chan.org/fsw/ff/doc/rename.html) - Rename/Move a file or sub-directory
  - [f_chmod](http://elm-chan.org/fsw/ff/doc/chmod.html) - Change attribute of a file or sub-directory
  - [f_utime](http://elm-chan.org/fsw/ff/doc/utime.html) - Change timestamp of a file or sub-directory
  - [f_mkdir](http://elm-chan.org/fsw/ff/doc/mkdir.html) - Create a sub-directory
  - [f_chdir](http://elm-chan.org/fsw/ff/doc/chdir.html) - Change current directory
  - [f_chdrive](http://elm-chan.org/fsw/ff/doc/chdrive.html) - Change current drive
  - [f_getcwd](http://elm-chan.org/fsw/ff/doc/getcwd.html) - Retrieve the current directory and drive

---

## Media Access Interface

![layer](./assets/layers2.png)

Since FatFs module is the *Filesystem Layer* independent of platforms and storage media, it is completely separated from the physical devices, such as memory card, harddisk, and any type of storage device. The storage device control module is *not any part of FatFs module* and it needs to be provided by implementer. FatFs controls the storage devices via a simple media access interface shown below. Also, sample implementations for some platforms are available in the downloads. A function checker for storage device control module is available [here](http://elm-chan.org/fsw/ff/res/app4.c).

- **Storage Device Controls**
  - [disk_status](http://elm-chan.org/fsw/ff/doc/dstat.html) - Get device status
  - [disk_initialize](http://elm-chan.org/fsw/ff/doc/dinit.html) - Initialize device
  - [disk_read](http://elm-chan.org/fsw/ff/doc/dread.html) - Read data
  - [disk_write](http://elm-chan.org/fsw/ff/doc/dwrite.html) - Write data
  - [disk_ioctl](http://elm-chan.org/fsw/ff/doc/dioctl.html) - Control device dependent functions

- **Real Time Clock**
  - [get_fattime](http://elm-chan.org/fsw/ff/doc/fattime.html) - Get current time

---

## Resources

The FatFs module is a free software opened for education, research, and development. You can use, modify, and/or redistribute it for any purpose without any restriction under your responsibility. For further information, refer to the application note.

- **Getting Started:** [FatFs Application Note](http://elm-chan.org/fsw/ff/doc/appnote.html)
- **Download:** [FatFs R0.15 (zip)](http://elm-chan.org/fsw/ff/arc/ff15.zip) | [Changes](http://elm-chan.org/fsw/ff/updates.html) | [Latest Patch](http://elm-chan.org/fsw/ff/patches.html) *(August 15, 2023)*
- **Download:** [FatFs sample projects for various platforms](http://elm-chan.org/fsw/ff/ffsample.zip) *(November 6, 2022)*
- **Download:** [Previous Releases](http://elm-chan.org/fsw/ff/archives.html)
- **Community:** [FatFs User Forum](http://elm-chan.org/fsw/ff/bd/)
- [FAT32 Specification by Microsoft](https://msdn.microsoft.com/en-us/windows/hardware/gg463080.aspx) (The authorized document on FAT filesystem)
- [The basics of FAT filesystem](http://elm-chan.org/docs/fat_e.html) (FatFs is written based on this documentation)
- [The basics of exFAT filesystem](http://elm-chan.org/docs/exfat_e.html) (FatFs is written based on this documentation)
- [How to use MMC/SDC](http://elm-chan.org/docs/mmc/mmc_e.html)
- [Playing with FlashAir and FatFs](http://elm-chan.org/junk/fa/faff.html)
- [Nemuisan's Blog](http://nemuisan.blog.bai.ne.jp/) (Well-written implementations for STM32F/SPI & SDIO and LPC4088/SDMMC)
- [Read SD card with FatFs on STM32F4xx devices by Tilen Majerle](http://stm32f4-discovery.net/2014/07/library-21-read-sd-card-fatfs-stm32f4xx-devices/) (Quick and easy implementation for STM32F4-Discovery)
- [Benchmark 1](http://elm-chan.org/fsw/ff/res/rwtest1.png) (ATmega1284/20MHz with MMC via USART in SPI, CFC via GPIO)
- [Benchmark 2](http://elm-chan.org/fsw/ff/res/rwtest2.png) (LPC2368/72MHz with MMC via MCI)
- [Demo movie of an application](http://elm-chan.org/fsw/ff/res/fd.mp4) (this project is in ffsample.zip/lpc23xx)

---

[Return](http://elm-chan.org/fsw_e.html)
