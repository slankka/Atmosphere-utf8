/**
 * @file smc.h
 * @brief Secure monitor call wrappers.
 */
#pragma once
#include "../utils/types.h"
#include "svc.h"

#ifdef __cplusplus
extern "C" {
#endif

u64 smcGenerateRandomU64(void);

#ifdef __cplusplus
}
#endif
