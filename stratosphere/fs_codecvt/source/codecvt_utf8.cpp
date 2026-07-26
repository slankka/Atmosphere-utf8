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

static pf_u32 g_fs_match_failure_stage;

void set_fs_match_failure_stage(pf_u32 stage) {
    g_fs_match_failure_stage = stage;
}

pf_s32 oem2unicode_dbcs_safe(const pf_s8* src, pf_u16* dst) {
    const pf_u8 b0 = static_cast<pf_u8>(src[0]);

    /* ASCII, standalone continuation, two-byte UTF-8 and invalid F5-FF are
     * already bounded to at most the legacy two-byte character buffer. */
    if (b0 < 0xE0 || b0 >= 0xF5) {
        return oem2unicode_utf8(src, dst);
    }

    /* A three/four-byte UTF-8 lead cannot be represented by PrFILE2's DBCS
     * temporary. Consume only the bytes the caller actually staged; the next
     * continuation byte is handled independently on the following call. */
    const pf_u8 b1 = static_cast<pf_u8>(src[1]);
    dst[0] = 0xFFFD;
    return pack_oem2unicode_width((b1 & 0xC0) == 0x80 ? 2 : 1);
}

pf_s32 oem2unicode_utf8(const pf_s8* src, pf_u16* dst) {
    const pf_u8 b0 = static_cast<pf_u8>(src[0]);

#if defined(FS_CODECVT_DIAG_MATCH_STAGE) || defined(FS_CODECVT_DIAG_VERSION_ID)
    /* Encode a diagnostic value as a visible ASCII long name.  Keep F25's
     * safe SBCS/DBCS consumption: a lead byte consumes two bytes and a
     * standalone continuation consumes one. */
    if (b0 >= 0x80) {
        const pf_u32 stage = g_fs_match_failure_stage <= 25 ?
                             g_fs_match_failure_stage : 25;
        dst[0] = static_cast<pf_u16>('A' + stage);
        return pack_oem2unicode_width(b0 >= 0xC0 ? 2 : 1);
    }
#endif

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
#if defined(FS_CODECVT_DIAG_OEM2U_DBCS_SAFE)
        /* Diagnostic probe: several PrFILE2 callers stage an OEM character in
         * a two-byte temporary buffer before invoking oem2unicode(). Avoid
         * reading src[2] and keep consumption within that legacy DBCS
         * contract. This deliberately cannot decode CJK correctly; it only
         * tests whether the third-byte read is the black-screen trigger. */
        if ((b1 & 0xC0) != 0x80) {
            dst[0] = 0xFFFD;
            return pack_oem2unicode_width(1);
        }
        dst[0] = 0xFFFD;
        return pack_oem2unicode_width(2);
#else
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
#if defined(FS_CODECVT_DIAG_OEM2U_WIDTH2)
        /* Diagnostic probe: preserve the real three-byte read/decode but
         * report the legacy DBCS maximum width. This separates an unsafe
         * src[2] read from downstream rejection of oem_width == 3. */
        return pack_oem2unicode_width(2);
#else
        return pack_oem2unicode_width(3);
#endif
#endif
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
#if defined(FS_CODECVT_DIAG_OEM_SINGLE_BYTE)
    /* Diagnostic/strategy probe: PrFILE2's is_oem_mb_char consumers only know
     * SBCS/DBCS and may copy at most two bytes to a temporary character
     * buffer. Treat the UTF-8 byte stream as SBCS at this layer; the dedicated
     * conversion and validation hooks retain responsibility for UTF-8. */
    (void)src;
    (void)num;
    return PF_FALSE;
#else
    const pf_u8 b = static_cast<pf_u8>(src);
    if (num == 1) {
        return b >= 0xC0;
    }
    if (num == 2) {
        return (b & 0xC0) == 0x80;
    }
    return PF_FALSE;
#endif
}

/* === (5) Unicode char width: always 2 for BMP === */

pf_s32 unicode_char_width_utf8(const pf_u16* /*src*/) {
    return 2;
}

/* === (6) Is Unicode multi-byte: always false for BMP === */

pf_bool is_unicode_mb_utf8(pf_u16 /*src*/, pf_bool /*num*/) {
    return PF_FALSE;
}

/* === High-level complete-string converters ===
 *
 * PrFILE2's PF_CHARCODE callbacks are also invoked with one/two-byte temporary
 * character buffers, so globally teaching oem2unicode() about a third byte is
 * unsafe. These two functions replace only the path routines whose source is
 * guaranteed to be a complete NUL-terminated string. */

pf_s32 transform_from_unicode_to_normal_utf8(pf_s8* dst, const pf_u16* src) {
    while (*src != 0) {
        const pf_s32 width = unicode2oem_utf8(src, dst);
        const pf_s16 oem_width = static_cast<pf_s16>(width >> 16);
        const pf_s16 uni_width = static_cast<pf_s16>(width & 0xFFFF);
        dst += oem_width;
        src += uni_width >> 1;
    }
    *dst = 0;
    return 0;
}

pf_s32 transform_in_unicode_utf8(pf_u16* dst, const pf_s8* src) {
    pf_s32 count = 0;
    while (*src != 0) {
        const pf_s32 width = oem2unicode_utf8(src, dst);
        const pf_s16 oem_width = static_cast<pf_s16>(width >> 16);
        const pf_s16 uni_width = static_cast<pf_s16>(width & 0xFFFF);
        src += oem_width;
        dst += uni_width >> 1;
        ++count;
    }
    *dst = 0;
    return count;
}

pf_u16 get_next_char_of_pattern_utf8(PfStr64* pattern, pf_bool is_long_name) {
    if (!pattern || !pattern->head) return 0;

    pf_u16 wc = 0;
    if (pattern->code_mode == 1) {
        const pf_s8* const src = pattern->head;
        if (*src == 0 || src >= pattern->tail) return 0;

        if (is_long_name) {
            const pf_s32 packed = oem2unicode_utf8(src, &wc);
            pf_s32 width = static_cast<pf_s16>(packed >> 16);
            if (width < 1 || src + width > pattern->tail) width = 1;
            pattern->head = src + width;
        } else {
            /* The 8.3 short-name side remains byte-oriented. Long UTF-8 names
             * are matched through the long-name branch above. */
            wc = static_cast<pf_u8>(*src);
            pattern->head = src + 1;
        }
    } else {
        const pf_u16* const src = reinterpret_cast<const pf_u16*>(pattern->head);
        if (*src == 0 || pattern->head + sizeof(pf_u16) > pattern->tail) return 0;
        wc = *src;
        pattern->head += sizeof(pf_u16);
        if (!is_long_name && wc > 0x7F) {
            /* A multibyte UTF-8 encoding cannot be represented by the legacy
             * two-byte short-name return value; keep conservative fallback. */
            wc = '_';
        }
    }

    if (wc >= 'a' && wc <= 'z') wc = static_cast<pf_u16>(wc - 0x20);
    if (is_long_name && wc >= 0xFF41 && wc <= 0xFF5A) {
        wc = static_cast<pf_u16>(wc - 0x20);
    }
    return wc;
}

static bool is_sfn_ascii(pf_u32 c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '$' || c == '%' || c == '\'' ||
           c == '@' || c == '~' || c == '`' || c == '!' || c == '(' ||
           c == ')' || c == '{' || c == '}' || c == '^' || c == '#' ||
           c == '&';
}

static pf_u32 ascii_upper(pf_u32 c) {
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

struct NameCursor {
    const pf_s8* cur;
    const pf_s8* end;
    u32 mode;
};

static pf_u32 next_name_codepoint(NameCursor* cursor) {
    if (!cursor || cursor->cur >= cursor->end) return 0;

    if (cursor->mode == 2) {
        if (cursor->cur + sizeof(pf_u16) > cursor->end) {
            cursor->cur = cursor->end;
            return 0xFFFD;
        }
        const pf_u16 value = static_cast<pf_u16>(
            static_cast<pf_u8>(cursor->cur[0]) |
            (static_cast<pf_u16>(static_cast<pf_u8>(cursor->cur[1])) << 8));
        cursor->cur += sizeof(pf_u16);
        return value;
    }

    const pf_u8 b0 = static_cast<pf_u8>(*cursor->cur++);
    if (b0 < 0x80) return b0;

    auto take_cont = [&](pf_u8* out) -> bool {
        if (cursor->cur >= cursor->end) return false;
        const pf_u8 value = static_cast<pf_u8>(*cursor->cur);
        if ((value & 0xC0) != 0x80) return false;
        ++cursor->cur;
        *out = value;
        return true;
    };

    pf_u8 b1 = 0, b2 = 0, b3 = 0;
    if (b0 >= 0xC2 && b0 <= 0xDF && take_cont(&b1)) {
        return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
    }
    if (b0 >= 0xE0 && b0 <= 0xEF && take_cont(&b1) && take_cont(&b2)) {
        if ((b0 != 0xE0 || b1 >= 0xA0) &&
            (b0 != 0xED || b1 < 0xA0)) {
            return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) |
                   (b2 & 0x3F);
        }
        return 0xFFFD;
    }
    if (b0 >= 0xF0 && b0 <= 0xF4 && take_cont(&b1) &&
        take_cont(&b2) && take_cont(&b3)) {
        if ((b0 != 0xF0 || b1 >= 0x90) &&
            (b0 != 0xF4 || b1 < 0x90)) {
            return 0x10000 + ((b0 & 7) << 18) + ((b1 & 0x3F) << 12) +
                   ((b2 & 0x3F) << 6) + (b3 & 0x3F);
        }
    }
    return 0xFFFD;
}

static const pf_s8* find_last_dot(const PfStr64* pattern) {
    NameCursor cursor{pattern->head, pattern->tail, pattern->code_mode};
    const pf_s8* dot = nullptr;
    while (cursor.cur < cursor.end) {
        const pf_s8* const at = cursor.cur;
        const pf_u32 cp = next_name_codepoint(&cursor);
        if (cp == '.') dot = at;
    }
    if (dot == pattern->head) return nullptr;
    return dot;
}

static bool is_exact_sfn(const PfStr64* pattern, const pf_s8* dot) {
    NameCursor cursor{pattern->head, pattern->tail, pattern->code_mode};
    u32 base_len = 0;
    u32 ext_len = 0;
    bool in_ext = false;

    while (cursor.cur < cursor.end) {
        const pf_s8* const at = cursor.cur;
        const pf_u32 cp = next_name_codepoint(&cursor);
        if (at == dot) {
            if (in_ext || base_len == 0) return false;
            in_ext = true;
            continue;
        }
        if (cp >= 'a' && cp <= 'z') return false;
        if (!is_sfn_ascii(cp)) return false;
        if ((!in_ext && ++base_len > 8) || (in_ext && ++ext_len > 3)) {
            return false;
        }
    }
    return base_len != 0 && (!in_ext || ext_len != 0);
}

static void append_alias_part(pf_s8* dst, u32* pos, u32 limit,
                              const pf_s8* begin, const pf_s8* end,
                              u32 mode) {
    NameCursor cursor{begin, end, mode};
    while (cursor.cur < cursor.end && *pos < limit) {
        const pf_u32 cp = next_name_codepoint(&cursor);
        if (cp == ' ' || cp == '.') continue;
        const pf_u32 upper = ascii_upper(cp);
        dst[(*pos)++] = static_cast<pf_s8>(is_sfn_ascii(upper) ? upper : '_');
    }
}

pf_u32 parse_short_name_utf8_fat(pf_s8* dst, const PfStr64* pattern) {
    if (!dst) return 1;
    dst[0] = 0;
    if (!pattern || !pattern->head || !pattern->tail ||
        pattern->tail <= pattern->head ||
        (pattern->code_mode != 1 && pattern->code_mode != 2)) {
        return 1;
    }

    const pf_s8* const dot = find_last_dot(pattern);
    if (is_exact_sfn(pattern, dot)) {
        u32 pos = 0;
        NameCursor cursor{pattern->head, pattern->tail, pattern->code_mode};
        while (cursor.cur < cursor.end && pos < 12) {
            const pf_u32 cp = next_name_codepoint(&cursor);
            dst[pos++] = static_cast<pf_s8>(ascii_upper(cp));
        }
        dst[pos] = 0;
        return 0;
    }

    /* Long names receive a deterministic ASCII seed.  The existing
     * VFiPFENT_AdjustSFN caller owns collision detection and ~N adjustment. */
    u32 pos = 0;
    const pf_s8* const base_end = dot ? dot : pattern->tail;
    append_alias_part(dst, &pos, 6, pattern->head, base_end,
                      pattern->code_mode);
    if (pos == 0) dst[pos++] = '_';
    dst[pos++] = '~';
    dst[pos++] = '1';

    if (dot) {
        const pf_s8* ext = dot + (pattern->code_mode == 2 ? 2 : 1);
        if (ext < pattern->tail) {
            dst[pos++] = '.';
            append_alias_part(dst, &pos, 12, ext, pattern->tail,
                              pattern->code_mode);
            if (dst[pos - 1] == '.') --pos;
        }
    }
    dst[pos] = 0;
    return 1;
}

pf_s32 split_path_utf8_mkdir(void* /*context*/, const PfStr64* path,
                             PfStr64* dir_path, PfStr64* filename) {
    if (!path || !dir_path || !filename || !path->head || !path->tail) return 10;
    if (path->code_mode != 1 || path->tail <= path->head) return 2;

    const pf_s8* begin = path->head;
    const pf_s8* end = path->tail;
    const pf_s8* separator = nullptr;
    for (const pf_s8* p = begin; p < end; ++p) {
        if (*p == '/' || *p == '\\') separator = p;
    }

    if (!separator || separator + 1 >= end) return 2;

    dir_path->head = begin;
    dir_path->tail = separator;
    dir_path->code_mode = 1;
    dir_path->_pad = 0;

    filename->head = separator + 1;
    filename->tail = end;
    filename->code_mode = 1;
    filename->_pad = 0;
    return 0;
}

using SplitPathFunction = pf_s32 (*)(void*, const PfStr64*, PfStr64*, PfStr64*);
static SplitPathFunction g_original_split_path = nullptr;

void set_original_split_path(void* function) {
    g_original_split_path = reinterpret_cast<SplitPathFunction>(function);
}

pf_s32 split_path_ascii_probe(void* context, const PfStr64* path,
                              PfStr64* dir_path, PfStr64* filename) {
    static const pf_s8 target[] =
        "/ROM/\xE4\xB8\xAD\xE6\x96\x87\xE7\x9B\xAE\xE5\xBD\x95";
    static const pf_s8 parent[] = "/ROM";
    static const pf_s8 sentinel[] = "UTF8TEST";

    if (path && dir_path && filename && path->head && path->tail &&
        path->code_mode == 1 &&
        static_cast<size_t>(path->tail - path->head) == sizeof(target) - 1) {
        bool match = true;
        for (size_t i = 0; i < sizeof(target) - 1; ++i) {
            if (path->head[i] != target[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            dir_path->head = parent;
            dir_path->tail = parent + sizeof(parent) - 1;
            dir_path->code_mode = 1;
            dir_path->_pad = 0;
            filename->head = sentinel;
            filename->tail = sentinel + sizeof(sentinel) - 1;
            filename->code_mode = 1;
            filename->_pad = 0;
            return 0;
        }
    }

    if (!g_original_split_path) return 10;
    return g_original_split_path(context, path, dir_path, filename);
}

pf_s32 split_path_ascii_prefix_probe(void* context, const PfStr64* path,
                                     PfStr64* dir_path, PfStr64* filename) {
    static const pf_s8 prefix[] = "/ROM/";
    static const pf_s8 parent[] = "/ROM";
    static const pf_s8 sentinel[] = "UTF8PFX";

    /* Flight Test 39 deliberately ignores PF_STR::tail and code_mode.  The
     * p_mkdir caller has already accepted the PF_STR as mode 1, and checking
     * only the ASCII namespace keeps the probe away from unrelated system
     * mkdir calls while removing F38's remaining match assumptions. */
    bool match = path && path->head && dir_path && filename;
    for (size_t i = 0; match && i < sizeof(prefix) - 1; ++i) {
        if (path->head[i] != prefix[i]) match = false;
    }
    if (match) {
        dir_path->head = parent;
        dir_path->tail = parent + sizeof(parent) - 1;
        dir_path->code_mode = 1;
        dir_path->_pad = 0;
        filename->head = sentinel;
        filename->tail = sentinel + sizeof(sentinel) - 1;
        filename->code_mode = 1;
        filename->_pad = 0;
        return 0;
    }

    if (!g_original_split_path) return 10;
    return g_original_split_path(context, path, dir_path, filename);
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
