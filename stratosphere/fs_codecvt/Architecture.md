## Architecture of fs_codecvt Unified FS overlay KIP

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