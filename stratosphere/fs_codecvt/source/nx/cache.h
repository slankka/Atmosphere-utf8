/**
 * @file cache.h
 * @brief AArch64 cache operations.
 */
#pragma once
#include "../utils/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void armDCacheFlush(void* addr, size_t size);
void armDCacheClean(void* addr, size_t size);
void armICacheInvalidate(void* addr, size_t size);
void armDCacheZero(void* addr, size_t size);

#ifdef __cplusplus
}
#endif
