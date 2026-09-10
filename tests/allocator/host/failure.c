#define _GNU_SOURCE
#include "switch.h"
#include "nxvm.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAGE 4096
static unsigned maps, fail_map_at;
static bool fail_unmap;
static void *pending;
static size_t pending_bytes;
bool envIsSyscallHinted(unsigned n) { return n==4 || n==5; }
void virtmemLock(void) {}
void virtmemUnlock(void) {}
void *virtmemFindStack(size_t bytes,size_t guard) {
    (void)guard;
    assert(!pending);
    pending=mmap(NULL,bytes,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(pending==MAP_FAILED) { pending=NULL;return NULL; }
    pending_bytes=bytes;
    return pending;
}
VirtmemReservation *virtmemAddReservation(void *p,size_t n) {
    assert(p==pending && n==pending_bytes); // This test uses page alignment.
    VirtmemReservation *r=malloc(sizeof(*r));assert(r);
    *r=(VirtmemReservation){p,n};pending=NULL;return r;
}
void virtmemRemoveReservation(VirtmemReservation *r) {
    assert(!munmap(r->address,r->bytes));free(r);
}
// Simulate inaccessible backing and accessible destination with copies. These
// doubles test allocator ownership/rollback, not Horizon mapping behavior.
Result svcMapMemory(void *to,void *from,size_t bytes) {
    if(++maps==fail_map_at) return 0x1234;
    assert(!mprotect(to,bytes,PROT_READ|PROT_WRITE));
    memcpy(to,from,bytes);
    assert(!mprotect(from,bytes,PROT_NONE));
    return 0;
}
Result svcUnmapMemory(void *from,void *to,size_t bytes) {
    if(fail_unmap) return 0x5678;
    assert(!mprotect(to,bytes,PROT_READ|PROT_WRITE));
    memcpy(to,from,bytes);
    assert(!mprotect(from,bytes,PROT_NONE));
    return 0;
}
static void rollback_test(void) {
    assert(nxvm_init(6*PAGE));
    unsigned char *a=nxvm_reserve(8*PAGE,PAGE),*b=nxvm_reserve(8*PAGE,PAGE);
    assert(a && b && nxvm_commit(a,4*PAGE));
    memset(a,0xa7,4*PAGE);
    assert(nxvm_decommit(a+PAGE,PAGE));
    assert(nxvm_decommit(a+3*PAGE,PAGE));
    // The first hole commits, the second mapping fails. Rollback must return
    // just that first hole and leave pre-existing mappings/data intact.
    fail_map_at=maps+2;
    assert(!nxvm_commit(b,2*PAGE));
    NxvmStats s=nxvm_stats();
    assert(s.committed==2*PAGE && !s.poisoned && s.last_svc_error==0x1234);
    assert(a[0]==0xa7 && a[2*PAGE]==0xa7);
    fail_map_at=0;
    assert(nxvm_commit(b,4*PAGE)); // Uses every remaining backing page.
    for(size_t i=0;i<4*PAGE;i++) assert(!b[i]);
    assert(nxvm_release(b,8*PAGE) && nxvm_release(a,8*PAGE));
    assert(!nxvm_stats().committed && !nxvm_stats().reserved);
    assert(nxvm_destroy());
}
static void poison_test(void) {
    assert(nxvm_init(2*PAGE));
    void *p=nxvm_reserve(4*PAGE,PAGE);assert(p && nxvm_commit(p,PAGE));
    fail_unmap=true;
    assert(!nxvm_release(p,4*PAGE));
    NxvmStats s=nxvm_stats();
    assert(s.poisoned && s.committed==PAGE && s.reserved==4*PAGE && s.reservations==1);
    assert(!nxvm_destroy() && !nxvm_commit(p,PAGE));
    // Poisoned ownership is deliberately retained until process teardown.
}
int main(void) {
    for(unsigned i=0;i<1000;i++) rollback_test();
    pid_t child=fork();assert(child>=0);
    if(!child) { poison_test();_exit(0); }
    int status=0;assert(waitpid(child,&status,0)==child);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    puts("HOST_PASS rollback_cycles=1000 failed_unmap_retains_ownership=1");
    return 0;
}
