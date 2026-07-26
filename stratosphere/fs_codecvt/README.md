# fs_codecvt — UTF-8 Codecvt KIP for Atmosphere

## 架构

fs_codecvt 是一个 ARM64 KIP 格式的 FS Overlay 模块，在运行时对 Nintendo FS
系统模块注入 UTF-8 文件名支持的 Hook。它不会作为第二个 FS 进程启动，而是由
修改后的 Fusee 在重建 Package2 时前置到原始 FS 进程映像。

### 与 emuMMC 的对比

| | emuMMC | fs_codecvt |
|---|---|---|
| 目标 | 劫持 SD/eMMC 读写，重定向到镜像 | FAT32 UTF-8 路径转换、SFN 生成与目录输出过滤 |
| 注入方式 | KIP 前置注入（同） | KIP 前置注入（同） |
| Hook 方式 | B 指令覆盖函数入口 | B 指令覆盖函数入口（同） |
| 代码量 | ~5000 行（含 SD 驱动栈） | 约 500 行（转换、SFN 与目录 bridge） |
| 固件覆盖 | 1.0.0 ~ 22.5.0（72 个偏移文件） | 19.0.0 ~ 22.5.0 ExFAT-capable FS（5 个版本） |

### 启动流程

```text
Hekate payload=fusee.bin
  → ConfigureStratosphere: 从 sdmc:/atmosphere/fs_overlays/ 加载 FS Overlay
    → 仅接受 title_id == 0x0100000000000000 (FS) 的 KIP
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

`sdmc:/atmosphere/kips/` 保留给传统的独立进程 KIP；FS Overlay 必须放在
`sdmc:/atmosphere/fs_overlays/`。Fusee 当前只接受一个 FS Overlay，发现多个文件
或非 FS Program ID 的 KIP 会直接报错。`emummc=0` 只表示进入真实系统，不会禁用
fs_codecvt。

### 实机验证状态

截至 2026-07-26，表中的介质均指 **FAT32 SD 卡**；FS 变体指 Daybreak
选择 `FAT32 + exFAT` 后实际加载的 ExFAT-capable FS：

| HOS/FS 变体 | FAT32 双契约偏移 | 静态唯一匹配 | FAT32 实机三项验证 |
|---|---:|---:|---:|
| 19.0.0 ExFAT-capable | ✅ | ✅ | **3 PASS**（19.0.1，Flight #47） |
| 20.2.0 ExFAT-capable | ✅ | ✅ | 待测试 |
| 21.2.0 ExFAT-capable | ✅ | ✅ | 待测试 |
| 22.0.0 ExFAT-capable | ✅ | ✅ | 待测试 |
| 22.5.0 ExFAT-capable | ✅ | ✅ | 待测试 |

原始 exFAT 介质方案在部分版本上的 3 PASS 不能代替当前 FAT32 双契约 KIP 的
实机验证。基于 Atmosphère 1.9.2 的整套系统也不能直接启动 HOS 20–22；测试外置
KIP 时必须搭配支持目标 HOS 的 Atmosphère/Fusee。

## 实现细节

### 1. FAT32 双契约转换

原始 Nintendo 代码使用 CP932/Shift-JIS 编码表处理 FAT 长文件名。
PrFILE2 同时存在两种调用契约，不能像原始 exFAT 方案一样全局替换六槽：

- 旧 `PF_CHARCODE` 层只 Hook slot0 与 slot3。slot0 最多读取两字节，避免从
  PrFILE2 的 DBCS 临时缓冲越界；slot3 负责 UTF-8 首字节/续字节分类。
- `transformFromUnicodeToNormal`、`transformInUnicode` 和 pattern reader 接收完整
  字符串，可安全执行真正的三字节 UTF-8 转换。
- FAT32 `parseShortName` 为 CJK 长文件名生成合法 ASCII 8.3 别名种子。

Flight #24–#28 已确认，全局 slot0 读取第三字节会导致测试程序黑屏；双契约分流
是 FAT32 方案的必要条件，而不是可选优化。

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
- sanitize[3]: 只用于识别映像的 TBNZ 站点，当前方案不修改它们
- dir_hook/…/dir_scan_continue: utf8dir 的 4 个标签偏移
- name_reg/bound_reg/byte_reg: 各版本目录扫描使用的寄存器
- cave/cave_size: 计算值
- 三个完整字符串入口和 FAT32 `parseShortName` 入口
- codecvt_entry、dir_hook_opcode 与六个 identity_checks: 运行时精确识别签名

偏移表覆盖 19.0.0 / 20.2.0 / 21.2.0 / 22.0.0 / 22.5.0 的 ExFAT-capable
FS。它们均已对提取实体完成唯一匹配验证；除 19.0.1 外仍需 FAT32 实机测试。

### 4. 版本检测

find_fs() 从当前 overlay 的 `__argdata__` 开始按页扫描后续 RX 映射。
每个候选必须同时匹配 codecvt、目录 Hook、高层转换、pattern、SFN 和六条
补充身份指令。22.0.0 与 22.5.0 还使用拒绝路径中不同的常量指令进行区分。
这样既能防止相邻版本误匹配，也能在布局为
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
sdmc:/atmosphere/fs_overlays/fs_codecvt_unpacked.kip
```

Fusee 会拒绝低三位压缩 flags 非零或总尺寸未按 `0x1000` 对齐的 Overlay KIP。
19.0.1 FAT32 当前已验证基准为 Flight #47：

```text
SHA-256: B4DFC77894852EECCB2BF7244493B3F25A81A8A11719691F1F6F25FD86E2A0F8
结果: 系统正常启动，三项验证全部 PASS
```

加入 20.2.0–22.5.0 偏移后的多固件候选为 Flight #48：

```text
SHA-256: B20FFE7A354FB6AD536BD85C61FFAB7BAD8B11279CD9E5E1B2CDC88605D9DBA0
结果: 干净编译及五版本离线唯一匹配通过；20.2.0–22.5.0 FAT32 实机待验证
```

只替换外置 `fs_codecvt_unpacked.kip` 不需要重新编译 package3；修改 Fusee 的
Overlay 加载或 Package2 重建逻辑后，才需要重新构建 Fusee/package3 并更新
启动 payload。其余固件版本以及启用 emuMMC 的组合仍需分别实机验证。
