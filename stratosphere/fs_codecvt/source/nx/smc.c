/**
 * @file smc.c
 * @brief Secure monitor call wrappers.
 *
 * Based on emuMMC's smc.c, simplified for fs_codecvt.
 */

#include "nx/smc.h"
#include "nx/svc.h"
#include "utils/fatal.h"
#include <string.h>

Result smcGenerateRandomBytes(void *dst, u32 size) {
    SecmonArgs args;
    args.X[0] = 0xC3000006;     /* smcGenerateRandomBytes */
    args.X[1] = size;
    Result rc = svcCallSecureMonitor(&args);
    if (rc == 0) {
        if (args.X[0] != 0) {
            rc = (26u | ((u32)args.X[0] << 9u));
        }
        if (rc == 0) {
            memcpy(dst, &args.X[1], size);
        }
    }
    return rc;
}

u64 smcGenerateRandomU64(void) {
    u64 random = 0x70000000ULL;
    smcGenerateRandomBytes(&random, sizeof(random));
    /* If SMC fails, fallback address is used — no abort needed. */
    return random;
}
