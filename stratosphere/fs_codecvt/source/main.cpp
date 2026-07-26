/*
 * fs_codecvt KIP — runtime UTF-8 codecvt hook for FS.
 * Graceful: any init failure → skip hooks, FS boots unmodified.
 */

#include <stdint.h>
#include <string.h>
#include "nx/svc.h"
#include "nx/smc.h"
#include "nx/cache.h"
#include "utils/fatal.h"
#include "codecvt_utf8.hpp"
#include "fs_offsets.h"

extern "C" void __init(void);
extern "C" void __libc_init_array(void);
extern char _start;
extern char __argdata__;

static uintptr_t fs_code_base;
static size_t fs_code_size;
static u8 *fs_rw_mapping;
static Handle self_proc_handle;
static char inner_heap[INNER_HEAP_SIZE];
static const FsCodecvtOffsets *fs_offs;

#define NOP_  0xD503201F

/* Validated FAT32 production profile (Flight Test 46).  Keep the individual
 * diagnostic switches available for controlled experiments, while normal
 * builds select the exact same dual-contract behavior through one name. */
#if defined(FS_CODECVT_FAT32_DUAL_CONTRACT)
    #ifndef FS_CODECVT_FAT32_HIGH_LEVEL
        #define FS_CODECVT_FAT32_HIGH_LEVEL
    #endif
    #ifndef FS_CODECVT_FAT32_HIGH_LEVEL_DIR
        #define FS_CODECVT_FAT32_HIGH_LEVEL_DIR
    #endif
    #ifndef FS_CODECVT_OEM2U_GLOBAL_DBCS_SAFE
        #define FS_CODECVT_OEM2U_GLOBAL_DBCS_SAFE
    #endif
    #ifndef FS_CODECVT_DIAG_INCLUDE_AUX
        #define FS_CODECVT_DIAG_INCLUDE_AUX
    #endif
    #ifndef FS_CODECVT_DIAG_HOOK_MASK
        #define FS_CODECVT_DIAG_HOOK_MASK 9
    #endif
#endif

#if defined(FS_CODECVT_DIAG_HOOK_MASK)
static constexpr u32 CodecvtHookMask = FS_CODECVT_DIAG_HOOK_MASK;
#elif defined(FS_CODECVT_FAT32_HIGH_LEVEL)
static constexpr u32 CodecvtHookMask = 0;
#else
static constexpr u32 CodecvtHookMask = 0x3F;
#endif

static constexpr bool codecvt_hook_enabled(int slot) {
    return (CodecvtHookMask & (1u << slot)) != 0;
}

extern "C" void __initheap(void) {
    extern char *fake_heap_start, *fake_heap_end;
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + INNER_HEAP_SIZE;
}

static bool branch_delta(uintptr_t source, uintptr_t target, s64 *words) {
    const s64 delta = static_cast<s64>(target) - static_cast<s64>(source);
    if ((delta & 3) != 0) return false;
    *words = delta >> 2;
    return true;
}

static bool encode_b(uintptr_t source, uintptr_t target, u32 *out, bool link = false) {
    s64 words;
    if (!branch_delta(source, target, &words) || words < -(1LL << 25) || words >= (1LL << 25)) return false;
    *out = (link ? 0x94000000 : 0x14000000) | (static_cast<u32>(words) & 0x03FFFFFF);
    return true;
}

static bool encode_b_cond(uintptr_t source, uintptr_t target, u32 condition, u32 *out) {
    s64 words;
    if (!branch_delta(source, target, &words) || words < -(1LL << 18) || words >= (1LL << 18)) return false;
    *out = 0x54000000 | ((static_cast<u32>(words) & 0x7FFFF) << 5) | (condition & 0xF);
    return true;
}

static void w32(uintptr_t a, u32 v) { *(u32*)(fs_rw_mapping+(a-fs_code_base)) = v; }
static void nop(uintptr_t s) { w32(s, NOP_); }

static void recv_thread(void *arg) {
    Handle h = *(Handle*)arg;
    memset(armGetTls(),0,0x10); s32 idx=0;
    svcReplyAndReceive(&idx,&h,1,INVALID_HANDLE,UINT64_MAX);
    self_proc_handle = ((u32*)armGetTls())[3];
    svcCloseHandle(h); svcExitThread();
    while(1){}
}

static bool get_ph(void) {
    Handle s,c; if(svcCreateSession(&s,&c,0,0)) return false;
    u8 stk[0x1000]; Handle th;
    if(svcCreateThread(&th,(void*)recv_thread,&s,stk+0x1000,0x20,3)){svcCloseHandle(s);svcCloseHandle(c);return false;}
    if(svcStartThread(th)){svcCloseHandle(s);svcCloseHandle(c);svcCloseHandle(th);return false;}
    static const u32 process_handle_message[4] = { 0x00000000, 0x80000000, 0x00000002, 0xFFFF8001 };
    memcpy(armGetTls(), process_handle_message, sizeof(process_handle_message));

    /* The receiver closes the server session without replying, so the send
     * result is intentionally not used as the success signal. Wait for the
     * receiver thread, exactly as emuMMC does, before consuming the copied
     * process handle. */
    svcSendSyncRequest(c);
    svcCloseHandle(c);
    s32 wait_index = -1;
    const Result wait_rc = svcWaitSynchronization(&wait_index, &th, 1, UINT64_MAX);
    svcCloseHandle(th);
    return wait_rc == 0 && self_proc_handle != 0;
}

static bool map_fs(void) {
    int n=0;Result r;
    do{ u64 a=0x70000000ULL; r=svcMapProcessMemory((void*)a,self_proc_handle,fs_code_base,fs_code_size);
        if(r==0){fs_rw_mapping=(u8*)a;return true;}
    }while((r==0xDC01||r==0xD401)&&++n<100);
    return false;
}
static void unmap(void){if(fs_rw_mapping){svcUnmapProcessMemory(fs_rw_mapping,self_proc_handle,fs_code_base,fs_code_size);fs_rw_mapping=0;}}

static bool matches_fs(uintptr_t base, uintptr_t region_end, const FsCodecvtOffsets *o) {
    u32 max_offset = o->codecvt[5] + sizeof(u32);
    for (int i = 0; i < 3; ++i) {
        if (o->sanitize[i] + sizeof(u32) > max_offset) max_offset = o->sanitize[i] + sizeof(u32);
    }
    if (o->dir_hook && o->dir_hook + sizeof(u32) > max_offset) max_offset = o->dir_hook + sizeof(u32);
    if (o->dir_hook && o->cave + o->cave_size > max_offset) max_offset = o->cave + o->cave_size;
#if !defined(FS_CODECVT_DIAG_LEGACY_MATCH)
    if (o->path_from_unicode && o->path_from_unicode + sizeof(u32) > max_offset) max_offset = o->path_from_unicode + sizeof(u32);
    if (o->path_in_unicode && o->path_in_unicode + sizeof(u32) > max_offset) max_offset = o->path_in_unicode + sizeof(u32);
    if (o->pattern_next_char && o->pattern_next_char + sizeof(u32) > max_offset) max_offset = o->pattern_next_char + sizeof(u32);
    if (o->path_token_dbcs_branch && o->path_token_dbcs_branch + sizeof(u32) > max_offset) max_offset = o->path_token_dbcs_branch + sizeof(u32);
    if (o->shortname_oem_call && o->shortname_oem_call + 2 * sizeof(u32) > max_offset) max_offset = o->shortname_oem_call + 2 * sizeof(u32);
    if (o->mkdir_split_call && o->mkdir_split_call + sizeof(u32) > max_offset) max_offset = o->mkdir_split_call + sizeof(u32);
    if (o->mkdir_parent_result && o->mkdir_parent_result + sizeof(u32) > max_offset) max_offset = o->mkdir_parent_result + sizeof(u32);
    if (o->mkdir_local_error && o->mkdir_local_error + sizeof(u32) > max_offset) max_offset = o->mkdir_local_error + sizeof(u32);
    if (o->parse_shortname_entry && o->parse_shortname_entry + sizeof(u32) > max_offset) max_offset = o->parse_shortname_entry + sizeof(u32);
#endif
    const uintptr_t last = base + max_offset;
    if (last < base || last > region_end) return false;
    const volatile u32 *entry = reinterpret_cast<const volatile u32 *>(base + o->codecvt[0]);
    if (entry[0] != o->codecvt_entry[0] || entry[1] != o->codecvt_entry[1]) return false;
    if (o->dir_hook && *reinterpret_cast<const volatile u32 *>(base + o->dir_hook) != o->dir_hook_opcode) return false;
#if defined(FS_CODECVT_DIAG_MATCH_STAGE)
    u32 stage = 0;
    if (o->path_from_unicode &&
        *reinterpret_cast<const volatile u32 *>(base + o->path_from_unicode) != o->path_from_entry) stage = 1;
    else if (o->path_in_unicode &&
             *reinterpret_cast<const volatile u32 *>(base + o->path_in_unicode) != o->path_in_entry) stage = 2;
    else if (o->pattern_next_char &&
             *reinterpret_cast<const volatile u32 *>(base + o->pattern_next_char) != o->pattern_entry) stage = 3;
    else if (o->path_token_dbcs_branch &&
             *reinterpret_cast<const volatile u32 *>(base + o->path_token_dbcs_branch) != o->path_token_dbcs_opcode) stage = 4;
    else if (o->shortname_oem_call) {
        const volatile u32 *call = reinterpret_cast<const volatile u32 *>(base + o->shortname_oem_call);
        if (call[0] != o->shortname_oem_opcodes[0]) stage = 5;
        else if (call[1] != o->shortname_oem_opcodes[1]) stage = 6;
    }
    if (stage == 0 && o->mkdir_split_call &&
        *reinterpret_cast<const volatile u32 *>(base + o->mkdir_split_call) != o->mkdir_split_opcode) stage = 7;
    if (stage == 0 && o->mkdir_parent_result &&
        *reinterpret_cast<const volatile u32 *>(base + o->mkdir_parent_result) != o->mkdir_parent_result_opcode) stage = 8;
    if (stage == 0 && o->mkdir_local_error &&
        *reinterpret_cast<const volatile u32 *>(base + o->mkdir_local_error) != o->mkdir_local_error_opcode) stage = 9;
    if (stage == 0 && o->parse_shortname_entry &&
        *reinterpret_cast<const volatile u32 *>(base + o->parse_shortname_entry) != o->parse_shortname_entry_opcode) stage = 10;
    set_fs_match_failure_stage(stage);
#elif !defined(FS_CODECVT_DIAG_LEGACY_MATCH)
    if (o->path_from_unicode && *reinterpret_cast<const volatile u32 *>(base + o->path_from_unicode) != o->path_from_entry) return false;
    if (o->path_in_unicode && *reinterpret_cast<const volatile u32 *>(base + o->path_in_unicode) != o->path_in_entry) return false;
    if (o->pattern_next_char && *reinterpret_cast<const volatile u32 *>(base + o->pattern_next_char) != o->pattern_entry) return false;
    if (o->path_token_dbcs_branch && *reinterpret_cast<const volatile u32 *>(base + o->path_token_dbcs_branch) != o->path_token_dbcs_opcode) return false;
    if (o->shortname_oem_call) {
        const volatile u32 *call = reinterpret_cast<const volatile u32 *>(base + o->shortname_oem_call);
        if (call[0] != o->shortname_oem_opcodes[0] || call[1] != o->shortname_oem_opcodes[1]) return false;
    }
    if (o->mkdir_split_call && *reinterpret_cast<const volatile u32 *>(base + o->mkdir_split_call) != o->mkdir_split_opcode) return false;
    if (o->mkdir_parent_result && *reinterpret_cast<const volatile u32 *>(base + o->mkdir_parent_result) != o->mkdir_parent_result_opcode) return false;
    if (o->mkdir_local_error && *reinterpret_cast<const volatile u32 *>(base + o->mkdir_local_error) != o->mkdir_local_error_opcode) return false;
    if (o->parse_shortname_entry && *reinterpret_cast<const volatile u32 *>(base + o->parse_shortname_entry) != o->parse_shortname_entry_opcode) return false;
#endif
    for (int i = 0; i < 3; ++i) {
        if (o->sanitize[i] == 0) continue;  /* sentinel: no sanitize for this version */
        const u32 op = *reinterpret_cast<const volatile u32 *>(base + o->sanitize[i]);
        if ((op & 0x7F000000) != 0x37000000) return false;
    }
    return true;
}

static s32 find_fs(uintptr_t region_end) {
    /* Fusee lays the RX image out as:
     *
     *   [fs_codecvt][emuMMC, when enabled][FS]
     *
     * __argdata__ is therefore only the start of the next injected image; it
     * is the FS base when emuMMC is disabled, but the emuMMC base otherwise.
     * Walk page-aligned candidates and require all of matches_fs()'s exact
     * instruction checks before accepting one. */
    uintptr_t candidate = (reinterpret_cast<uintptr_t>(&__argdata__) + 0xFFF) & ~static_cast<uintptr_t>(0xFFF);
    for (; candidate < region_end; candidate += 0x1000) {
        for (s32 i = FsVer_Count - 1; i >= 0; --i) {
            if (matches_fs(candidate, region_end, &g_fs_codecvt_offsets[i])) {
                fs_code_base = candidate;
                fs_code_size = region_end - candidate;
                return i;
            }
        }
    }
    return -1;
}

static bool install(void) {
    const FsCodecvtOffsets*o=fs_offs;
#if defined(FS_CODECVT_OEM2U_GLOBAL_DBCS_SAFE)
    const uintptr_t oem2unicode_target =
        reinterpret_cast<uintptr_t>(&oem2unicode_dbcs_safe);
#else
    const uintptr_t oem2unicode_target =
        reinterpret_cast<uintptr_t>(&oem2unicode_utf8);
#endif
    const uintptr_t codecvt_targets[6] = {
        oem2unicode_target, (uintptr_t)&unicode2oem_utf8,
        (uintptr_t)&oem_char_width_utf8, (uintptr_t)&is_oem_mb_utf8,
        (uintptr_t)&unicode_char_width_utf8, (uintptr_t)&is_unicode_mb_utf8,
    };
    u32 codecvt_hooks[6] = {};
    for (int i = 0; i < 6; ++i) {
        if (o->codecvt[i] == 0 || !codecvt_hook_enabled(i)) continue;
        if (!encode_b(fs_code_base + o->codecvt[i], codecvt_targets[i], &codecvt_hooks[i])) return false;
    }

#if defined(FS_CODECVT_DIAG_PATH_TRANSFORMS) || defined(FS_CODECVT_FAT32_HIGH_LEVEL)
    if (!o->path_from_unicode || !o->path_in_unicode || !o->pattern_next_char) return false;
    u32 path_from_hook, path_in_hook, pattern_next_hook;
    if (!encode_b(fs_code_base + o->path_from_unicode,
                  reinterpret_cast<uintptr_t>(&transform_from_unicode_to_normal_utf8),
                  &path_from_hook)) return false;
    if (!encode_b(fs_code_base + o->path_in_unicode,
                  reinterpret_cast<uintptr_t>(&transform_in_unicode_utf8),
                  &path_in_hook)) return false;
    if (!encode_b(fs_code_base + o->pattern_next_char,
                  reinterpret_cast<uintptr_t>(&get_next_char_of_pattern_utf8),
                  &pattern_next_hook)) return false;
#endif

#if defined(FS_CODECVT_FAT32_HIGH_LEVEL)
    if (!o->parse_shortname_entry) return false;
    u32 parse_shortname_hook;
    if (!encode_b(fs_code_base + o->parse_shortname_entry,
                  reinterpret_cast<uintptr_t>(&parse_short_name_utf8_fat),
                  &parse_shortname_hook)) return false;
#endif

#if defined(FS_CODECVT_DIAG_PATH_TOKEN_BYTES)
    if (!o->path_token_dbcs_branch) return false;
#endif

#if defined(FS_CODECVT_DIAG_SHORTNAME_OEM_CALL)
    if (!o->shortname_oem_call) return false;
    u32 shortname_oem_hook;
    if (!encode_b(fs_code_base + o->shortname_oem_call,
                  reinterpret_cast<uintptr_t>(&oem2unicode_utf8),
                  &shortname_oem_hook, true)) return false;
#endif

#if defined(FS_CODECVT_DIAG_MKDIR_SPLIT_PATH)
    if (!o->mkdir_split_call) return false;
    u32 mkdir_split_hook;
    if (!encode_b(fs_code_base + o->mkdir_split_call,
                  reinterpret_cast<uintptr_t>(&split_path_utf8_mkdir),
                  &mkdir_split_hook, true)) return false;
#endif

#if defined(FS_CODECVT_DIAG_MKDIR_ASCII_PROBE)
    if (!o->mkdir_split_call) return false;
    set_original_split_path(reinterpret_cast<void*>(fs_code_base + 0xF9B00));
    u32 mkdir_ascii_probe_hook;
    if (!encode_b(fs_code_base + o->mkdir_split_call,
                  reinterpret_cast<uintptr_t>(&split_path_ascii_probe),
                  &mkdir_ascii_probe_hook, true)) return false;
#endif

#if defined(FS_CODECVT_DIAG_MKDIR_ASCII_PREFIX_PROBE)
    if (!o->mkdir_split_call) return false;
    set_original_split_path(reinterpret_cast<void*>(fs_code_base + 0xF9B00));
    u32 mkdir_ascii_prefix_probe_hook;
    if (!encode_b(fs_code_base + o->mkdir_split_call,
                  reinterpret_cast<uintptr_t>(&split_path_ascii_prefix_probe),
                  &mkdir_ascii_prefix_probe_hook, true)) return false;
#endif

    /* Directory hook is optional — versions with dir_hook==0 skip it entirely.
     * When enabled, the hook is in the middle of Directory::Read, not at an ABI
     * function boundary. Preserve every live caller-saved register except X8,
     * which deliberately receives the validated next scan index. The C++
     * validator returns DirResult in X0/X1 according to AAPCS64. */
#if !defined(FS_CODECVT_DIAG_SANITIZE_ONLY) && \
    !defined(FS_CODECVT_DIAG_CODECVT_ONLY) && \
    (!defined(FS_CODECVT_DIAG_HOOK_MASK) || defined(FS_CODECVT_DIAG_INCLUDE_AUX)) && \
    (!defined(FS_CODECVT_FAT32_HIGH_LEVEL) || \
     defined(FS_CODECVT_FAT32_HIGH_LEVEL_DIR))
    if (o->dir_hook) {
        const uintptr_t cb=fs_code_base+o->cave,vf=(uintptr_t)&utf8_dir_validate_cpp;
        u32 t[48];int ti=0;
        auto emit = [&](u32 op) { t[ti++] = op; };
        auto stp = [&](u32 a, u32 b, u32 off) { emit(0xA90003E0 | ((off / 8) << 15) | (b << 10) | a); };
        auto ldp = [&](u32 a, u32 b, u32 off) { emit(0xA94003E0 | ((off / 8) << 15) | (b << 10) | a); };
        auto mov_x = [&](u32 dst, u32 src) { emit(0xAA0003E0 | (src << 16) | dst); };
        auto pc = [&]() -> uintptr_t { return cb + static_cast<uintptr_t>(ti) * 4; };

        emit(0xD102C3FF); /* sub sp, sp, #0xB0 */
        stp(0, 1, 0x00); stp(2, 3, 0x10); stp(4, 5, 0x20); stp(6, 7, 0x30);
        stp(9,10, 0x40); stp(11,12,0x50); stp(13,14,0x60); stp(15,16,0x70);
        stp(17,18,0x80); stp(30,31,0x90);
        emit(0x2A0003E0 | (o->byte_reg << 16)); /* mov w0, byte_reg (W9 or W10) */
        mov_x(1, 8);                     /* mov x1, x8 */
        mov_x(2, o->name_reg);           /* mov x2, name register */
        mov_x(3, o->bound_reg);          /* mov x3, bound register */
        { u32 op; if (!encode_b(pc(), vf, &op, true)) return false; emit(op); }
        stp(0, 1, 0xA0);                /* save returned index/action */
        ldp(0, 1, 0x00); ldp(2, 3, 0x10); ldp(4, 5, 0x20); ldp(6, 7, 0x30);
        ldp(9,10, 0x40); ldp(11,12,0x50); ldp(13,14,0x60); ldp(15,16,0x70);
        ldp(17,18,0x80); ldp(30,31,0x90);
        emit(0xF94053E8); /* ldr x8,  [sp, #0xA0] */
        emit(0xF94057EB); /* ldr x11, [sp, #0xA8] — use X11 for action, not X10 (21.2.0 byte in W10!) */
        emit(0x9102C3FF); /* add sp, sp, #0xB0 */
        emit(0x7100017F); /* cmp w11, #0 */
        { u32 op; if (!encode_b_cond(pc(),fs_code_base+o->dir_reject,0,&op)) return false; emit(op); }
        emit(0x7100057F); /* cmp w11, #1 */
        { u32 op; if (!encode_b_cond(pc(),fs_code_base+o->dir_ascii_checks,0,&op)) return false; emit(op); }
        { u32 op; if (!encode_b(pc(),fs_code_base+o->dir_scan_continue,&op)) return false; emit(op); }
        if((u32)(ti*4)>o->cave_size)return false;
        u32 dir_hook;
        if (!encode_b(fs_code_base + o->dir_hook, cb, &dir_hook)) return false;

        for(int i=0;i<ti;i++)w32(cb+i*4,t[i]);
        w32(fs_code_base + o->dir_hook, dir_hook);
    }
#endif

    /* Commit codecvt hooks. */
    for (int i = 0; i < 6; ++i) {
        if (o->codecvt[i] && codecvt_hook_enabled(i)) w32(fs_code_base + o->codecvt[i], codecvt_hooks[i]);
    }
#if defined(FS_CODECVT_DIAG_PATH_TRANSFORMS) || defined(FS_CODECVT_FAT32_HIGH_LEVEL)
    w32(fs_code_base + o->path_from_unicode, path_from_hook);
    w32(fs_code_base + o->path_in_unicode, path_in_hook);
    w32(fs_code_base + o->pattern_next_char, pattern_next_hook);
#endif
#if defined(FS_CODECVT_FAT32_HIGH_LEVEL)
    w32(fs_code_base + o->parse_shortname_entry, parse_shortname_hook);
#endif
#if defined(FS_CODECVT_DIAG_PATH_TOKEN_BYTES)
    /* PrFILE2's OEM path tokenizer assumes every multibyte character is a
     * two-byte DBCS pair.  UTF-8 cannot satisfy that contract.  Route mode-1
     * strings through its ordinary byte path instead; that path already
     * accepts every byte >= 0x80 and advances exactly one byte per loop. */
    w32(fs_code_base + o->path_token_dbcs_branch, 0x14000013); /* b 0xF9984 */
#endif
#if defined(FS_CODECVT_DIAG_SHORTNAME_OEM_CALL)
    /* parseShortName passes a pointer into the complete filename here, so the
     * UTF-8 decoder may safely inspect a third byte.  Keep the global slot0
     * untouched because other callers provide only two-byte temporaries. */
    w32(fs_code_base + o->shortname_oem_call, shortname_oem_hook);
    nop(fs_code_base + o->shortname_oem_call + sizeof(u32));
#endif
#if defined(FS_CODECVT_DIAG_MKDIR_SPLIT_PATH)
    w32(fs_code_base + o->mkdir_split_call, mkdir_split_hook);
#endif
#if defined(FS_CODECVT_DIAG_MKDIR_ASCII_PROBE)
    w32(fs_code_base + o->mkdir_split_call, mkdir_ascii_probe_hook);
#endif
#if defined(FS_CODECVT_DIAG_MKDIR_ASCII_PREFIX_PROBE)
    w32(fs_code_base + o->mkdir_split_call, mkdir_ascii_prefix_probe_hook);
#endif
#if defined(FS_CODECVT_DIAG_MKDIR_PARENT_RESULT)
    if (!o->mkdir_parent_result) return false;
    /* Tag only the value returned when GetEntryOfPath fails.  Its following
     * CBNZ still tests W0, while the mkdir epilogue returns tagged W21. */
    w32(fs_code_base + o->mkdir_parent_result, 0x11040015); /* add w21,w0,#0x100 */
#endif
#if defined(FS_CODECVT_DIAG_MKDIR_LOCAL_ERROR)
    if (!o->mkdir_local_error) return false;
    w32(fs_code_base + o->mkdir_local_error, 0x52800075); /* mov w21,#3 */
#endif
#if !defined(FS_CODECVT_FAT32_HIGH_LEVEL) && \
    !defined(FS_CODECVT_DIAG_CODECVT_ONLY) && \
    (!defined(FS_CODECVT_DIAG_HOOK_MASK) || defined(FS_CODECVT_DIAG_INCLUDE_AUX))
    /* The diagnostic codecvt-only build leaves the independent filename
     * filter changes untouched so the two hook groups can be isolated. */
    for (int i = 0; i < 3; ++i) { if (o->sanitize[i]) nop(fs_code_base + o->sanitize[i]); }
#endif
    return true;
}

extern "C" void __init(void) {
    __libc_init_array();
    MemoryInfo mi; u32 pi;
    /* startup has already split this overlay's own text/rodata/data permissions,
     * so querying &_start only describes the first overlay subregion. Query the
     * untouched RX region beginning at the next image instead; it contains
     * [emuMMC, when enabled][FS]. */
    const uintptr_t following_module =
        (reinterpret_cast<uintptr_t>(&__argdata__) + 0xFFF) &
        ~static_cast<uintptr_t>(0xFFF);
    if(svcQueryMemory(&mi, &pi, following_module)) return;
    const uintptr_t region_end = mi.addr + mi.size;
    if (region_end < mi.addr || following_module < mi.addr || following_module >= region_end) return;
    const s32 vi = find_fs(region_end);
    if(vi<0)return;
#if defined(FS_CODECVT_DIAG_VERSION_ID)
    /* Expose the table entry selected by find_fs() through the OEM decoder:
     * A=entry 0, B=entry 1, ... . */
    set_fs_match_failure_stage(static_cast<u32>(vi));
#endif
    if(!get_ph())return;
    if(!map_fs())return;
    fs_offs=&g_fs_codecvt_offsets[vi];
    if (!install()) { unmap(); return; }
    armDCacheFlush(fs_rw_mapping,fs_code_size);
    armICacheInvalidate((void*)fs_code_base,fs_code_size);
    unmap();
}
