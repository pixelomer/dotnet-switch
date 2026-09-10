// Test double for libnx; never shipped in a Switch build.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
typedef uint32_t Result;
typedef pthread_mutex_t Mutex;
typedef struct VirtmemReservation { void *address; size_t bytes; } VirtmemReservation;
#define R_FAILED(x) ((x)!=0)
static inline void mutexLock(Mutex *m) { pthread_mutex_lock(m); }
static inline void mutexUnlock(Mutex *m) { pthread_mutex_unlock(m); }
bool envIsSyscallHinted(unsigned);
void virtmemLock(void);
void virtmemUnlock(void);
void *virtmemFindStack(size_t,size_t);
VirtmemReservation *virtmemAddReservation(void*,size_t);
void virtmemRemoveReservation(VirtmemReservation*);
Result svcMapMemory(void*,void*,size_t);
Result svcUnmapMemory(void*,void*,size_t);
