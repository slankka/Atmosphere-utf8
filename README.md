
![Banner](img/banner.png?raw=true)
=====

![License](https://img.shields.io/badge/License-GPLv2-blue.svg)
[![Chat on Discord](https://img.shields.io/badge/Discord-5865f2?logo=discord&logoColor=white)](https://discordapp.com/invite/ZdqEhed)
![Made with Notepad++](img/np++.png?raw=true)

Atmosphère is a work-in-progress customized firmware for the Nintendo Switch.

**UTF-8 KIP fs_codecvt**
=====

This repo provides a KIP overlay that enables UTF-8 filenames on the Nintendo Switch by replacing the FS sysmodule's built-in codecvt.

```
 An "overlay" does NOT replace the original FS.kip1. Instead, it is
  injected INTO the FS process address space at boot, hooks specific
  functions, then chains to the real FS code.

  During Package2 rebuild, Fusee arranges the FS process image as:

    [ fs_codecvt .text / .data / .bss ] [ emummc (optional) ] [ FS.kip1 original ]

  Boot chain: codecvt startup -> install hooks -> jump to emummc or FS entry

 ========================================================================

                         +---------------------+
                         |   Application/Game   |
                         +----------+----------+
                                    |
                         FS IPC (SVC calls)
                                    |
   +-----------------------------------------------------------------+
   |                   FS Process Address Space                       |
   |                                                                  |
   |   +----------------------------------------------------------+  |
   |   |  fs_codecvt_utf8.kip                                     |  |
   |   |  ======================================================  |  |
   |   |  Type: FS Overlay (title_id = 0x0100000000000000)        |  |
   |   |                                                          |  |
   |   |  Hooks installed into FS memory at boot:                 |  |
   |   |    * PF_CHARCODE codecvt functions                       |  |
   |   |    * SFAT Directory::Read (filename sanitization)        |  |
   |   |    * FileName / PathName normalization                   |  |
   |   |                                                          |  |
   |   |  Runs FIRST in FS process, then chains to next layer.    |  |
   |   +--------------------------+-------------------------------+  |
   |                              |                                  |
   |                    (chained via BR __argdata__)                  |
   |                              v                                  |
   |   +----------------------------------------------------------+  |
   |   |  FS.kip1  (Nintendo Original Filesystem Service)         |  |
   |   |  ======================================================  |  |
   |   |  * Real filesystem logic                                  |  |
   |   |  * FAT32 / exFAT driver                                   |  |
   |   |  * File CRUD, dir enumeration, access control             |  |
   |   |  * NCA / RomFS / SaveData management                      |  |
   |   +--------------------------+-------------------------------+  |
   |                              |                                  |
   |                     Storage I/O (NAND read/write)               |
   |                              |                                  |
   |   +----------------------------------------------------------+  |
   |   |  EMUMMC (emummc.kip, optional)                           |  |
   |   |  ======================================================  |  |
   |   |  * Also injected into FS process space                    |  |
   |   |  * Intercepts NAND/eMMC I/O at storage driver level       |  |
   |   |  * Redirects: sysNAND chip -> SD card image file          |  |
   |   |  * Transparent: FS.kip1 does NOT know it is emulated      |  |
   |   +--------------------------+-------------------------------+  |
   |                              |                                  |
   +------------------------------+---------------------------------+
                                  |
                       +----------+----------+
                       |                     |
              +--------v--------+   +--------v--------+
              |  Physical NAND   |   |     SD Card      |
              |  (sysNAND chip)  |   | (emuMMC image)   |
              |                  |   |                  |
              |  NEVER TOUCHED   |   |  All CFW writes  |
              |  by custom FW    |   |  go here only    |
              +------------------+   +------------------+

 ========================================================================

  Layer        | Runs in       | Purpose
  -------------+---------------+------------------------------------------
  Application  | own process   | Games, homebrew -- calls FS via IPC
  fs_codecvt   | FS process    | Hooks filename encoding BEFORE real FS
  FS.kip1      | FS process    | Core filesystem: files, dirs, NCA, etc.
  EMUMMC       | FS process    | Redirects NAND I/O to SD card image
  Storage      | hardware      | Physical NAND chip or SD card
```

Components
=====

Atmosphère consists of multiple components, each of which replaces/modifies a different component of the system:

* Fusée: First-stage Loader, responsible for loading and validating stage 2 (custom TrustZone) plus package2 (Kernel/FIRM sysmodules), and patching them as needed. This replaces all functionality normally in Package1loader/NX Bootloader.
* Exosphère: Customized TrustZone, to run a customized Secure Monitor
* Thermosphère: EL2 EmuNAND support, i.e. backing up and using virtualized/redirected NAND images
* Stratosphère: Custom Sysmodule(s), both Rosalina style to extend the kernel/provide new features, and of the loader reimplementation style to hook important system actions
* Troposphère: Application-level Horizon OS patches, used to implement desirable CFW features

Licensing
=====

This software is licensed under the terms of the GPLv2, with exemptions for specific projects noted below.

You can find a copy of the license in the [LICENSE file](LICENSE).

Exemptions:
* [Nintendo](https://github.com/Nintendo) is exempt from GPLv2 licensing and may (at its option) instead license any source code authored for the Atmosphère project under the Zero-Clause BSD license.

Credits
=====

Atmosphère is currently being developed and maintained by __SciresM__, __TuxSH__, __hexkyz__, and __fincs__.<br>
In no particular order, we credit the following for their invaluable contributions:

* __switchbrew__ for the [libnx](https://github.com/switchbrew/libnx) project and the extensive [documentation, research and tool development](http://switchbrew.org) pertaining to the Nintendo Switch.
* __devkitPro__ for the [devkitA64](https://devkitpro.org/) toolchain and libnx support.
* __ReSwitched Team__ for additional [documentation, research and tool development](https://reswitched.github.io/) pertaining to the Nintendo Switch.
* __ChaN__ for the [FatFs](http://elm-chan.org/fsw/ff/00index_e.html) module.
* __Marcus Geelnard__ for the [bcl-1.2.0](https://sourceforge.net/projects/bcl/files/bcl/bcl-1.2.0) library.
* __naehrwert__ and __st4rk__ for the original [hekate](https://github.com/nwert/hekate) project and its hwinit code base.
* __CTCaer__ for the continued [hekate](https://github.com/CTCaer/hekate) project's fork and the [minerva_tc](https://github.com/CTCaer/minerva_tc) project.
* __m4xw__ for development of the [emuMMC](https://github.com/m4xw/emummc) project.
* __Riley__ for suggesting "Atmosphere" as a Horizon OS reimplementation+customization project name.
* __hedgeberg__ for research and hardware testing.
* __lioncash__ for code cleanup and general improvements.
* __jaames__ for designing and providing Atmosphère's graphical resources.
* Everyone who submitted entries for Atmosphère's [splash design contest](https://github.com/Atmosphere-NX/Atmosphere-splashes).
* _All those who actively contribute to the Atmosphère repository._
