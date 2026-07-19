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

/* === PF_CHARCODE function table (6 slots) === */
struct PF_CHARCODE {
    pf_s32 (*oem2unicode)(const pf_s8* src, pf_u16* dst);       /* +0x00 */
    pf_s32 (*unicode2oem)(const pf_u16* src, pf_s8* dst);       /* +0x08 */
    pf_s32 (*oem_char_width)(const pf_s8* src);                 /* +0x10 */
    pf_bool (*is_oem_mb_char)(const pf_s8 src, pf_bool num);    /* +0x18 */
    pf_s32 (*unicode_char_width)(const pf_u16* src);            /* +0x20 */
    pf_bool (*is_unicode_mb_char)(const pf_u16 src, pf_bool num);/* +0x28 */
};

/* === UTF-8 codecvt C++ implementations === */
#ifdef __cplusplus
extern "C" {
#endif

/* (1) OEM->Unicode: UTF-8 bytes → UTF-16 code unit.
 *     Returns (oem_width << 16) | uni_width. */
pf_s32 oem2unicode_utf8(const pf_s8* src, pf_u16* dst);

/* (2) Unicode->OEM: UTF-16 code unit → UTF-8 bytes.
 *     Returns (oem_width << 16) | uni_width */
pf_s32 unicode2oem_utf8(const pf_u16* src, pf_s8* dst);

/* (3) OEM char width: byte width of a UTF-8 sequence from first byte */
pf_s32 oem_char_width_utf8(const pf_s8* src);

/* (4) Is OEM multi-byte: lead byte detection / continuation byte detection.
 *     Note: num is 1 or 2 (passed as integer despite pf_bool in struct). */
pf_bool is_oem_mb_utf8(pf_s8 src, int num);

/* (5) Unicode char width: always 2 (BMP) */
pf_s32 unicode_char_width_utf8(const pf_u16* src);

/* (6) Is Unicode multi-byte: always false (BMP) */
pf_bool is_unicode_mb_utf8(pf_u16 src, pf_bool num);

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
