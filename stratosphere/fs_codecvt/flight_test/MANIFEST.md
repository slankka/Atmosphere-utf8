# Flight Test Manifest

## Flight #1 — 2026-07-19

**文件**: `fs_codecvt_codecvt_only_unpacked.kip`
**用途**: 21.2.0 exFAT codecvt-only 测试（目录 Hook 已禁用）
**部署**: 复制到 `sd:/atmosphere/kips/` 并重命名为 `fs_codecvt_unpacked.kip`

### 文件指纹

| 属性 | 值 |
|------|-----|
| **MD5** | `11D3025B945E3D925F9BCB59025C246E` |
| **SHA256** | `7ACE70D256611B8C763620656FB5C4806E5974180C589465C4CBD68D17C58F17` |
| 大小 | 6772 bytes |

### 关键变更

- 6 个 codecvt offset 修正为实际 21.2.0 exFAT 反汇编值：
  - OEM→Unicode: `0x10ECC0`
  - Unicode→OEM: `0x10EE10`
  - OEM char width: `0x10EFE0`
  - Is OEM MB: `0x10F020`
  - Unicode width: `0x10F070`
  - Is Unicode MB: `0x10F080`
- Entry 签名: `ldrsb w8,[x0]` / `and w11,w8,#0xff` (`0x39C00008`, `0x12001D0B`)
- 目录 Hook: **禁用**（`dir_hook=0`，21.2 控制流与 22.x 不同，需重新设计）
- Sanitize offsets 保持原值（`0x105F14, 0x1061C8, 0x106D24`）

### 预期结果

- ✅ 21.2.0 正常启动（不黑屏）
- ✅ UTF-8 文件名读写正常（6 个 codecvt Hook 生效）
- ⚠️ 目录 Hook 未激活（等后续重新设计）

### 测试结果 — 2026-07-19

| 项目 | 结果 | 说明 |
|------|------|------|
| 系统启动 | ✅ PASS | 21.2.0 正常启动，不黑屏 |
| direct read/write | ✅ PASS | CJK 文件创建/写入/读回成功，6 个 codecvt Hook 正确 |
| /ROM lists CJK dir | ❌ FAIL | 预期：dir_hook=0，目录 Hook 已禁用 |
| CJK dir lists file | ❌ FAIL | 预期：同上，目录枚举依赖 dir Hook |

**结论**: codecvt offset 修正完全正确。2 个 FAIL 均由 `dir_hook=0` 导致，属预期行为。
下一步：针对 21.2.0 的 W10/X8/X28/W20 控制流重新设计目录 bridge。

---

## Flight #2 — 2026-07-19

**文件**: `fs_codecvt_full_unpacked.kip`
**用途**: 21.2.0 exFAT 完整测试（codecvt + dir hook + sanitize）
**部署**: 复制到 `sd:/atmosphere/kips/` 并重命名为 `fs_codecvt_unpacked.kip`

### 文件指纹

| 属性 | 值 |
|------|-----|
| **MD5** | `F7C26D61ED05CBD99306E65BC1D04B3C` |
| **SHA256** | `BBE324174AB7F0B8AD9D7E61D5619BE6D28E03BBFDCF844F910C832BFD7F5DA5` |
| 大小 | 6804 bytes |

### 关键变更（相对于 Flight #1）

- 目录 Hook **已启用**，capstone 精确定位四个偏移：
  - `dir_hook` = `0xE48D0` (b.lo #0xe4864 → 替换为 B cave)
  - `dir_reject` = `0xE4864` (cmp x24, x21)
  - `dir_ascii_checks` = `0xE48D4` (cmp w10, #0x5c)
  - `dir_scan_continue` = `0xE4914` (cmp x9, x8)
- `dir_hook_opcode` = `0x54FFFCA3`
- 新增 `byte_reg=10` 字段，trampoline 中 `mov w0, w10` 替代硬编码 `mov w0, w9`
- `name_reg=28` (X28), `bound_reg=9` (X9)

### 预期结果

- ✅ 21.2.0 正常启动
- ✅ direct read/write PASS（codecvt 已验证）
- 🎯 /ROM lists CJK dir → 目标 PASS
- 🎯 CJK dir lists file → 目标 PASS

### 测试结果 — 2026-07-19

| 项目 | 结果 | 说明 |
|------|------|------|
| 系统启动 | ✅ PASS | |
| direct read/write | ✅ PASS | |
| /ROM lists CJK dir | ❌ FAIL | 与 Flight #1 相同 |
| CJK dir lists file | ❌ FAIL | |

**根因**: W10 寄存器冲突。21.2.0 的文件名字节在 W10，但 trampoline 也把 validator 返回值（action）写入 X10，覆盖了原始字节。跳到 `dir_ascii_checks` (`cmp w10, #0x5c`) 时比较的是 action 值 (0/1/2) 而非文件名字节。22.5.0 没这个问题因为字节在 W9。

---

## Flight #3 — 2026-07-19

**文件**: `fs_codecvt_full_v2_unpacked.kip`
**用途**: 21.2.0 exFAT 完整测试 — 修复 W10/X11 寄存器冲突
**部署**: 复制到 `sd:/atmosphere/kips/` 并重命名为 `fs_codecvt_unpacked.kip`

### 文件指纹

| 属性 | 值 |
|------|-----|
| **MD5** | `88A747C7666C5246B1740374D434DD1B` |
| **SHA256** | `DA4F78E3FB9EAE68EF42BA94C372A005766DB0FB1B03C57517AA7DD4B546F2C1` |
| 大小 | 6804 bytes |

### 关键变更（相对于 Flight #2）

- **修复 W10 寄存器冲突**：trampoline 中 action 改用 X11
  - `ldr x10, [sp, #0xA8]` → `ldr x11, [sp, #0xA8]`
  - `cmp w10, #0` → `cmp w11, #0`
  - `cmp w10, #1` → `cmp w11, #1`
- 其余偏移与 Flight #2 相同

### 预期结果

- ✅ 21.2.0 正常启动
- ✅ direct read/write PASS
- 🎯 /ROM lists CJK dir → 目标 PASS
- 🎯 CJK dir lists file → 目标 PASS

### 测试结果

> 待真机测试

---

## Flight #4 — 2026-07-19 (sanitize 排除测试)

**文件**: `fs_codecvt_nosanitize_unpacked.kip`
**用途**: 排除 sanitize NOP 干扰，仅 codecvt + dir hook
**部署**: 复制到 `sd:/atmosphere/kips/` 并重命名为 `fs_codecvt_unpacked.kip`

### 文件指纹

| 属性 | 值 |
|------|-----|
| **SHA256** | `A16ED10FD8885DAECF755222D6681A6BEAB41C32623209223B4CF0D528A230D7` |
| 大小 | 6804 bytes |

### 关键变更（相对于 Flight #3）

- **sanitize 设为 {0,0,0}**：21.2.0 的 TBNZ bit 1 不是 ASCII 过滤器，NOP 它们可能破坏 FS 功能
- codecvt + dir hook 与 Flight #3 相同（W10/X11 修复保留）

### 预期结果

- ✅ 系统启动
- ✅ direct read/write PASS
- 🎯 /ROM lists CJK dir → 目标 PASS
- 🎯 CJK dir lists file → 目标 PASS

### 测试结果

> A16ED10F: 1P 2F 3F — sanitize 排除无影响

---

## Flight #5 — 2026-07-19 (scan_continue 偏移修正)

**文件**: `fs_codecvt_sc_ldrb_unpacked.kip`
**SHA256**: `6377D0E82E522ABC14ACC12AEED19A387CC3F5A00C01B5A528E6F6AA53CF5714`

**关键变更**: `dir_scan_continue` 从 `0xE4914` 改为 `0xE48C0`

**根因**: validator 返回 `X0 = index + N`（N=2,3,4），原 scan_continue 处 `add x8,x8,#1` 导致 X8 多跳 1 字节。改为直接跳 `ldrb w10,[x28,x8]`，空终止符由 `cbz` 处理。

### 测试结果

> **6377D0E8**: ✅ **3 PASS** — 21.2.0 exFAT 完整验证通过！

---

## 21.2.0 exFAT 调试总结

| Bug | 症状 | 修复 |
|-----|------|------|
| codecvt offset 用错 | 启动黑屏 | 修正为 `0x10ECC0` 系列 |
| W10 被 trampoline action 覆盖 | dir hook 无效果 | action 改用 X11 |
| scan_continue `add x8,#1` | UTF-8 字节错位 | 指向 `ldrb` (`0xE48C0`) |

当前 21.2.0 exFAT 完整偏移：codecvt `0x10ECC0~0x10F080`，sanitize 跳过，
dir `0xE48D0/0xE4864/0xE48D4/0xE48C0`，reg 28/9/10。
