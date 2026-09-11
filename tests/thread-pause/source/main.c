#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/stat.h>

typedef struct { _Atomic uint32_t stop; uint32_t pad; _Atomic uint64_t counter; } Worker;
extern void pause_worker(void*);
extern char pause_loop_begin[],pause_loop_end[];
#define WORKERS 2
#define ROUNDS 128
int main(void) {
    mkdir("sdmc:/switch/dotnet-runtime-tests",0777);
    FILE *f=fopen("sdmc:/switch/dotnet-runtime-tests/thread-pause.txt","w");
    if(!f)return 1;
    fprintf(f,"THREAD_PAUSE_BEGIN workers=%d rounds=%d\n",WORKERS,ROUNDS);fclose(f);
    f=fopen("sdmc:/switch/dotnet-runtime-tests/thread-pause.txt","a");
    if(!f)return 1;
    Worker w[WORKERS]={0};Thread t[WORKERS];unsigned created=0,started=0,passed=0;
    bool failed=false;
    for(unsigned i=0;i<WORKERS;i++) {
        Result rc=threadCreate(&t[i],pause_worker,&w[i],NULL,0x10000,0x30,i);
        if(R_FAILED(rc)){fprintf(f,"create_fail=%x\n",rc);failed=true;break;}
        created++;
        rc=threadStart(&t[i]);
        if(R_FAILED(rc)){fprintf(f,"start_fail=%x\n",rc);failed=true;break;}
        started++;
    }
    svcSleepThread(10000000);
    for(unsigned round=0;round<ROUNDS && !failed;round++) {
        for(unsigned i=0;i<started && !failed;i++) {
            Result rc=svcSetThreadActivity(t[i].handle,ThreadActivity_Paused);
            if(R_FAILED(rc)){fprintf(f,"pause_fail=%x round=%u worker=%u\n",rc,round,i);failed=true;break;}
            ThreadContext a={0},b={0};
            Result ca=svcGetThreadContext3(&a,t[i].handle);
            uint64_t before=atomic_load_explicit(&w[i].counter,memory_order_relaxed);
            svcSleepThread(1000000);
            uint64_t after=atomic_load_explicit(&w[i].counter,memory_order_relaxed);
            Result cb=svcGetThreadContext3(&b,t[i].handle);
            bool stable=before==after && !memcmp(&a,&b,sizeof(a));
            bool loop=a.pc.x>=(uintptr_t)pause_loop_begin && a.pc.x<(uintptr_t)pause_loop_end;
            bool simd=true;for(unsigned j=0;j<16;j++)if(((unsigned char*)&a.fpu_gprs[0])[j]!=0x5a)simd=false;
            bool regs=a.cpu_gprs[0].x==(uintptr_t)&w[i] && a.cpu_gprs[19].x==0x1919 && a.tpidr==(uintptr_t)&w[i] && simd;
            rc=svcSetThreadActivity(t[i].handle,ThreadActivity_Runnable);
            if(R_FAILED(rc)) { fprintf(f,"resume_fail=%x\n",rc);fflush(f);abort(); }
            uint64_t deadline=armGetSystemTick()+armGetSystemTickFreq()/10;
            while(atomic_load_explicit(&w[i].counter,memory_order_relaxed)==after && armGetSystemTick()<deadline)svcSleepThread(100000);
            bool progress=atomic_load_explicit(&w[i].counter,memory_order_relaxed)!=after;
            bool valid=R_SUCCEEDED(ca)&&R_SUCCEEDED(cb)&&stable&&loop&&regs&&progress;
            if(!round||!valid) {
                fprintf(f,"round=%u worker=%u get=%x/%x stable=%d loop=%d regs=%d progress=%d pc=%llx tp=%llx count=%llu\n",round,i,ca,cb,stable,loop,regs,progress,(unsigned long long)a.pc.x,(unsigned long long)a.tpidr,(unsigned long long)after);fflush(f);
            }
            if(valid)passed++;else failed=true;
        }
    }
    for(unsigned i=0;i<started;i++)atomic_store_explicit(&w[i].stop,1,memory_order_release);
    for(unsigned i=0;i<started;i++)threadWaitForExit(&t[i]);
    for(unsigned i=0;i<created;i++)threadClose(&t[i]);
    fprintf(f,"THREAD_PAUSE_END passed=%u/%u failed=%d\n",passed,WORKERS*ROUNDS,failed);
    fclose(f);return 0;
}
