#include <switch.h>
#include "nxvm.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define PAGE ((size_t)4096)
#define MIB ((size_t)1024 * 1024)
static FILE *logfile;
#define CHECK(x) do { if (!(x)) { fprintf(logfile,"FAIL line=%d: %s\n",__LINE__,#x); fflush(logfile); return false; } } while (0)
static bool zeroed(const unsigned char *p, size_t n) {
    for (size_t i=0; i<n; i++) if (p[i]) return false;
    return true;
}
static bool run_basic(void) {
    CHECK(nxvm_init(4*MIB));
    CHECK(!nxvm_init(4*MIB));
    CHECK(!nxvm_reserve(0,PAGE));
    CHECK(!nxvm_reserve(PAGE,3*PAGE));
    unsigned char *a=nxvm_reserve(8*MIB,2*MIB);
    unsigned char *b=nxvm_reserve(8*MIB,PAGE);
    CHECK(a && b && !((uintptr_t)a & (2*MIB-1)));
    CHECK(!nxvm_destroy());
    CHECK(!nxvm_commit(a+1,PAGE));
    CHECK(!nxvm_commit(a,0));
    CHECK(!nxvm_commit(a+8*MIB,PAGE));
    CHECK(!nxvm_release(a,PAGE));
    CHECK(nxvm_commit(a,4*MIB));
    CHECK(zeroed(a,4*MIB));
    memset(a,0x91,4*MIB);
    CHECK(nxvm_commit(a,4*MIB));
    CHECK(a[0]==0x91); // Idempotent commitment must preserve data.
    CHECK(!nxvm_commit(b,PAGE)); // Pool exhausted without changing live pages.
    CHECK(nxvm_stats().committed==4*MIB);
    for(size_t i=0;i<4*MIB;i+=2*PAGE) CHECK(nxvm_decommit(a+i,PAGE));
    CHECK(nxvm_stats().committed==2*MIB);
    CHECK(nxvm_commit(b,2*MIB)); // Noncontiguous backing, contiguous destination.
    CHECK(zeroed(b,2*MIB));
    memset(b,0x62,2*MIB);
    for(size_t i=PAGE;i<4*MIB;i+=2*PAGE) CHECK(a[i]==0x91);
    CHECK(nxvm_release(b,8*MIB));
    CHECK(!nxvm_release(b,8*MIB));
    CHECK(nxvm_commit(a,4*MIB));
    for(size_t i=0;i<4*MIB;i+=2*PAGE) CHECK(zeroed(a+i,PAGE));
    CHECK(nxvm_decommit(a,4*MIB));
    CHECK(nxvm_decommit(a,4*MIB));
    CHECK(nxvm_stats().committed==0);
    for(unsigned cycle=0;cycle<128;cycle++) {
        CHECK(nxvm_commit(a,4*MIB));
        CHECK(zeroed(a,4*MIB));
        memset(a,0xac,4*MIB);
        CHECK(nxvm_decommit(a+MIB,2*MIB));
        CHECK(nxvm_commit(a+MIB,2*MIB));
        CHECK(zeroed(a+MIB,2*MIB));
        CHECK(a[0]==0xac && a[3*MIB]==0xac);
        CHECK(nxvm_decommit(a,4*MIB));
        CHECK(nxvm_stats().committed==0);
    }
    CHECK(nxvm_release(a,8*MIB));
    CHECK(nxvm_stats().reservations==0 && nxvm_stats().reserved==0);
    fprintf(logfile,"BASIC_PASS cycles=128 fragmentation=512_holes exhaustion=checked\n");fflush(logfile);
    return true;
}

typedef struct { unsigned id, passes; } Worker;
static void worker(void *arg) {
    Worker *w=arg;
    for(unsigned cycle=0;cycle<128;cycle++) {
        unsigned char *p=nxvm_reserve(64*PAGE,PAGE);
        if(!p) return;
        if(!nxvm_commit(p,16*PAGE) || !zeroed(p,16*PAGE)) return;
        memset(p,w->id+1,16*PAGE);
        svcSleepThread(10000);
        for(size_t i=0;i<16*PAGE;i++) if(p[i]!=w->id+1) return;
        if(!nxvm_decommit(p+PAGE,8*PAGE) || !nxvm_commit(p+PAGE,8*PAGE)) return;
        if(!zeroed(p+PAGE,8*PAGE) || p[0]!=w->id+1 || p[9*PAGE]!=w->id+1) return;
        if(!nxvm_release(p,64*PAGE)) return;
        w->passes++;
    }
}
static bool run_threads(void) {
    Thread threads[4]; Worker workers[4]={0};
    for(unsigned i=0;i<4;i++) {
        workers[i].id=i;
        CHECK(R_SUCCEEDED(threadCreate(&threads[i],worker,&workers[i],NULL,64*1024,0x2c,-2)));
        CHECK(R_SUCCEEDED(threadStart(&threads[i])));
    }
    for(unsigned i=0;i<4;i++) {
        CHECK(R_SUCCEEDED(threadWaitForExit(&threads[i])));
        CHECK(R_SUCCEEDED(threadClose(&threads[i])));
        CHECK(workers[i].passes==128);
    }
    NxvmStats s=nxvm_stats();
    CHECK(!s.poisoned && !s.last_svc_error && !s.committed && !s.reserved && !s.reservations);
    fprintf(logfile,"THREAD_PASS workers=4 cycles_each=128 committed=%zu reserved=%zu\n",s.committed,s.reserved);fflush(logfile);
    CHECK(nxvm_destroy());
    CHECK(nxvm_init(MIB));
    CHECK(nxvm_destroy());
    return true;
}
int main(void) {
    mkdir("sdmc:/switch/dotnet-runtime-tests",0777);
    logfile=fopen("sdmc:/switch/dotnet-runtime-tests/allocator.txt","w");
    if(!logfile) return 1;
    fprintf(logfile,"ALLOCATOR_BEGIN\n");fflush(logfile);
    bool success=run_basic() && run_threads();
    NxvmStats s=nxvm_stats();
    fprintf(logfile,"ALLOCATOR_END success=%u committed=%zu reserved=%zu poison=%u svc=%08x\n",success,s.committed,s.reserved,s.poisoned,s.last_svc_error);
    fclose(logfile);
    svcExitProcess();
}
