# fs_codecvt — UTF-8 Codecvt KIP for Atmosphere

## 架构

fs_codecvt 是一个 ARM64 KIP 格式的 FS Overlay 模块，在运行时对 Nintendo FS
系统模块注入 UTF-8 文件名支持的 Hook。它不会作为第二个 FS 进程启动，而是由
修改后的 Fusee 在重建 Package2 时前置到原始 FS 进程映像。

### 与 emuMMC 的对比

| | emuMMC | fs_codecvt |
|---|---|---|
| 目标 | 劫持 SD/eMMC 读写，重定向到镜像 | 替换 PF_CHARCODE 6 个槽函数 + SFAT 目录过滤器 |
| 注入方式 | KIP 前置注入（同） | KIP 前置注入（同） |
| Hook 方式 | B 指令覆盖函数入口 | B 指令覆盖函数入口（同） |
| 代码量 | ~5000 行（含 SD 驱动栈） | ~400 行（纯 codecvt 逻辑） |
| 固件覆盖 | 1.0.0 ~ 22.5.0（72 个偏移文件） | 19.0.0 ~ 22.5.0 exFAT（5 个版本） |

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
| 19.0.0 exFAT | ✅ | ✅ | **3 PASS** (19.0.1 verified) |
| 20.2.0 exFAT | ✅ | ✅ | **3 PASS** |
| 21.2.0 exFAT | ✅ | ✅ | **3 PASS** |
| 22.0.0 exFAT | ✅ | ✅ | **3 PASS** |
| 22.5.0 exFAT | ✅ | ✅ | **3 PASS** |

原 `patches.ini` 在 19、20 和 22.5 上通过，不能代替对应版本的 KIP 实机验证。

## 实现细节

### 1. PF_CHARCODE 六个槽函数（纯 C++）

原始 Nintendo 代码使用 CP932/Shift-JIS 编码表处理 FAT 长文件名。
fs_codecvt 用 B 指令将六个函数入口重定向到 C++ UTF-8 实现：

| 槽位 | 函数 | 返回约定 | 需要 Trampoline？ |
|---|---|---|---|
| +0x00 | oem2unicode | W0=(oem_width<<16)\|uni_width | ❌ 直接 B |
| +0x08 | unicode2oem | W0=(oem_width<<16)\|uni_width | ❌ 直接 B |
| +0x10 | oem_char_width | W0=字节宽度 | ❌ |
| +0x18 | is_oem_mb_char | W0=bool（参数是 int 1 或 2） | ❌ |
| +0x20 | unicode_char_width | W0=2 | ❌ |
| +0x28 | is_unicode_mb_char | W0=false | ❌ |

oem2unicode 与 unicode2oem 都由 C++ 直接返回打包宽度：
`W0 = (oem_width << 16) | uni_width`。这样与 PrFILE2 的真实 ABI 一致，
无需额外 trampoline。

早期 KIP 曾错误地让 oem2unicode 返回 `W0=oem_width`，再由 trampoline 设置
`W1=2`，导致 FS 启动黑屏。单 Hook 实机隔离证明真实 ABI 必须把两个宽度打包
进 `W0`；修正后 Hook 0 和完整版本均成功启动。

### 2. utf8dir 过滤器（C++ + 动态 ABI bridge）

SFAT Directory::Read 在扫描文件名时有一个条件分支（B.HS/B.LO）
会拒绝所有 ≥ 0x80 的字节。fs_codecvt 将其替换为跳转到 cave（NOP 空洞），
在 cave 中放置一个最小 ABI bridge，调用 C++ 验证器。UTF-8 验证算法
全部保留在 C++ 中；bridge 只负责保存 FS 中段 Hook 的 live registers、
传递参数，并恢复 C++ 返回的新扫描索引。

```
SFAT 循环中:
  cmp W9, #0x7F
  B.HS reject_entry     ← 替换为 B cave
  ascii_checks:         ← ascii_checks 标签
  ...
  scan_continue:        ← scan_continue 标签

cave（NOP 空洞，由 codecvt 替换产生）:
  bridge 调用 utf8_dir_validate_cpp → 返回 DirResult(index, action):
    DIR_REJECT         (0) → B reject_entry
    DIR_ASCII_CONTINUE (1) → B ascii_checks
    DIR_SCAN_CONTINUE  (2) → B scan_continue
```

Cave 位于 unicode2oem 替换代码之后、oem_char_width 之前，
大小因固件版本而异（≥ 0xA0 字节）。当前 bridge 为 35 条指令、0x8C
字节，由 install() 在运行时动态构建。AArch64 `B.cond` 的 imm19 会写入
bits 23:5，并在写入前检查分支范围。

### 3. 偏移量数据

每个固件版本需要偏移与原始指令签名（见 fs_offsets.cpp）：
- codecvt[6]: 六个槽函数的入口偏移
- sanitize[3]: 三个 TBNZ → NOP 站点
- dir_hook/…/dir_scan_continue: utf8dir 的 4 个标签偏移
- name_reg/bound_reg: 寄存器编号（19.x/20.x 用 X25/X19，21.x+ 用 X26/X24）
- cave/cave_size: 计算值
- codecvt_entry[2]/dir_hook_opcode: 运行时精确识别签名

偏移表已覆盖固件: 19.0.0 / 20.2.0 / 21.2.0 / 22.0.0 / 22.5.0（仅 exFAT）。
“已覆盖”表示存在偏移和签名，不等于已完成对应版本的 KIP 实机验证。

### 4. 版本检测

find_fs() 从当前 overlay 的 `__argdata__` 开始按页扫描后续 RX 映射。
每个候选必须同时匹配 codecvt 的两条入口指令、目录 Hook 原始指令和
三个 TBNZ sanitizer。这样既能区分 21.2.0 与 22.x，也能在布局为
`fs_codecvt → emuMMC → FS` 时找到真正的 FS text base。

### 5. 重定位处理

KIP 编译为 -fPIE。内核的 KIP Loader 处理部分重定位，但 `&function` 类
的函数指针需要 R_AARCH64_RELATIVE 处理。nx_dynamic.c 提供了最小实现。

### 6. FS text 映射、KAC 与 cache

fs_codecvt 取得自身 FS 进程句柄后，使用 `svcMapProcessMemory` 为原始 FS text
建立 RW alias。所有分支范围、原始指令和 cave 大小检查完成后才统一写入补丁；
随后刷新 RW alias 的 D-cache、失效原 RX 地址的 I-cache，并解除映射。

Overlay 需要 `svcSetProcessMemoryPermission`、`svcMapProcessMemory` 和
`svcUnmapProcessMemory` 等特权 SVC。Fusee 使用 Atmosphère 内置、按 FS 版本
适配的 emuMMC capabilities 作为最终 FS KIP 的 KAC，即使 `emummc=0` 也一样。
Cache maintenance 与 emuMMC 的实现保持一致，执行 DC/IC 操作时处理
`TPIDRRO_EL0 + 0x104` 标记。

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
