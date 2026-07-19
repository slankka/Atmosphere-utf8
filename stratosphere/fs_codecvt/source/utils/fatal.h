/*
 * Copyright (c) 2024 Atmosphere-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#pragma once
#include "../utils/types.h"

enum FatalReason
{
    Fatal_InitMMU = 0,
    Fatal_UnknownVersion,
    Fatal_BadResult,
    Fatal_GetConfig,
    Fatal_Max
};

#ifdef __cplusplus
extern "C" {
#endif

void fatal_abort(u32 reason);

#ifdef __cplusplus
}
#endif
