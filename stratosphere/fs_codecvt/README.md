# fs_codecvt — UTF-8 Codecvt KIP for Atmosphere

## 架构

fs_codecvt 是一个 ARM64 KIP 格式的 FS Overlay 模块，在运行时对 Nintendo FS
系统模块注入 UTF-8 文件名支持的 Hook。它不会作为第二个 FS 进程启动，而是由
修改后的 Fusee 在重建 Package2 时前置到原始 FS 进程映像。

### 启动流程

```text
Hekate payload=fusee.bin
  → ConfigureStratosphere: 从 sdmc:/atmosphere/kips/ 加载 KIP
    → 若 title_id == 0x0100000000000000 (FS) → 标记为 FS Overlay KIP
  → RebuildPackage2: 将 fs_codecvt 注入 FS 进程空间
    [fs_codecvt .text/.data/.bss] [emummc（可选）] [FS 原始 KIP]

FS 进程启动:
  fs_codecvt/start.s 运行:
    1. 检测 ASLR 基址
    2. svcSetProcessMemoryPermission: 设置自身段权限
    3. 清零 .bss
    4. __nx_dynamic: 处理 R_AARCH64_RELATIVE 重定位
    5. BL __init → 安装 Hook
    6. BR __argdata__ → 跳转 emuMMC 或 FS 原始入口
```

Hekate 的 `pkg3=` 会解析 Atmosphère package3 并提取所需组件，但不会执行其中
内嵌的 Fusee，因此不能触发这里新增的 FS Overlay 加载逻辑。验证本模块时必须
通过 `payload=` 启动实际包含该逻辑的 `fusee.bin`，或直接注入该 payload。

Fusee 当前只保存一个 FS Overlay；`sdmc:/atmosphere/kips/` 中不要同时放置多个
Program ID 为 `0100000000000000` 的自定义 KIP。`emummc=0` 只表示进入真实系统，
不会禁用 fs_codecvt。

### 实机验证状态

截至 2026-07-19：

| HOS/变体 | 偏移表 | KIP 启动 | 三项验证 |
|---|---:|---:|---:|
| 19.0.0 exFAT | ✅ | ✅ | **3 PASS** |
| 20.2.0 exFAT | ✅ | ✅ | **3 PASS** |
| 21.2.0 exFAT | ✅ | ✅ | **3 PASS** |
| 22.0.0 exFAT | ✅ | ✅ | **3 PASS** |
| 22.5.0 exFAT | ✅ | ✅ | **3 PASS** |


**3 PASS** meaning:

1. file direct read/write round-trip!
2. lists CJK dir
3. CJK dir lists file

## 构建与部署

`make` 同时生成压缩中间产物 `fs_codecvt.kip` 和用于 fusee overlay 注入的
`fs_codecvt_unpacked.kip`。部署时必须使用后者：

```text
sdmc:/atmosphere/kips/fs_codecvt_unpacked.kip
```

Fusee 会拒绝低三位压缩 flags 非零或总尺寸未按 `0x1000` 对齐的 Overlay KIP。
22.5.0 exFAT 最终实机验证产物为：

```text
SHA-256: C6042EC1C0579E9CFE855751181A866BE490AC8D4632B32A2509BF96C3E5274A
结果: 系统正常启动，三项验证全部 PASS
```

只替换外置 `fs_codecvt_unpacked.kip` 不需要重新编译 package3；修改 Fusee 的
Overlay 加载或 Package2 重建逻辑后，才需要重新构建 Fusee/package3 并更新
启动 payload。其余固件版本以及启用 emuMMC 的组合仍需分别实机验证。

## architecture

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
