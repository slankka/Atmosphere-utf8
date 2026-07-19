/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * UTF-8 codecvt replacement functions for the 6-slot PF_CHARCODE interface.
 * These replace Nintendo's CP932/Shift-JIS codecvt with proper UTF-8 handling.
 */

#include "codecvt_utf8.hpp"

extern "C" {

/* === (1) OEM→Unicode: UTF-8 → UTF-16 decoder ===
 *
 * X0 = src (UTF-8 byte pointer)
 * X1 = dst (UTF-16 output buffer)
 * Returns: W0 = (oem_width << 16) | uni_width, with uni_width = 2.
 */

static constexpr pf_s32 pack_oem2unicode_width(pf_s32 oem_width) {
    return (oem_width << 16) | 2;
}

pf_s32 oem2unicode_utf8(const pf_s8* src, pf_u16* dst) {
    const pf_u8 b0 = static_cast<pf_u8>(src[0]);

    /* ASCII fast path: b0 < 0x80 */
    if (b0 < 0x80) {
        dst[0] = b0;
        return pack_oem2unicode_width(1);
    }

    /* Standalone continuation byte (0x80–0xBF) → U+FFFD */
    if (b0 < 0xC0) {
        dst[0] = 0xFFFD;
        return pack_oem2unicode_width(1);
    }

    /* 2-byte sequence (C0–DF) */
    if (b0 < 0xE0) {
        const pf_u8 b1 = static_cast<pf_u8>(src[1]);
        /* Overlong guard: reject < 0xC2 */
        if (b0 < 0xC2 || (b1 & 0xC0) != 0x80) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }
        dst[0] = static_cast<pf_u16>(((b0 & 0x1F) << 6) | (b1 & 0x3F));
        return pack_oem2unicode_width(2);
    }

    /* 3-byte sequence (E0–EF) — CJK characters are here */
    if (b0 < 0xF0) {
        const pf_u8 b1 = static_cast<pf_u8>(src[1]);
        const pf_u8 b2 = static_cast<pf_u8>(src[2]);

        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }

        /* Overlong guard: E0 must be followed by ≥ 0xA0 */
        if (b0 == 0xE0 && b1 < 0xA0) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }

        /* Surrogate guard: ED must not encode D800–DFFF */
        if (b0 == 0xED && b1 >= 0xA0) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }

        dst[0] = static_cast<pf_u16>(((b0 & 0x0F) << 12) |
                                     ((b1 & 0x3F) << 6) |
                                     (b2 & 0x3F));
        return pack_oem2unicode_width(3);
    }

    /* 4-byte sequence (F0–F4) → output U+FFFD (BMP-only FAT LFN)
     * But still consume the correct number of bytes. */
    if (b0 < 0xF5) {
        const pf_u8 b1 = static_cast<pf_u8>(src[1]);
        /* At least one valid continuation byte */
        if ((b1 & 0xC0) != 0x80) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }
        dst[0] = 0xFFFD;
        return pack_oem2unicode_width(4);
    }

    /* Invalid: F5–FF */
    dst[0] = 0xFFFD;
    return pack_oem2unicode_width(1);
}

/* === (2) Unicode→OEM: UTF-16 → UTF-8 encoder ===
 *
 * X0 = src (UTF-16 pointer)
 * X1 = dst (UTF-8 output buffer)
 * Returns: W0 = (oem_width << 16) | uni_width
 */

pf_s32 unicode2oem_utf8(const pf_u16* src, pf_s8* dst) {
    const pf_u16 u = src[0];

    /* ASCII: u < 0x80 → 1 byte */
    if (u < 0x80) {
        dst[0] = static_cast<pf_s8>(u);
        dst[1] = 0;
        return (1 << 16) | 2;
    }

    /* 2-byte UTF-8: 0x80 ≤ u < 0x800 */
    if (u < 0x800) {
        dst[0] = static_cast<pf_s8>(0xC0 | (u >> 6));
        dst[1] = static_cast<pf_s8>(0x80 | (u & 0x3F));
        dst[2] = 0;
        return (2 << 16) | 2;
    }

    /* Surrogates or invalid: D800–DFFF, FFFE, FFFF → '_' */
    if ((u >= 0xD800 && u <= 0xDFFF) || u == 0xFFFE || u == 0xFFFF) {
        dst[0] = '_';
        dst[1] = 0;
        return (1 << 16) | 2;
    }

    /* 3-byte UTF-8: u ≥ 0x800 */
    dst[0] = static_cast<pf_s8>(0xE0 | (u >> 12));
    dst[1] = static_cast<pf_s8>(0x80 | ((u >> 6) & 0x3F));
    dst[2] = static_cast<pf_s8>(0x80 | (u & 0x3F));
    dst[3] = 0;
    return (3 << 16) | 2;
}

/* === (3) OEM char width: byte width of UTF-8 sequence from first byte === */

pf_s32 oem_char_width_utf8(const pf_s8* src) {
    const pf_u8 b0 = static_cast<pf_u8>(src[0]);
    if (b0 < 0x80) return 1;
    if (b0 < 0xC0) return 1;   /* standalone continuation */
    if (b0 < 0xE0) return 2;
    if (b0 < 0xF0) return 3;
    return 4;
}

/* === (4) Is OEM multi-byte character ===
 *
 * num == 1: is this a UTF-8 lead byte? (≥ 0xC0)
 * num == 2: is this a UTF-8 continuation byte? ((b & 0xC0) == 0x80)
 * other: false
 */

pf_bool is_oem_mb_utf8(pf_s8 src, int num) {
    const pf_u8 b = static_cast<pf_u8>(src);
    if (num == 1) {
        return b >= 0xC0;
    }
    if (num == 2) {
        return (b & 0xC0) == 0x80;
    }
    return PF_FALSE;
}

/* === (5) Unicode char width: always 2 for BMP === */

pf_s32 unicode_char_width_utf8(const pf_u16* /*src*/) {
    return 2;
}

/* === (6) Is Unicode multi-byte: always false for BMP === */

pf_bool is_unicode_mb_utf8(pf_u16 /*src*/, pf_bool /*num*/) {
    return PF_FALSE;
}

/* === UTF-8 Directory Validator (SFAT Directory::Read filter) ===
 *
 * Called from the cave trampoline. Validates that a byte sequence
 * in a filename is valid UTF-8.
 *
 * W9  = first_byte  (the byte being scanned)
 * X8  = index       (current index in filename; returned updated in DirResult)
 * X25/X26 = name_ptr (pointer to filename buffer)
 * X19/X24 = bound    (max scan index)
 */

DirResult utf8_dir_validate_cpp(u32 first_byte, u64 index,
                                const u8* name_ptr, u64 bound) {
    const u8 b0 = static_cast<u8>(first_byte);
    const u64 idx = index;

    /* Control characters → reject */
    if (b0 < 0x20) {
        return {idx, DIR_REJECT};
    }

    /* ASCII printable → pass to original ascii_checks */
    if (b0 < 0x7F) {
        return {idx, DIR_ASCII_CONTINUE};
    }

    /* Invalid UTF-8 lead byte (0x7F–0xC1) → reject */
    if (b0 < 0xC2) {
        return {idx, DIR_REJECT};
    }

    /* Helper: check if byte at index i is a valid continuation byte */
    auto is_cont = [&](u64 i) -> bool {
        if (i >= bound) return false;
        u8 b = name_ptr[i];
        return (b & 0xC0) == 0x80;
    };

    /* 2-byte UTF-8 (C2–DF) */
    if (b0 < 0xE0) {
        if (!is_cont(idx + 1)) {
            return {idx, DIR_REJECT};
        }
        return {idx + 2, DIR_SCAN_CONTINUE};
    }

    /* 3-byte UTF-8 (E0–EF) — CJK characters */
    if (b0 < 0xF0) {
        if (!is_cont(idx + 1) || !is_cont(idx + 2)) {
            return {idx, DIR_REJECT};
        }
        /* Overlong guard: E0 + < 0xA0 */
        if (b0 == 0xE0 && name_ptr[idx + 1] < 0xA0) {
            return {idx, DIR_REJECT};
        }
        /* Surrogate guard: ED + ≥ 0xA0 */
        if (b0 == 0xED && name_ptr[idx + 1] >= 0xA0) {
            return {idx, DIR_REJECT};
        }
        return {idx + 3, DIR_SCAN_CONTINUE};
    }

    /* 4-byte UTF-8 (F0–F4) — validate format, output U+FFFD */
    if (b0 < 0xF5) {
        if (!is_cont(idx + 1) || !is_cont(idx + 2) || !is_cont(idx + 3)) {
            return {idx, DIR_REJECT};
        }
        /* Overlong guard: F0 + < 0x90 */
        if (b0 == 0xF0 && name_ptr[idx + 1] < 0x90) {
            return {idx, DIR_REJECT};
        }
        /* Range limit: F4 + ≥ 0x90 */
        if (b0 == 0xF4 && name_ptr[idx + 1] >= 0x90) {
            return {idx, DIR_REJECT};
        }
        return {idx + 4, DIR_SCAN_CONTINUE};
    }

    return {idx, DIR_REJECT};
}

} /* extern "C" */
