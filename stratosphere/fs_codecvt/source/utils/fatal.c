/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include "utils/fatal.h"
#include "nx/svc.h"
#include "nx/smc.h"

void fatal_abort(u32 reason) {
    /* Write the reason to IRAM for post-mortem analysis. */
    volatile u32 *iram = (volatile u32 *)0x4003E000;
    iram[0] = 0x30464356; /* "VFC0" = fs_codecvt magic */
    iram[1] = reason;
    iram[2] = 0x0100000000000000ULL & 0xFFFFFFFF; /* FS title_id low */
    iram[3] = 0x0100000000000000ULL >> 32;         /* FS title_id high */

    /* Trigger a reboot via SMC. */
    SecmonArgs args;
    args.X[0] = 0xC3000401; /* smcRebootToRcm */
    svcCallSecureMonitor(&args);

    /* Should never reach here. */
    while (1) { }
}
