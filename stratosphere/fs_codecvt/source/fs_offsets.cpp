/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * Offset tables for each supported FS version (exFAT variants, 19.0.0–22.5.0).
 *
 * Offsets derived from nx-filesystem-utf8 project's build_patches22.py.
 * All values are relative to the decompressed FS .text section. The KIP1
 * header is not part of the runtime mapping addressed by fs_code_base.
 */

#include "fs_offsets.h"

/* SHA-256 hash prefixes (first 8 bytes) — matches fusee_stratosphere.cpp */
const u8 g_fs_hashes[FsVer_Count][8] = {
    /* FsVer_19_0_0_Exfat */
    { 0xED, 0xA8, 0x78, 0x68, 0xA4, 0x49, 0x07, 0x50 },
    /* FsVer_20_2_0_Exfat — NOTE: 20.2.0 is between 20.1.0 and 21.0.0,
       using 20.0.0 ExFAT hash as closest match */
    { 0x47, 0x41, 0x07, 0x10, 0x65, 0x4F, 0xA4, 0x3F },
    /* FsVer_21_2_0_Exfat */
    { 0x56, 0x25, 0x17, 0xA1, 0x92, 0xC3, 0xC8, 0xF0 },
    /* FsVer_22_0_0_Exfat */
    { 0xFB, 0x0B, 0x68, 0xDB, 0x24, 0x03, 0xD1, 0x19 },
    /* FsVer_22_5_0_Exfat */
    { 0xD4, 0x45, 0x28, 0x29, 0x5B, 0x41, 0x92, 0xBA },
};

/*
 * Size of the unicode2oem replacement code (~0x130 bytes).
 * This determines where the NOP cave starts.
 */
#define UNI2OEM_CODE_SIZE 0x130

/*
 * Offset tables. Register usage:
 *   19.x/20.x:   name_reg=25 (X25), bound_reg=19 (X19), byte_reg=9 (W9)
 *   21.2.0:      name_reg=28 (X28), bound_reg=9  (X9),  byte_reg=10 (W10)
 *   22.x+:       name_reg=26 (X26), bound_reg=24 (X24), byte_reg=9 (W9)
 */
const FsCodecvtOffsets g_fs_codecvt_offsets[FsVer_Count] = {
    /* FsVer_19_0_0_Exfat */
    {
        { 0xFEAC0, 0xFEC30, 0xFEE00, 0xFEE40, 0xFEE90, 0xFEEA0 },
        { 0xF5888, 0xF5B48, 0xF6638 },
        0xD4AF4, 0xD4A90, 0xD4AF8, 0xD4B20,
        25, 19, 9, 0,
        0xFEC30 + UNI2OEM_CODE_SIZE,  /* cave */
        0xFEE00 - (0xFEC30 + UNI2OEM_CODE_SIZE), /* cave_size */
        { 0x39C00008, 0x12001D0A }, 0x54FFFCE3,
    },
    /* FsVer_20_2_0_Exfat */
    {
        { 0x109440, 0x1095B0, 0x109780, 0x1097C0, 0x109810, 0x109820 },
        { 0x100618, 0x1008D8, 0x1013C8 },
        0xDE6B4, 0xDE650, 0xDE6B8, 0xDE6E0,
        25, 19, 9, 0,
        0x1095B0 + UNI2OEM_CODE_SIZE,
        0x109780 - (0x1095B0 + UNI2OEM_CODE_SIZE),
        { 0x39C00008, 0x12001D0A }, 0x54FFFCE3,
    },
    /* FsVer_21_2_0_Exfat — full codecvt + dir hook (capstone-verified).
     * Dir hook at 0xE48D0 (b.lo→0xE4864), byte in W10 (ldrb w10,[x28,x8]).
     * Control flow: W10/X8/X28/W20, differs from 22.x W9/X8/X26/X24. */
    {
        { 0x10ECC0, 0x10EE10, 0x10EFE0, 0x10F020, 0x10F070, 0x10F080 },
        { 0, 0, 0 },
        0xE48D0, 0xE4864, 0xE48D4, 0xE48C0,
        28, 9, 10, 0,
        0x10EE10 + UNI2OEM_CODE_SIZE,
        0x10EFE0 - (0x10EE10 + UNI2OEM_CODE_SIZE),
        { 0x39C00008, 0x12001D0B }, 0x54FFFCA3,
    },
    /* FsVer_22_0_0_Exfat */
    {
        { 0x111520, 0x111670, 0x111840, 0x111880, 0x1118D0, 0x1118E0 },
        { 0x108784, 0x108A30, 0x109584 },
        0xE7040, 0xE70C0, 0xE7044, 0xE7088,
        26, 24, 9, 0,
        0x111670 + UNI2OEM_CODE_SIZE,
        0x111840 - (0x111670 + UNI2OEM_CODE_SIZE),
        { 0x39C00008, 0x12001D0B }, 0x54000409,
    },
    /* FsVer_22_5_0_Exfat — identical to 22.0.0 */
    {
        { 0x111520, 0x111670, 0x111840, 0x111880, 0x1118D0, 0x1118E0 },
        { 0x108784, 0x108A30, 0x109584 },
        0xE7040, 0xE70C0, 0xE7044, 0xE7088,
        26, 24, 9, 0,
        0x111670 + UNI2OEM_CODE_SIZE,
        0x111840 - (0x111670 + UNI2OEM_CODE_SIZE),
        { 0x39C00008, 0x12001D0B }, 0x54000409,
    },
};
