# fs_codecvt — Unified UTF-8 Codecvt KIP for Atmosphère

`fs_codecvt` is a single FS overlay KIP that enables UTF-8 filenames on both
FAT32 and ExFAT SD media. It targets only the ExFAT-capable FS binaries shipped
in Nintendo's **FAT32 + exFAT** firmware packages; FAT-only firmware binaries
are intentionally out of scope.

Deploy one file, with no media-specific profile selection:

```text
sdmc:/atmosphere/fs_overlays/fs_codecvt_unpacked.kip
```

The unified build uses a conservative dual-contract design. Complete-string
path hooks perform lossless UTF-8 conversion for both media types, while the
legacy `PF_CHARCODE` layer uses a bounded two-byte decoder so FAT32 cannot read
past its temporary DBCS buffer. The FAT 8.3 alias hook is reached only on the
FAT path; ExFAT continues through the shared long-name path. This avoids an
unreliable boot-time media-format guess and prevents the FAT32 hangs caused by
the old global six-slot ExFAT replacement.

## Architecture

`fs_codecvt` is an ARM64 KIP-format FS process overlay. It installs UTF-8
filename hooks into Nintendo's FS sysmodule at runtime. It does not start a
second FS process. Modified Fusee prepends the overlay to the original FS
process image while rebuilding Package2.

### Comparison with emuMMC

| | emuMMC | fs_codecvt |
|---|---|---|
| Purpose | Redirect SD/eMMC I/O to an image | UTF-8 path conversion, FAT SFN generation, and directory-output filtering |
| Injection | Prepended KIP image | Prepended KIP image |
| Hook method | `B` instruction at function entry | `B` instruction at function entry and a directory-scan bridge |
| Approximate scope | ~5,000 lines including the SD stack | ~500 lines for conversion, SFN, and the directory bridge |
| Firmware coverage | HOS 1.0.0–22.5.0 | Five ExFAT-capable FS binaries from HOS 19.0.0–22.5.0 |

### Boot flow

```text
Hekate payload=fusee.bin
  -> ConfigureStratosphere loads an FS overlay from
     sdmc:/atmosphere/fs_overlays/
     -> only a KIP with title_id == 0x0100000000000000 (FS) is accepted
  -> RebuildPackage2 prepends fs_codecvt to the FS process image
     [fs_codecvt .text/.data/.bss] [emuMMC, optional] [original FS KIP]

FS process startup:
  fs_codecvt/start.s
    1. Detect the ASLR base.
    2. Set permissions for the overlay segments with
       svcSetProcessMemoryPermission.
    3. Clear .bss.
    4. Process R_AARCH64_RELATIVE relocations through __nx_dynamic.
    5. Call __init to locate FS and install the hooks.
    6. Branch through __argdata__ to emuMMC or the original FS entry point.
```

Hekate's `pkg3=` option parses the Atmosphère `package3` and extracts the
required components, but it does not execute the Fusee image embedded in that
package. Therefore, it cannot trigger the custom FS-overlay loader described
here. Testing must boot the modified `fusee.bin` through `payload=`, or inject
that payload directly.

`sdmc:/atmosphere/kips/` remains reserved for traditional standalone
initial-process KIPs. FS overlays must be placed in
`sdmc:/atmosphere/fs_overlays/`. Fusee accepts exactly one FS overlay and
reports a fatal error if the directory contains multiple overlays or a KIP
whose Program ID is not FS. `emummc=0` selects sysMMC; it does not disable
`fs_codecvt`.

## Hardware validation status

The firmware entries below refer to the ExFAT-capable FS binary installed by
Daybreak's **FAT32 + exFAT** option. The offset set identifies that binary, not
the current SD-card format. The same offset set is used with FAT32 and ExFAT
media.

Status as of 2026-07-27:

| HOS / FS binary | Unified offsets | FAT32 media | ExFAT media with unified KIP |
|---|---:|---:|---:|
| 19.0.0 ExFAT-capable | Yes | **3 PASS** on 19.0.1 | Pending regression |
| 20.2.0 ExFAT-capable | Yes | **3 PASS** | Pending regression |
| 21.2.0 ExFAT-capable | Yes | **3 PASS** | Pending regression |
| 22.0.0 ExFAT-capable | Yes | **3 PASS** | Pending regression |
| 22.5.0 ExFAT-capable | Yes | **3 PASS** | Pending regression |

The historical three-pass results from the old global six-slot ExFAT KIP do
not replace regression testing of the unified KIP on ExFAT media.

`3 PASS` means all three checks succeeded:

- Direct file read/write round trip through a CJK path.
- Enumeration of `/ROM` returns the CJK directory.
- Enumeration of the CJK directory returns the expected file.

## Implementation details

### 1. Unified dual-contract conversion

Nintendo's original code uses a CP932/Shift-JIS character-code table for FAT
long filenames. PrFILE2 exposes two incompatible caller contracts, so the
global six-slot replacement used by the old ExFAT build is unsafe on FAT32:

- At the legacy `PF_CHARCODE` layer, only slot 0 and slot 3 are hooked. Slot 0
  reads no more than two bytes, respecting PrFILE2's temporary DBCS buffer.
  Slot 3 classifies UTF-8 lead and continuation bytes.
- `transformFromUnicodeToNormal`, `transformInUnicode`, and the pattern reader
  receive complete strings and can safely perform true three-byte UTF-8
  conversion.
- On FAT32, the `parseShortName` hook generates a valid ASCII 8.3 alias seed
  for a CJK long filename. ExFAT does not execute the traditional FAT SFN path.

Flights #24–#28 demonstrated that reading the third byte globally from slot 0
causes the FAT32 test program to hang or black-screen. The split contract is a
correctness requirement, not an optional optimization.

### 2. `utf8dir` filter and dynamic ABI bridge

The filename scan in `SFAT Directory::Read` contains a conditional branch that
rejects every byte greater than or equal to `0x80`. `fs_codecvt` replaces that
branch with a jump to a code cave. A small dynamically generated ABI bridge in
the cave calls the C++ UTF-8 validator.

The validator remains entirely in C++. The bridge only preserves the live
registers at this mid-function hook, passes the arguments, and restores the new
scan index returned by C++.

```text
Inside the SFAT loop:
  cmp W9, #0x7F
  B.HS reject_entry       <- replaced with B cave
  ascii_checks:
  ...
  scan_continue:

Inside the cave:
  bridge calls utf8_dir_validate_cpp -> DirResult(index, action)
    DIR_REJECT         (0) -> B reject_entry
    DIR_ASCII_CONTINUE (1) -> B ascii_checks
    DIR_SCAN_CONTINUE  (2) -> B scan_continue
```

The cave is located after the replaced `unicode2oem` code and before
`oem_char_width`. Its size varies by firmware and is at least `0xA0` bytes.
The current bridge contains 35 instructions (`0x8C` bytes) and is generated by
`install()` at runtime. Before writing an AArch64 `B.cond`, the installer checks
that its `imm19` target is in range.

### 3. Offset data

Each supported ExFAT-capable FS binary needs offsets and original-instruction
signatures in `fs_offsets.cpp`:

- `codecvt[6]`: entry offsets for all six original character-code functions.
- `sanitize[3]`: TBNZ sites used only as image identity checks; the unified
  build does not modify them.
- `dir_hook` through `dir_scan_continue`: the four directory-filter labels.
- `name_reg`, `bound_reg`, and `byte_reg`: registers used by that firmware's
  directory scan.
- `cave` and `cave_size`: the code-cave location and capacity.
- The three complete-string entry points and the FAT `parseShortName` entry.
- `codecvt_entry`, `dir_hook_opcode`, and six `identity_checks` used for exact
  runtime image identification.

The table covers the ExFAT-capable FS binaries for HOS 19.0.0, 20.2.0,
21.2.0, 22.0.0, and 22.5.0. FAT-only firmware offsets are intentionally not
included. All five entries have been uniquely matched against their extracted
FS images.

### 4. Version detection

`find_fs()` scans page-aligned candidates in the RX mapping after the current
overlay's `__argdata__`. A candidate must match the codecvt entry, directory
hook, high-level conversion functions, pattern reader, SFN function, and six
additional identity instructions. HOS 22.0.0 and 22.5.0 are further separated
by different constant instructions in the reject path.

This prevents adjacent firmware versions from being confused and locates the
real FS text base in both supported layouts:

```text
[fs_codecvt] [FS]
[fs_codecvt] [emuMMC] [FS]
```

### 5. Relocation handling

The KIP is compiled with `-fPIE`. The kernel KIP loader processes part of the
relocation data, while function-address expressions such as `&function` also
require `R_AARCH64_RELATIVE` handling. `nx_dynamic.c` provides the minimal
runtime implementation.

### 6. FS text mapping, KAC, and cache maintenance

After acquiring a handle to its own FS process, `fs_codecvt` uses
`svcMapProcessMemory` to create an RW alias of the original FS text. It verifies
all branch ranges, original instructions, and cave sizes before committing any
patch. It then flushes the RW alias from D-cache, invalidates I-cache for the
original RX mapping, and unmaps the alias.

The overlay requires privileged SVCs including
`svcSetProcessMemoryPermission`, `svcMapProcessMemory`, and
`svcUnmapProcessMemory`. Fusee applies Atmosphère's firmware-specific emuMMC
capabilities as the final FS KAC even when `emummc=0`. Cache maintenance follows
the emuMMC implementation and handles the `TPIDRRO_EL0 + 0x104` flag around
DC/IC operations.

## Building and deployment

Running `make` produces two files:

- `fs_codecvt.kip`: compressed intermediate artifact.
- `fs_codecvt_unpacked.kip`: uncompressed FS overlay for Fusee injection.

Deploy only the uncompressed artifact:

```text
sdmc:/atmosphere/fs_overlays/fs_codecvt_unpacked.kip
```

Fusee rejects an overlay whose KIP compression flags or segment layout are not
valid for overlay injection.

The current unified build, after removing the out-of-scope FAT-only table, is:

```text
SHA-256: 69346316550F6A41EBB490208FD653F190658474EC858E4A57F0B8345D5E2703
Size:    9860 bytes
Marker:  fs_codecvt-unified-fat32-exfat-v1
```

Historical validation artifacts include:

- Flight #47, the cleaned 19.0.1 FAT32 production baseline:
  `B4DFC77894852EECCB2BF7244493B3F25A81A8A11719691F1F6F25FD86E2A0F8`.
- Flight #48, the first five-firmware FAT32 candidate:
  `B20FFE7A354FB6AD536BD85C61FFAB7BAD8B11279CD9E5E1B2CDC88605D9DBA0`.

Replacing only the external `fs_codecvt_unpacked.kip` does not require
rebuilding `package3`. Changes to the Fusee overlay loader or Package2 rebuild
logic require rebuilding Fusee/package3 and updating the boot payload.
Additional firmware versions and emuMMC combinations must be validated
separately.
