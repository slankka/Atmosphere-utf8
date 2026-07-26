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
    if (!o->codecvt[0] || !o->codecvt[3] ||
        !o->path_from_unicode || !o->path_in_unicode ||
        !o->pattern_next_char || !o->parse_shortname_entry ||
        !o->dir_hook || !o->dir_reject || !o->dir_ascii_checks ||
        !o->dir_scan_continue || !o->cave || !o->cave_size) {
        return false;
    }
    u32 max_offset = o->codecvt[5] + sizeof(u32);
    for (int i = 0; i < 3; ++i) {
        if (o->sanitize[i] + sizeof(u32) > max_offset) max_offset = o->sanitize[i] + sizeof(u32);
    }
    if (o->dir_hook && o->dir_hook + sizeof(u32) > max_offset) max_offset = o->dir_hook + sizeof(u32);
    if (o->dir_hook && o->cave + o->cave_size > max_offset) max_offset = o->cave + o->cave_size;
    if (o->path_from_unicode && o->path_from_unicode + sizeof(u32) > max_offset) max_offset = o->path_from_unicode + sizeof(u32);
    if (o->path_in_unicode && o->path_in_unicode + sizeof(u32) > max_offset) max_offset = o->path_in_unicode + sizeof(u32);
    if (o->pattern_next_char && o->pattern_next_char + sizeof(u32) > max_offset) max_offset = o->pattern_next_char + sizeof(u32);
    for (const auto &check : o->identity_checks) {
        if (check.offset && check.offset + sizeof(u32) > max_offset) {
            max_offset = check.offset + sizeof(u32);
        }
    }
    if (o->parse_shortname_entry && o->parse_shortname_entry + sizeof(u32) > max_offset) max_offset = o->parse_shortname_entry + sizeof(u32);
    const uintptr_t last = base + max_offset;
    if (last < base || last > region_end) return false;
    const volatile u32 *entry = reinterpret_cast<const volatile u32 *>(base + o->codecvt[0]);
    if (entry[0] != o->codecvt_entry[0] || entry[1] != o->codecvt_entry[1]) return false;
    if (o->dir_hook && *reinterpret_cast<const volatile u32 *>(base + o->dir_hook) != o->dir_hook_opcode) return false;
    if (o->path_from_unicode && *reinterpret_cast<const volatile u32 *>(base + o->path_from_unicode) != o->path_from_entry) return false;
    if (o->path_in_unicode && *reinterpret_cast<const volatile u32 *>(base + o->path_in_unicode) != o->path_in_entry) return false;
    if (o->pattern_next_char && *reinterpret_cast<const volatile u32 *>(base + o->pattern_next_char) != o->pattern_entry) return false;
    for (const auto &check : o->identity_checks) {
        if (check.offset &&
            *reinterpret_cast<const volatile u32 *>(base + check.offset) != check.opcode) {
            return false;
        }
    }
    if (o->parse_shortname_entry && *reinterpret_cast<const volatile u32 *>(base + o->parse_shortname_entry) != o->parse_shortname_entry_opcode) return false;
    for (int i = 0; i < 3; ++i) {
        if (o->sanitize[i] == 0) continue;
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
    const FsCodecvtOffsets *o = fs_offs;
    if (!o->codecvt[0] || !o->codecvt[3] ||
        !o->path_from_unicode || !o->path_in_unicode ||
        !o->pattern_next_char || !o->parse_shortname_entry ||
        !o->dir_hook || !o->dir_reject || !o->dir_ascii_checks ||
        !o->dir_scan_continue || !o->cave || !o->cave_size) {
        return false;
    }

    /* The legacy PF_CHARCODE path may provide only a two-byte temporary.
     * Hook only OEM->Unicode and byte classification at this layer. */
    u32 oem2unicode_hook, is_oem_mb_hook;
    if (!encode_b(fs_code_base + o->codecvt[0],
                  reinterpret_cast<uintptr_t>(&oem2unicode_dbcs_safe),
                  &oem2unicode_hook)) return false;
    if (!encode_b(fs_code_base + o->codecvt[3],
                  reinterpret_cast<uintptr_t>(&is_oem_mb_utf8),
                  &is_oem_mb_hook)) return false;

    /* These entry points receive complete strings and can safely perform
     * full UTF-8 conversion. */
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

    u32 parse_shortname_hook;
    if (!encode_b(fs_code_base + o->parse_shortname_entry,
                  reinterpret_cast<uintptr_t>(&parse_short_name_utf8_fat),
                  &parse_shortname_hook)) return false;

    /* The hook is in the middle of Directory::Read, not at an ABI
     * function boundary. Preserve every live caller-saved register except X8,
     * which deliberately receives the validated next scan index. The C++
     * validator returns DirResult in X0/X1 according to AAPCS64. */
    {
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

    w32(fs_code_base + o->codecvt[0], oem2unicode_hook);
    w32(fs_code_base + o->codecvt[3], is_oem_mb_hook);
    w32(fs_code_base + o->path_from_unicode, path_from_hook);
    w32(fs_code_base + o->path_in_unicode, path_in_hook);
    w32(fs_code_base + o->pattern_next_char, pattern_next_hook);
    w32(fs_code_base + o->parse_shortname_entry, parse_shortname_hook);
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
    if(!get_ph())return;
    if(!map_fs())return;
    fs_offs=&g_fs_codecvt_offsets[vi];
    if (!install()) { unmap(); return; }
    armDCacheFlush(fs_rw_mapping,fs_code_size);
    armICacheInvalidate((void*)fs_code_base,fs_code_size);
    unmap();
}
