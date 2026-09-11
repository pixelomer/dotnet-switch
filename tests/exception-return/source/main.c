#include <switch.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/stat.h>

uint64_t probe_fault_count;
typedef struct {
    uint64_t result,x9,x19,x1,nzcv,tp_before,tp_after,tp_entry;
    unsigned char q0[16];
} ProbeResult;
void probe_fault_roundtrip(ProbeResult *result, uintptr_t temporary_tp);
#define ROUNDS 1024
#define WORKERS 4
static atomic_bool start;
typedef struct {unsigned passes; ProbeResult first,last; bool nonzero;} Worker;
static void run(void *arg) {
    Worker *w=arg;
    while(!atomic_load_explicit(&start,memory_order_acquire)) svcSleepThread(1000000);
    for(unsigned i=0;i<ROUNDS;i++) {
        ProbeResult r={0};
        uintptr_t tp=w->nonzero?(uintptr_t)&r:0;
        probe_fault_roundtrip(&r,tp);
        bool simd=true;for(unsigned j=0;j<16;j++)if(r.q0[j]!=0x5a)simd=false;
        bool valid=r.result==0x42 && r.x9==0x99 && r.x19==0x1919 && r.x1==0x1111 && r.nzcv==0x60000000 && simd && r.tp_before==tp && r.tp_entry==tp && r.tp_after==tp;
        if(!i)w->first=r;
        w->last=r;
        if(!valid)break;
        w->passes++;
    }
}
int main(void) {
    mkdir("sdmc:/switch/dotnet-runtime-tests",0777);
    FILE *f=fopen("sdmc:/switch/dotnet-runtime-tests/exception-return.txt","w");
    if(!f) return 1;
    fprintf(f,"EXCEPTION_PROBE_BEGIN workers=%u rounds=%u\n",WORKERS,ROUNDS);fflush(f);
    Thread threads[WORKERS];Worker workers[WORKERS]={0};unsigned created=0,started=0;
    for(unsigned i=0;i<WORKERS;i++) {
        workers[i].nonzero=(i&1)!=0;
        Result rc=threadCreate(&threads[i],run,&workers[i],NULL,0x10000,0x2c,-2);
        if(R_FAILED(rc)){fprintf(f,"thread_create_failed=%x\n",rc);break;}
        created++;
        rc=threadStart(&threads[i]);
        if(R_FAILED(rc)){fprintf(f,"thread_start_failed=%x\n",rc);break;}
        started++;
    }
    atomic_store_explicit(&start,true,memory_order_release);
    unsigned total=0;
    for(unsigned i=0;i<started;i++) {
        threadWaitForExit(&threads[i]);Worker *w=&workers[i];ProbeResult *r=&w->first;
        fprintf(f,"worker=%u nonzero_tp=%u passes=%u/%u tp_before=%llx tp_entry=%llx tp_after=%llx\n",i,w->nonzero,w->passes,ROUNDS,(unsigned long long)r->tp_before,(unsigned long long)r->tp_entry,(unsigned long long)r->tp_after);
        total+=w->passes;
    }
    for(unsigned i=0;i<created;i++) threadClose(&threads[i]);
    fprintf(f,"EXCEPTION_PROBE_END passes=%u/%u caught=%llu\n",total,WORKERS*ROUNDS,(unsigned long long)probe_fault_count);
    fclose(f);return 0;
}
