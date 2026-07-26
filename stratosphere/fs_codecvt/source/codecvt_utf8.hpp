/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#pragma once
#include "utils/types.h"

/* === PrFILE2 type aliases === */
typedef s8   pf_s8;
typedef u8   pf_u8;
typedef s16  pf_s16;
typedef u16  pf_u16;
typedef s32  pf_s32;
typedef u32  pf_u32;
typedef bool pf_bool;

constexpr pf_bool PF_TRUE  = true;
constexpr pf_bool PF_FALSE = false;

/* === UTF-8 conversion implementations === */
#ifdef __cplusplus
extern "C" {
#endif

/* (1) OEM->Unicode: UTF-8 bytes → UTF-16 code unit.
 *     Returns (oem_width << 16) | uni_width. */
pf_s32 oem2unicode_utf8(const pf_s8* src, pf_u16* dst);
/* Compatibility decoder for legacy callers that stage one OEM character in
 * a buffer of at most two bytes. */
pf_s32 oem2unicode_dbcs_safe(const pf_s8* src, pf_u16* dst);

/* (2) Unicode->OEM: UTF-16 code unit → UTF-8 bytes.
 *     Returns (oem_width << 16) | uni_width */
pf_s32 unicode2oem_utf8(const pf_u16* src, pf_s8* dst);

/* Is OEM multi-byte: lead byte detection / continuation byte detection.
 *     Note: num is 1 or 2 (passed as integer despite pf_bool in struct). */
pf_bool is_oem_mb_utf8(pf_s8 src, int num);

/* High-level PrFILE2 path converters. Unlike the PF_CHARCODE callbacks,
 * these are only called with complete NUL-terminated strings. */
pf_s32 transform_from_unicode_to_normal_utf8(pf_s8* dst, const pf_u16* src);
pf_s32 transform_in_unicode_utf8(pf_u16* dst, const pf_s8* src);

struct PfStr64 {
    const pf_s8* head;     /* +0x00 */
    const pf_s8* tail;     /* +0x08 */
    u32 code_mode;         /* +0x10 */
    u32 _pad;
};

pf_u16 get_next_char_of_pattern_utf8(PfStr64* pattern, pf_bool is_long_name);

/* FAT32-only replacement for the complete 8.3 alias builder.  It keeps the
 * UTF-8 long name separate and emits an ASCII SFN suitable for collision
 * adjustment by the existing PrFILE2 caller. */
pf_u32 parse_short_name_utf8_fat(pf_s8* dst, const PfStr64* pattern);

/* === UTF-8 directory validator (SFAT Directory::Read filter) === */

enum DirAction : u32 {
    DIR_REJECT         = 0,  /* jump to reject_entry */
    DIR_ASCII_CONTINUE = 1,  /* jump to ascii_checks */
    DIR_SCAN_CONTINUE  = 2,  /* jump to scan_continue (index already advanced) */
};

struct DirResult {
    u64 index;
    u64 action;
};

DirResult utf8_dir_validate_cpp(u32 first_byte, u64 index,
                                const u8* name_ptr, u64 bound);

#ifdef __cplusplus
}
#endif
