/**
 * @file svc.h
 * @brief Wrappers for kernel syscalls.
 */
#pragma once
#include "../utils/types.h"

/// Memory information structure.
typedef struct {
    u64 addr;
    u64 size;
    u32 type;
    u32 attr;
    u32 perm;
    u32 device_refcount;
    u32 ipc_refcount;
    u32 padding;
} MemoryInfo;

/// Secure monitor arguments.
typedef struct {
    u64 X[8];
} SecmonArgs;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
_Static_assert(sizeof(SecmonArgs) == 0x40, "SecmonArgs size");
#endif

Result svcSetHeapSize(void* addr, u64 size);
Result svcSetMemoryPermission(void* addr, u64 size, u32 perm);
Result svcSetMemoryAttribute(void* addr, u64 size, u32 state0, u32 state1);
Result svcMapMemory(void* dst, void* src, u64 size);
Result svcUnmapMemory(void* dst, void* src, u64 size);
Result svcQueryMemory(MemoryInfo* meminfo, u32* pageinfo, u64 addr);
Result svcExitProcess(void);
Result svcCreateThread(Handle* out, void* entry, void* arg, void* stack_top, int prio, int cpuid);
Result svcStartThread(Handle handle);
Result svcExitThread(void);
Result svcSleepThread(s64 ns);
Result svcCloseHandle(Handle handle);
Result svcWaitSynchronization(s32* index, const Handle* handles, s32 num, s64 timeout);
Result svcCreateSession(Handle* server, Handle* client, u32 unk0, u32 unk1);
Result svcAcceptSession(Handle* session, Handle port);
Result svcReplyAndReceive(s32* index, const Handle* handles, s32 num, Handle reply, s64 timeout);
Result svcSendSyncRequest(Handle session);
Result svcCreateEvent(Handle* wevent, Handle* revent);
Result svcMapProcessMemory(void* dst, Handle proc, u64 src, u64 size);
Result svcUnmapProcessMemory(void* dst, Handle proc, u64 src, u64 size);
Result svcSetProcessMemoryPermission(Handle proc, u64 addr, u64 size, u32 perm);
Result svcCallSecureMonitor(SecmonArgs* args);

#ifdef __cplusplus
}
#endif

u64 svcGetCurrentProcessorNumber(void);

#ifdef __cplusplus
extern "C" {
#endif

void* armGetTls(void);

#ifdef __cplusplus
}
#endif
