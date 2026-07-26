# Flight Test Manifest — 19.0.1 FAT32

| # | SHA256(前8) | 变更 | 结果 |
|---|-------------|------|------|
| 1 | `25915F8C` | 新增 FAT32 表, cave=0xECFC0 | ❌ 黑屏 (cave 错误) |
| 2 | `BF6C71DF` | cave→0xECF44 | ❌ 黑屏 (find_fs 页扫描误匹配) |
| 3 | `47138B09` | 空 KIP (`__init` return) | ✅ 3 FAIL (系统正常) |
| 4 | `7A906D8D` | 移除 find_fs 页扫描 | ✅ 3 FAIL (matches_fs 不匹配) |
| 5 | `9B617D7E` | 绕过 svcQueryMemory, fixed 2MB | ✅ 3 FAIL (matches_fs 不匹配) |
| 6 | `0FA3CCED` | 硬编码 FAT32 | ❌ 黑屏 (**fs_code_base 未赋值!**) |
| 7 | `0C7EF985` | 硬编码 + sanitize=0 | ❌ 黑屏 |
| 8 | `5860EA39` | 硬编码 + dir_hook=0 | ❌ 黑屏 |
| 9 | `83FF45A6` | 硬编码 + 仅 codecvt[0] | ❌ 黑屏 |
| 10 | `26F2A537` | 修复 fs_code_base + 运行时自检 | ✅ 3 FAIL (matches_fs 不匹配) |
| 11 | `162793FC` | 硬编码 FAT32 (fs_code_base 已修复) | ❌ 黑屏 |
| 12 | `8D6DD57A` | svcQueryMemory 取实际 size | ✅ 开机，3 FAIL（检查的是 emuMMC 基址） |
| 13 | `7AC37D88` | `&_start + 0x8000` 固定基址 | ✅ 开机，3 FAIL（同样检查的是 emuMMC 基址） |
| 14 | `25E677DC` | 从 `&_start` 查询区域后扫描 FS | ⚠️ 静态审查否决：Overlay 权限已分段，查询上界可能过早 |
| 15 | `39489F25` | 从 `__argdata__` 查询 `[emuMMC][FS]` RX 区域，再严格扫描 FS | ⚠️ 能开机；运行 codecvt_test 后黑屏假死，需强制关机 |
| 16 | `AAE22B05` | F15 定位逻辑；仅安装六个 codecvt Hook，不改 directory/sanitize | ⚠️ 运行 codecvt_test 后黑屏 |
| 17 | `8EB28235` | F15 定位逻辑；仅安装 codecvt[0] OEM→Unicode Hook | ✅ 不黑屏，3 FAIL |
| 18 | `BE192056` | F15 定位逻辑；仅安装 codecvt[1] Unicode→OEM Hook | ✅ 不黑屏，3 FAIL |
| 19 | `C2C029CD` | F15 定位逻辑；仅写入 codecvt[0+1] 双向主转换 Hook | ✅ 不黑屏，3 FAIL |
| 20 | `04939678` | F15 定位逻辑；写入 codecvt[0+1+2+3]，不改 directory/sanitize | ⚠️ 能开机；运行 codecvt_test 后黑屏 |
| 21 | `53A93026` | F15 定位逻辑；写入 codecvt[0+1+2]，不改 slot3/directory/sanitize | ✅ 不黑屏，3 FAIL |
| 22 | `CABBE9C4` | F15 定位逻辑；仅写入 codecvt[3] `is_oem_mb_char`，隔离 slot3 本身与组合效应 | ✅ 不黑屏，3 FAIL |
| 23 | `B8AD47BA` | F15 定位逻辑；写入 codecvt[0+1+3]，去掉 slot2 以判断主转换与 slot3 的交互 | ⚠️ 能开机；运行 codecvt_test 后黑屏 |
| 24 | `A1DDB415` | F15 定位逻辑；仅写入 codecvt[0+3]，隔离 OEM→Unicode 与 `is_oem_mb_char` 的交互 | ⚠️ 能开机；运行 codecvt_test 后黑屏 |
| 25 | `126873F2` | slot0+3；slot0 的三字节分支限制为最多读取/消费 2 字节，验证 PrFILE2 DBCS 临时缓冲越界假说 | ✅ 不黑屏；direct read/write PASS，两个枚举 FAIL |
| 26 | `98F63673` | 完整 codecvt 但保留原生 slot3；启用三个高位字节放行点和严格目录 UTF-8 Hook | ✅ 不黑屏，3 FAIL |
| 27 | `F6758BAF` | 完整 codecvt+辅助 Hook；slot3 改为恒 false，使 PrFILE2 按单字节流处理 UTF-8 | ⚠️ 能开机；运行 codecvt_test 后黑屏假死 |
| 28 | `F16107EA` | #27 基础上完整读取/解码三字节，但返回 OEM width=2，区分第三字节读取与 width=3 | ⚠️ 能开机；运行 codecvt_test 后黑屏 |
| 29 | `0AE6160E` | 不替换六槽 codecvt；Hook `transformFromUnicodeToNormal@FA410` 与 `transformInUnicode@FA530`，仅加三个放行点，不装目录 Hook | ✅ 不黑屏，3 FAIL |
| 30 | `6F97CCF4` | #29 + Hook `GetNextCharOfPattern@F9190`，长名模式直接从完整 UTF-8 源串解码并按真实宽度推进 | ✅ 不黑屏，3 FAIL |
| 31 | `AF431074` | #30 + 在 `GetNextTokenOfPath@F9938` 绕过固定两字节 DBCS 分支，让 UTF-8 路径按合法高位字节逐字节分词 | ✅ 能启动，3 FAIL |
| 32 | `CDC606BB` | #31 + 仅启用 codecvt slot3，使通用 `PF_STR` 字符步进使用 UTF-8 首/续字节分类；仍不启用危险的全局 slot0 | ✅ 能启动，3 FAIL |
| 33 | `93BF586A` | #32 + 定点替换 `parseShortName@FA9F0` 的安全 slot0 调用；不改两个使用两字节栈缓冲的调用点 | ✅ 清空 `/ROM` 后复测：首次 create 仍为 `0x202`，3 FAIL |
| 34 | `2B14BBCD` | #33 + 仅替换 mkdir 内 `SplitPath` 调用 `F5D5C`，按 UTF-8 字节明确拆出父路径与末级名称 | ❌ 启动正常；空 `/ROM` 下首个 mkdir 仍为 `0x202`，三个 FAIL |
| 35 | `C212C86F` | #33 + 在 mkdir 的父目录 `GetEntryOfPath` 返回点添加 `+0x100` 错误来源标签；不启用 #34 SplitPath 替换 | ❌ 仍为 `0x202`，标签未触发，三个 FAIL |
| 36 | `D9DE588B` | #33 + 将 mkdir 本地名称校验共用的固定 `return 2` 改为 `return 3`，标记 `0xF5EB0` | ⛔ 已撤回；PrFILE2→HOS 错误映射使该标签方法不能可靠定位 |
| 37 | `95576682` | 干净 FAT32 高层方案：不改六槽/slot3/tokenizer；Hook 两个完整字符串转换、长名 pattern 和完整 `parseShortName@FA5D0`；不装目录 Hook | ❌ 空 `/ROM` 下首个 mkdir 仍为 `0x202`，三个 FAIL |
| 38 | `85613EF1` | 纯路径阶段探针：仅把目标 mkdir 的 `SplitPath` 输出改成父目录 `/ROM` + ASCII 名 `UTF8TEST`，其他 mkdir 转发原函数；不装任何 codecvt/目录 Hook | ❌ 与此前相同，直接操作仍全部为 `0x202`，三个 FAIL；完整路径匹配未产生可观察改写 |
| 39 | `73FB8BFD` | 放宽后的命中探针：只检查输入路径头部 ASCII `/ROM/`，不读取 `tail`、长度、`code_mode` 或中文字节；命中后改成 `/ROM/UTF8PFX` | ❌ 直接操作仍全部为 `0x202`，且 `/ROM` 没有 `UTF8PFX`；没有观察到该调用点改写 |
| 40 | `122D013A` | 双通道生效探针：#39 的 `/ROM/`→`UTF8PFX` mkdir 改写，加上已在 #25 实机证明有效的 slot0+3/两字节安全转换 | ❌ 仍为三个 FAIL、直接操作均 `0x202`，空 `/ROM` 没有 `UTF8PFX`；两个生效通道都未触发 |
| 41 | `53993A5D` | 匹配器回归探针：恢复 #25 的最小 FS 身份校验，仅安装 #25 已验证的 slot0+3/两字节安全转换；不安装 mkdir 或其他新 Hook | ✅ direct read/write PASS；创建并列出 `________`，两个枚举 FAIL；证明扩展安装条件导致 F29–F40 静默退出，但尚未定位具体原因 |
| 42 | `30029EA1` | 首个不匹配项编码探针：最小条件识别 FS，逐项检查 10 个新增 opcode；slot0 将首个失败序号编码成重复 ASCII 字母目录 | ✅ direct read/write PASS；列出 `AAAAAAAA` 与 `AAAAAAAA.txt`（测试输出将后者标为 dir），两个枚举 FAIL；诊断值为 0，说明所选表项没有发生任何非零扩展检查失败 |
| 43 | `95C73F00` | 运行时 FS 表项编号探针：把 `find_fs()` 选中的索引直接编码成重复 ASCII 字母；其余保持 F41/F42 的 slot0+3 安全行为 | ✅ 列出 `AAAAAAAA`；确认 `find_fs()` 实际选择索引 0，即 19.x ExFAT-capable FS，而不是索引 5 的 FAT-only FS |
| 44 | `D04E1A63` | 首次针对实际 19.x ExFAT-capable FS 填写高层 FAT32 offsets；只 Hook 完整字符串双向转换、长名 pattern 与完整 `parseShortName`，不全局替换危险的 codecvt slot0 | ❌ 不黑屏，但首次 mkdir 仍为 `0x202`，空 `/ROM` 下三个 FAIL；证明高层四入口单独不足以覆盖首错路径 |
| 45 | `B5238EDD` | 双契约组合：F44 高层完整 UTF-8 路径 + F25 已验证的 slot0/slot3；slot0 改用独立两字节安全解码器，高层入口继续使用完整三字节解码器 | ⚠️ 不黑屏；direct read/write 全部 PASS，但两个目录均 `total=0`、枚举 FAIL；确认双契约分流解决输入路径，剩余故障收敛到目录输出 |
| 46 | `87A512C2` | F45 + 仅启用现有 `Directory::Read` UTF-8 输出 Hook；仍不启用三个输入 sanitize NOP | ✅ 三项全部 PASS：direct read/write、`/ROM` 枚举 CJK 目录、CJK 目录枚举文件均成功；FAT32 双契约方案验证完成 |

## 根因分析

### 已确认
- **所有 offset 已验证正确**：10 个 offset 在 FS.nso 文件中全部精确匹配
- **B 指令编码正确**：capstone 验证 FAT32/exFAT 的 B 指令均跳转到正确目标
- **空 KIP 正常**：KIP 注入本身在 FAT32 上没问题
- **`&__argdata__` 的含义**：它等于 `&_start + 0x8000`，但只指向 Overlay
  后的下一个映像；启用 emuMMC 时这里是 emuMMC，不是 FS。
- **当前启动配置启用了 `emummcforce=1`**：Fusee 的实际 RX 布局是
  `[fs_codecvt 0x8000][emuMMC 0x59000][FS]`，因此 FS 基址不是
  `&_start + 0x8000`，而是当前构建下的 `&_start + 0x61000`。
- **#12 的 3 FAIL 是静默未安装 Hook**：它在 emuMMC 基址执行
  `matches_fs()`，不匹配后直接返回，随后系统正常启动。
- **实际 package3 的 emuMMC 已核对**：`G:\atmosphere\package3` 在
  `0x100000` 内嵌的 KIP 与本地 `emummc_unpacked.kip` 逐字节相同，运行时
  占用大小为 `0x59000`。
- **#15 的离线扫描已核对**：使用真实 emuMMC 段内容和 19.0.1 FAT32
  `FS_proper.nso` 重建 `[fs_codecvt][emuMMC][FS]` 后，六个版本表中仅有
  `FsVer_19_0_0_Fat32 @ 0x61000` 一个匹配。

### 静态审查结论

- #14 的“跳过 emuMMC 扫描 FS”方向成立，但它从 `&_start` 查询内存区域。
  `start.s` 在调用 `__init()` 前已经重新划分 Overlay 自身的内存权限，故该
  查询可能只覆盖 Overlay 的首个子区域，扫描上界不可靠。
- #15 改为从 `__argdata__` 查询尚未重新分段的 `[emuMMC][FS]` RX 区域；
  再按页扫描并以 codecvt 入口、directory hook 和三个 sanitize 点共同确认
  FS 基址。此方案已通过离线布局与编译后反汇编检查，仍需实机验证。

### 未解决
- **硬编码 FAT32 黑屏**（#6, #11）：offset 正确、B 指令正确、地址计算正确，但仍黑屏
- **#15 已越过基址识别阶段**：系统能启动，但 codecvt_test 首次触发转换路径后
  黑屏假死；需要按测试调用顺序隔离六个 codecvt Hook。
- **#20/#21 将触发因素收敛到 slot3**：codecvt[0+1+2] 不黑屏，加入
  codecvt[3] 后黑屏。
- **#22 证明 slot3 单独安全**：它自身不会令测试程序黑屏，因此 #20 是 slot3
  与 slot0–2 中至少一个 Hook 的组合效应。
- **#23 排除 slot2**：slot0+1+3 仍黑屏，所以 `oem_char_width` 不是必要触发因素。
- **#24 确认最小黑屏组合为 slot0+3**：OEM→Unicode 与路径字节分类器同时
  替换即触发，slot1/slot2 均非必要条件。
- **#25 是根因探针，不是修复候选**：TFP2 显示部分调用点只把一个 OEM 字符
  暂存在 2 字节缓冲中；原 UTF-8 slot0 遇到 CJK 会读取第 3 字节。#25 对三字节
  序列只读两字节并返回 U+FFFD。实机不再黑屏且 direct read/write PASS，确认
  黑屏与三字节读取/DBCS 暂存契约冲突直接相关；两个枚举 FAIL 符合替代字符
  无法与原 UTF-8 名称精确匹配的预期。
- **#26 改变设计方向**：不再尝试用 DBCS `is_oem_mb_char` 接口表达三字节
  UTF-8；保留 FS 原生 slot3，并由三个高位字节放行点避免逐字节路径检查拒绝，
  完整 slot0/1/2/4/5 负责转换，Directory::Read Hook 负责输出名称的严格 UTF-8
  校验。该组合用于验证能否同时避免双字节暂存冲突并恢复三个功能测试。
- **#26 证明保留 CP932 slot3 仍会破坏 UTF-8 边界**：虽然不黑屏，但三个测试
  全部 FAIL。#27 将 slot3 改为始终返回 false，使 PrFILE2 的 SBCS/DBCS 层不再
  对 UTF-8 字节配对；高位字节放行、完整转换和目录 UTF-8 Hook 保持启用。
- **#27 否定“仅禁用 DBCS 配对即可修复”**：slot3 恒 false 仍黑屏，说明 slot0
  的三字节行为还有独立的不兼容。#28 保留完整三字节读取和解码，但将返回的
  OEM 宽度限制为 2，用于区分 `src[2]` 读取与 `oem_width == 3` 哪一个直接触发。
- **#28 排除 `oem_width == 3` 是直接原因**：完整读取第三字节但返回 width=2
  仍黑屏；结合 #25，直接触发条件收敛为部分 PrFILE2 调用点向 slot0 只提供
  两字节临时字符缓冲。后续不再全局替换 slot0，转向 Hook 明确接收完整字符串
  的 `VFiPFPATH_transformInUnicode` 高层入口。
- **#29/#30 证明已定位的三个高层函数仍未覆盖 direct 测试的首个失败路径**：
  `transformFromUnicodeToNormal`、`transformInUnicode` 和
  `GetNextCharOfPattern` 同时 Hook 后仍为三个 FAIL。下一步必须依据测试程序逐项
  Result 码定位首次失败的 FS API，避免继续按最终摘要盲目扩展 Hook。
- **#30 的逐项输出已定位首个失败点**：`CreateDirectory` 对已存在的 `/ROM`
  下级 CJK 路径立即返回 `0x202`（`fs::ResultPathNotFound`）；后续 stat、文件创建、
  写入和目录打开均为同一结果，属于首错后的连锁失败，不是目录枚举阶段丢名。
- **实体代码已确认路径分词器仍使用固定两字节 DBCS 契约**：FAT FS 的
  `0xF9944`/`0xF9964` 连续调用 `is_oem_mb_char(byte, 1/2)`，与 TFP2
  `VFiPFPATH_GetNextTokenOfPath` 源码逐项吻合。`0xF9938` 的条件分支决定 mode=1
  是否进入该逻辑；普通分支本身会放行所有 `>=0x80` 字节，因此 #31 仅将该处
  改成无条件跳往普通逐字节路径，作为单变量验证。
- **#31 首次交付的 `AB2A354E` 不是有效实验结果**：误保存了压缩中间产物
  `fs_codecvt.kip`，其 KIP flags 为 `0x7F`（低三位压缩标志非零），Fusee 在
  注入前按设计报 `Invalid FS overlay kip`。已用正确的 `fs_codecvt_unpacked.kip`
  覆盖为 `AF431074`；flags=`0x78`、文件大小与 KIP 三段之和均为 `6308`，且
  Overlay 末端 `bss_address+bss_size=0x7000` 满足 `0x1000` 对齐要求。
- **#31 否定“固定两字节路径分词是首错的充分原因”**：正确的未压缩版本
  `AF431074` 能正常启动，但测试仍为 3 FAIL。该分支确实与 UTF-8 不兼容，
  但单独绕过它并未恢复 direct create/read/write；后续重新沿实际
  `CreateDirectory` 调用链定位，不再假定首个 `0x202` 只由该分支产生。
- **#31 的逐项输出与 #30 完全相同**：首次 `CreateDirectory`、随后 stat、文件
  创建和写打开仍全部返回 `0x202`；`/ROM` 自身仍能列出。复核 CP932 范围后可知，
  UTF-8 的部分 `E4 B8`、`E6 96` 等组合本来就会被误当成合法双字节字符，剩余
  高位字节又被普通分支放行，因此 #31 的逐字节绕过可能与原路径实际等价。
- **#32 改查通用 `PF_STR` 字符步进**：#25 已证明 slot0+slot3 的安全探针能令
  direct 测试通过，而 #31 只处理了一个 tokenizer 分支，未覆盖 `StrLen`、
  `MoveStrPos`、`StrNCmp` 等共享的 slot3 调用。#32 只在 #31 上增加 slot3，继续
  禁用全局 slot0，避免已确认的两字节临时缓冲越界组合。
- **#32 排除“只缺通用 slot3 字符步进”**：仅增加全局 slot3 后仍为三个 FAIL；
  因而 #25 的 direct PASS 还依赖 slot0 转换。下一步枚举 slot0 的实际调用点，
  只替换输入来自完整字符串的安全调用路径，避开两字节临时缓冲。
- **#33 选择首个安全 slot0 调用点**：FAT 二进制的 slot0 间接调用中，
  `0xF92B4`/`0xF93A8` 使用两字节栈临时缓冲，`0xFA570` 已被高层 Hook 覆盖；
  `0xFA9F0` 位于 `VFiPFPATH_parseShortName`，X0 指向完整 filename 当前位置，
  可安全读取三字节 UTF-8。#33 仅把该处 `ldr+blr` 改为直接 `bl UTF-8 + nop`。
- **#33 暴露测试介质已被早期探针污染**：逐项输出仍是 create/stat/create-file/
  open-write 全部 `0x202`，但 `/ROM` 当前包含唯一目录 `________`。#25 曾出现
  direct read/write PASS、两个枚举 FAIL，证明它能够用错误转换创建并回读对象，
  却不能按原 UTF-8 名称枚举；该异常目录极可能就是其遗留物。它会同时引入
  LFN 不匹配和 SFN 冲突，使 #26–#33 的创建结果不再是干净的单变量实验。
  在制作 #34 前，应先从 FAT32 卡删除 `/ROM/________`（以及其中测试文件），
  保留空的 `/ROM`，然后原样重跑 #33。
- **#33 清理介质后的复测排除残留目录干扰**：`/ROM` 已确认 `total=0`，但
  create/stat/create-file/open-write 仍全部为 `0x202`。因此 `0xFA9F0` 定点
  slot0 Hook 无效，且失败发生在写入任何新目录项之前。下一步转向整体替换
  `VFiPFPATH_SplitPath`，直接按 UTF-8 字节拆分父路径与末级名称。
- **#34 将 Hook 限制在 mkdir 调用点**：FAT `SplitPath` 公共入口为 `0xF9B00`，
  五个调用者中 `0xF5D5C` 与后续 `0xF6144 -> transformInUnicode` 同属
  `VFiPFDIR_p_mkdir`。#34 只改该 BL，避免影响其他启动期文件操作；对 mode=1
  路径按最后一个 `/` 拆分，使 `/ROM/中文目录` 明确得到父路径 `/ROM` 和末级
  UTF-8 名称 `中文目录`。
- **#34 排除 mkdir 内 `SplitPath` 是首错的决定点**：在已确认空的 `/ROM` 上，
  `CreateDirectory` 仍立即返回 `0x202`，逐项输出与 #33 完全一致。说明强制生成
  正确的父路径/末级名称仍不足以进入目录项创建；下一步追踪 `SplitPath` 之后
  各子调用的返回值与错误映射，定位实际产生 `PathNotFound` 的阶段。
- **#35 标记父目录查找的失败来源**：`0xF5DE4` 调用
  `VFiPFENT_ITER_GetEntryOfPath(iter, entry_dir, volume, dir_path, true)`，随后
  `0xF5DF4` 将 W0 保存到最终返回寄存器 W21。诊断版仅把该保存改为
  `add w21, w0, #0x100`；后续 `cbnz w0` 的控制流不变。若首个错误码改变，说明
  `0x202` 来源是父目录 `/ROM` 的解析/查找；若仍为 `0x202`，则来源位于之后
  的 filename 校验路径。
- **#35 标签未触发**：首个错误仍为 `0x202`，说明父目录 `GetEntryOfPath`
  的非零返回路径不是本次可观察到的首错。#36 改标记 mkdir 内多个名称检查汇合的
  固定 `return 2` 出口 `0xF5EB0`，用于区分本地校验与更深层子调用错误。
- **重新审视后撤回 #36 错误标签路线**：PrFILE2 内部错误会经过上层转换，不能
  假定修改内部值后测试程序必然显示不同的 Horizon Result；因此 #35 的结果也
  不能严格证明父目录查找成功。后续不再用最终 `0x202` 反推单个内部出口。
- **FAT32 与 exFAT 的关键差异是 SFN**：FAT32 的
  `VFiPFPATH_parseShortName@0xFA5D0` 会调用 OEM width，并按两字节块构造必须
  存在的 8.3 短别名；三字节 UTF-8 与该循环的 DBCS 契约不兼容。exFAT 不走
  同一传统 SFN 生成分支，因此过去在 exFAT 上验证的六槽方案不能直接外推到
  FAT32。#25 的“限读两字节后 direct PASS、磁盘留下下划线名称”应解释为有损
  名称能够自洽访问，而非正确 UTF-8 已接近完成。
- **#37 将 LFN 与 SFN 分流**：原生六个 CP932 回调全部保留，避免任何两字节
  临时缓冲读取第三字节。完整 UTF-8 字符串仅由 `transformInUnicode@FA530`、
  `transformFromUnicodeToNormal@FA410` 和 `GetNextCharOfPattern@F9190` 处理；
  新的完整 `parseShortName@FA5D0` Hook 为非 8.3/CJK 名称生成 ASCII
  `base~1.ext` 种子，再由原 `VFiPFENT_AdjustSFN` 处理碰撞。#37 不安装目录
  Hook，因为旧 trampoline cave 位于仍在使用的原生 `unicode2oem` 函数体内；
  首轮只验证 direct read/write，若成功再把目录 trampoline 移到 Overlay 自身。
- **#37 说明首错早于 SFN 生成，不能否定最终分流设计**：实体 xref 显示
  `parseShortName@FA5D0` 仅由 `0xFE8E8` 调用，而 mkdir 在到达这里之前仍需完成
  SplitPath、父目录遍历和已有目录项搜索。#37 第一次 create 即 `0x202`，因此
  新 SFN Hook 尚不能成为可观察结果。#38 不再依赖错误映射：只对测试目标路径
  将拆分结果替换为 `/ROM` 与 `UTF8TEST`，以实际创建的 ASCII 目录作为阶段证据。
- **#38 没有解决 Hook 命中歧义**：调用点和 AAPCS64 参数已经由实体反汇编确认，
  测试程序常量也确认为 UTF-8 `/ROM/中文目录`，但 #38 仍依赖 `tail-head`、
  `code_mode` 和完整中文字节比较。#39 仅比较 `head` 指向的 ASCII `/ROM/`
  前缀，既限制在测试目录命名空间，也去掉这些剩余假设。
- **#39 把问题提升到运行时补丁是否落地**：去掉上述匹配条件后，直接操作仍全部
  为 `0x202`，且没有创建 `UTF8PFX`。这不再支持继续修改 SplitPath 输出内容；
  但单独的 #39 仍无法区分“整个 `install()` 未提交”和“只有 `0xF5D5C` 并非
  实际执行调用点”。#40 同时加入 #25 已经实机产生 direct PASS/下划线目录的
  已知生效通道，用一次测试把这两个分支分开。
- **#40 证明当前构建没有提交任何 Hook**：两个互相独立的行为通道都没有出现。
  对 F25 与 F40 未压缩 KIP 的实体反汇编显示，两者启动、`svcQueryMemory`、映射
  和缓存刷新流程一致；关键差异是扩展后的 `install()` 要求路径转换、pattern、
  tokenizer、shortname 和 mkdir 等新增 offset 全部非零；任一必要字段为零都会在
  写入 Hook 前退出。#41 回到 F25 的安装条件和同一组已知有效 Hook，专门验证
  这一回归。
- **#41 证明 F29–F40 的扩展安装条件导致静默失效，但不能单独证明 opcode 不匹配**：
  恢复旧条件后 direct read/write 立即恢复 PASS，并重新创建 `________`。因此这些
  阴性结果不能用于否定对应 Hook 设计。#42 在接受旧身份条件后继续检查十个新增
  opcode，但忽略未填写（值为零）的字段，并把首个失败序号编码为目录名。
- **#42 的 `AAAAAAAA` 否定了“FAT-only 表中某个新增 opcode 不匹配”这一解释**：
  诊断值 0 表示所选记录的所有非零扩展检查都通过。更关键的是，19.x ExFAT-capable
  记录的十个扩展字段目前全部为零，而 19.x FAT-only 记录十项均已填写；因此该结果
  强烈表明运行时命中了索引 0 的 `FsVer_19_0_0_Exfat`。SD 卡格式为 FAT32 并不决定
  系统使用 FAT-only FS 模块：安装了 exFAT 支持的固件仍可用同一 FS 模块访问 FAT32
  介质。这样也统一解释了 F29–F40：它们在 ExFAT 记录的零字段处提前退出，连 F40
  的已知有效 codecvt 通道也未安装。#43 直接输出 `find_fs()` 表项索引作最终确认；
  若仍为 `A`，下一步必须从实际 19.x ExFAT FS 二进制提取并填写高层 Hook offsets，
  而不是继续使用 FAT-only 二进制的地址。
- **#43 已直接确认运行时 FS 变体**：目录显示 `AAAAAAAA`，即 `find_fs()` 返回
  索引 0 `FsVer_19_0_0_Exfat`。因此 F29–F40 的失败不是这些 Hook 设计本身的
  实机否定，而是扩展 offset 仅填写在索引 5 的 FAT-only 记录；索引 0 中对应字段
  全为零，`install()` 在写入任何 Hook 前退出。后续所有高层 offset 和 opcode
  必须从 19.x ExFAT-capable FS 实体重新定位。
- **#44 排除“只补齐高层完整字符串入口即可越过首错”**：在实际命中的索引 0
  中填写并校验 ExFAT-capable offsets 后，系统和测试程序均不黑屏，但空 `/ROM`
  下首次 `CreateDirectory` 仍返回 `0x202`。结合 #25/#41 的 slot0+slot3 两字节
  安全组合可令 direct read/write PASS，可知首次创建路径仍依赖旧 PF_CHARCODE
  调用链。#45 将两个调用契约分开：旧接口使用最多读取两字节的有损安全解码器，
  完整字符串入口继续使用真正的三字节 UTF-8 解码器，以免重新引入 #24 的越界
  黑屏。
- **#45 确认双契约分流解决了 FAT32 输入路径，但暴露了独立的目录输出问题**：
  mkdir、stat、文件创建、写入、提交、按原 CJK 路径回读和类型查询全部成功，说明
  高层完整 UTF-8 与旧接口两字节安全转换必须同时存在。与此同时，`/ROM` 和 CJK
  子目录均返回 `total=0`，不是 F25 的下划线条目。直接按 CJK 路径仍能查回对象，
  说明 LFN/SFN 链足以支持名称查找；更符合证据的是：F45 首次向 FS 外层输出真实
  UTF-8 高位字节，而高层模式此前刻意跳过了 `Directory::Read` UTF-8 Hook，条目在
  输出校验阶段被过滤。#46 只增加该目录 Hook，不增加三个输入 sanitize NOP，以
  单变量验证这个阶段。
- **#46 完成 FAT32 全链路验证**：加入 `Directory::Read` UTF-8 输出 Hook 后，三项
  测试全部 PASS。最终必需组合为：旧 PF_CHARCODE slot0 使用最多两字节的安全有损
  解码器、slot3 保持字节分类；拥有完整字符串的高层入口使用真正三字节 UTF-8；
  FAT32 `parseShortName` 生成 ASCII SFN；目录输出 Hook 接受并推进合法 UTF-8。
  三个输入 sanitize NOP 不需要启用。普通构建现通过
  `FS_CODECVT_FAT32_DUAL_CONTRACT` 选择与 F46 相同的正式配置；执行干净的普通
  `make -j8` 后，根目录未压缩 KIP 与 F46 均为 SHA-256 `87A512C2...`、大小
  10476 字节，已确认构建产物等价。

### `is_oem_mb_char` 调用约定复核

- PrFILE2 源码确认 `num=1` 检查首字节，`num=2` 检查第二字节；现有 UTF-8
  实现的模式次序没有写反。
- 路径扫描器在首字节命中后只额外消费一个字节，原生设计针对 CP932 这类
  双字节编码；它不能单独完整表达 UTF-8 的 3/4 字节字符。这是后续设计必须
  处理的结构限制，但尚不能解释 #20 是否由 slot3 单独触发，因此先执行 #22。

### 假说
- 若 #14 黑屏，再分别隔离 codecvt、sanitize 与 directory hook；此前硬编码
  版本实际写入了 emuMMC，不能用于判断某一类 FS Hook 是否有问题。
