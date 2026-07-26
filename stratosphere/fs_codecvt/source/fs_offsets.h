/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * PF_CHARCODE slot offsets and utf8dir hook offsets for each FS version.
 * All offsets are relative to the decompressed FS .text section. They do not
 * include the 0x100-byte KIP1 file header.
 */

#pragma once
#include "utils/types.h"

/* FS version enum — matches fusee's FsVersion ordering */
enum FsVersion : u32 {
    /* exFAT variants */
    FsVer_19_0_0_Exfat  = 0,
    FsVer_20_2_0_Exfat  = 1,
    FsVer_21_2_0_Exfat  = 2,
    FsVer_22_0_0_Exfat  = 3,
    FsVer_22_5_0_Exfat  = 4,

    /* FAT32 variants */
    FsVer_19_0_0_Fat32  = 5,

    FsVer_Count         = 6,
};

/* Complete offsets for one FS version.
 * codecvt[6]: function entry offsets for the 6 PF_CHARCODE slots.
 * sanitize[3]: TBNZ→NOP sites in the ASCII filter.
 * dir_hook/dir_reject/dir_ascii_checks/dir_scan_continue: utf8dir hook points.
 * name_reg: 25 for 19.x/20.x, 28 for 21.2.0, 26 for 22.x+
 * bound_reg: 19 for 19.x/20.x, 9 for 21.2.0, 24 for 22.x+
 * byte_reg: register holding the current filename byte (9=W9, 10=W10)
 */
struct FsCodecvtOffsets {
    u32 codecvt[6];        /* oem2u, uni2oem, oem_char_width,
                               is_oem_mb, uni_char_width, is_uni_mb */
    u32 sanitize[3];
    u32 dir_hook;
    u32 dir_reject;
    u32 dir_ascii_checks;
    u32 dir_scan_continue;
    u8  name_reg;          /* name base register (25/28/26) */
    u8  bound_reg;         /* loop bound register (19/9/24) */
    u8  byte_reg;          /* filename byte register (9=W9, 10=W10) */
    u8  _pad;              /* explicit padding to keep u32 alignment */
    u32 cave;              /* computed: uni2oem + sizeof(uni2oem_replacement) */
    u32 cave_size;         /* computed: aux[0] - cave */
    u32 codecvt_entry[2];  /* exact original first two instructions */
    u32 dir_hook_opcode;   /* exact original instruction at dir_hook */
    u32 path_from_unicode; /* VFiPFPATH_transformFromUnicodeToNormal */
    u32 path_in_unicode;   /* VFiPFPATH_transformInUnicode */
    u32 path_from_entry;   /* exact original first instruction */
    u32 path_in_entry;     /* exact original first instruction */
    u32 pattern_next_char; /* VFiPFPATH_GetNextCharOfPattern */
    u32 pattern_entry;     /* exact original first instruction */
    u32 path_token_dbcs_branch; /* GetNextTokenOfPath: mode!=1 branch */
    u32 path_token_dbcs_opcode; /* exact original conditional branch */
    u32 shortname_oem_call;     /* parseShortName safe full-source slot0 call */
    u32 shortname_oem_opcodes[2]; /* original LDR function pointer + BLR */
    u32 mkdir_split_call;       /* VFiPFDIR_p_mkdir call to SplitPath */
    u32 mkdir_split_opcode;     /* exact original BL */
    u32 mkdir_parent_result;    /* MOV W21,W0 after GetEntryOfPath */
    u32 mkdir_parent_result_opcode; /* exact original MOV */
    u32 mkdir_local_error;      /* shared local validation return-2 site */
    u32 mkdir_local_error_opcode; /* exact original MOV W21,#2 */
    u32 parse_shortname_entry;  /* VFiPFPATH_parseShortName entry */
    u32 parse_shortname_entry_opcode; /* exact original prologue */
};

/* SHA-256 hash prefix (first 8 bytes) for each supported FS version.
 * These identify the exact FS binary. */
extern const u8 g_fs_hashes[FsVer_Count][8];

/* Offset table for each supported FS version. */
extern const FsCodecvtOffsets g_fs_codecvt_offsets[FsVer_Count];
