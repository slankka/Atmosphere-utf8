/* Minimal ELF dynamic relocation processor for KIPs. */

#include "utils/types.h"

#define R_AARCH64_RELATIVE 0x403
#define DT_NULL    0
#define DT_RELA    7
#define DT_RELASZ  8

typedef struct {
    s64 d_tag;
    union { u64 d_val; u64 d_ptr; } d_un;
} Elf64_Dyn;

typedef struct {
    u64 r_offset;
    u64 r_info;
    s64 r_addend;
} Elf64_Rela;

static inline u32 ELF64_R_TYPE(u64 info) { return (u32)info; }

void __nx_dynamic(uintptr_t base, const void *dynamic_ptr) {
    const Elf64_Dyn *dyn = (const Elf64_Dyn *)dynamic_ptr;

    const Elf64_Rela *rela = (const Elf64_Rela *)0;
    u64 rela_count = 0;

    /* Scan .dynamic section to find RELA info */
    for (const Elf64_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        if (d->d_tag == DT_RELA) {
            rela = (const Elf64_Rela *)(base + d->d_un.d_ptr);
        } else if (d->d_tag == DT_RELASZ) {
            rela_count = d->d_un.d_val / sizeof(Elf64_Rela);
        }
    }

    if (rela == (const Elf64_Rela *)0) return;

    for (; rela_count--; ++rela) {
        if (ELF64_R_TYPE(rela->r_info) == R_AARCH64_RELATIVE) {
            u64 *patch_addr = (u64 *)(base + rela->r_offset);
            *patch_addr = base + rela->r_addend;
        }
    }
}
