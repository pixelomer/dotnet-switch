// Test only memory owned/reserved by this application; no global settings.
#include <switch.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <malloc.h>

size_t __nx_heap_size = 64 * 1024 * 1024;
static FILE *logfile;
#define LOG(...) do { fprintf(logfile, __VA_ARGS__); fflush(logfile); } while (0)
static u64 info(InfoType type) {
    u64 value=0;Result rc=svcGetInfo(&value,type,CUR_PROCESS_HANDLE,0);
    LOG("info,%u,rc=%08x,value=%llu\n",type,rc,(unsigned long long)value);
    return value;
}
static void alias_test(void) {
    const size_t reserved=64*1024*1024,chunk=2*1024*1024;
    if(!envIsSyscallHinted(0x04)||!envIsSyscallHinted(0x05)) {LOG("ALIAS_SKIP syscalls unavailable\n");return;}
    unsigned char *backing=aligned_alloc(0x1000,chunk);
    if(!backing){LOG("ALIAS_FAIL allocation\n");return;}
    virtmemLock();
    unsigned char *address=virtmemFindStack(reserved,0x1000);
    VirtmemReservation *reservation=address?virtmemAddReservation(address,reserved):NULL;
    virtmemUnlock();
    if(!reservation){free(backing);LOG("ALIAS_FAIL reservation\n");return;}
    LOG("ALIAS_BEGIN virtual=%p reserve=%zu backing=%p commit=%zu\n",(void*)address,reserved,(void*)backing,chunk);
    unsigned passes=0;bool mapped=false,half=false;
    for(unsigned i=0;i<128;i++) {
        memset(backing,0,chunk);
        Result rc=svcMapMemory(address,backing,chunk);
        if(R_FAILED(rc)){LOG("ALIAS_MAP_FAIL %08x\n",rc);break;}
        mapped=true;
        bool valid=true;for(size_t j=0;j<chunk;j++)if(address[j]){valid=false;break;}
        memset(address,0xa5,chunk);
        rc=svcUnmapMemory(address,backing,chunk/2);
        if(R_FAILED(rc)){LOG("ALIAS_DECOMMIT_FAIL %08x\n",rc);break;}
        half=true;
        memset(backing,0,chunk/2);
        rc=svcMapMemory(address,backing,chunk/2);
        if(R_FAILED(rc)){LOG("ALIAS_RECOMMIT_FAIL %08x\n",rc);break;}
        half=false;
        for(size_t j=0;j<chunk/2;j++)if(address[j]){valid=false;break;}
        for(size_t j=chunk/2;j<chunk;j++)if(address[j]!=0xa5){valid=false;break;}
        rc=svcUnmapMemory(address,backing,chunk);
        if(R_FAILED(rc)){LOG("ALIAS_RELEASE_FAIL %08x\n",rc);break;}
        mapped=false;
        for(size_t j=chunk/2;j<chunk;j++)if(backing[j]!=0xa5){valid=false;break;}
        if(!valid){LOG("ALIAS_FAIL data at %u\n",i);break;}
        passes++;
    }
    if(mapped) {
        size_t offset=half?chunk/2:0;
        Result rc=svcUnmapMemory(address+offset,backing+offset,chunk-offset);
        if(R_FAILED(rc)){LOG("ALIAS_CLEANUP_FAIL %08x; exiting without freeing mapped backing\n",rc);return;}
    }
    virtmemLock();virtmemRemoveReservation(reservation);virtmemUnlock();
    free(backing);
    LOG("ALIAS_END passes=%u/128 backing returned to allocator\n",passes);
}
static bool zeroed(const unsigned char *p,size_t n) {
    for(size_t i=0;i<n;i++) if(p[i]) return false;
    return true;
}
int main(void) {
    mkdir("sdmc:/switch/dotnet-runtime-tests",0777);
    logfile=fopen("sdmc:/switch/dotnet-runtime-tests/virtual-memory.txt","w");
    if(!logfile) return 1;
    LOG("VM_PROBE_BEGIN\n");
    info(InfoType_TotalMemorySize);info(InfoType_UsedMemorySize);
    u64 sys=info(InfoType_SystemResourceSizeTotal);
    info(InfoType_SystemResourceSizeUsed);
    u64 alias=info(InfoType_AliasRegionAddress),alias_size=info(InfoType_AliasRegionSize);
    info(InfoType_HeapRegionAddress);info(InfoType_HeapRegionSize);
    info(InfoType_AslrRegionAddress);info(InfoType_AslrRegionSize);
    LOG("hint,map=%u,unmap=%u,heap_override=%u\n",envIsSyscallHinted(0x2c),envIsSyscallHinted(0x2d),envHasHeapOverride());
    if(!envIsSyscallHinted(0x2c)||!envIsSyscallHinted(0x2d)||!sys) {
        LOG("SKIP physical mapping not advertised or no system resource allocation\n");
        goto end;
    }
    const size_t reserve=64*1024*1024, chunk=2*1024*1024;
    // Physical mapping is restricted to the alias region. Select only an
    // actually-unmapped slice, protect selection/reservation with libnx's lock.
    unsigned char *address=NULL;VirtmemReservation *reservation=NULL;
    virtmemLock();
    for(u64 cursor=alias;cursor<alias+alias_size;) {
        MemoryInfo mi;u32 page;Result rc=svcQueryMemory(&mi,&page,cursor);
        if(R_FAILED(rc)||mi.size==0)break;
        u64 candidate=(cursor+0x1fffff)&~0x1fffffull;
        if(mi.type==MemType_Unmapped && alias_size>=reserve && mi.size>=reserve && candidate>=alias && candidate<=alias+alias_size-reserve && candidate>=mi.addr && candidate<=mi.addr+mi.size-reserve) {
            address=(void*)candidate;
            reservation=virtmemAddReservation(address,reserve);
            break;
        }
        if(mi.addr+mi.size<=cursor)break;
        cursor=mi.addr+mi.size;
    }
    virtmemUnlock();
    if(!reservation) {LOG("FAIL no reservable alias slice\n");goto end;}
    LOG("reserved,%p,%zu\n",(void*)address,reserve);
    u64 before=info(InfoType_UsedMemorySize);
    unsigned passes=0;
    for(unsigned i=0;i<128;i++) {
        Result rc=svcMapPhysicalMemory(address,chunk);
        if(R_FAILED(rc)){LOG("map_failed,iteration=%u,rc=%08x\n",i,rc);break;}
        bool fresh=zeroed(address,chunk);
        memset(address,0xa5,chunk);
        // Decommit and recommit half of a live region; the retained half must
        // preserve data while the remapped half must be zero-initialized.
        rc=svcUnmapPhysicalMemory(address,chunk/2);
        if(R_FAILED(rc)){LOG("decommit_failed,%08x\n",rc);svcUnmapPhysicalMemory(address,chunk);break;}
        rc=svcMapPhysicalMemory(address,chunk/2);
        if(R_FAILED(rc)){LOG("recommit_failed,%08x\n",rc);svcUnmapPhysicalMemory(address+chunk/2,chunk/2);break;}
        bool half_zero=zeroed(address,chunk/2),retained=true;
        for(size_t j=chunk/2;j<chunk;j++)if(address[j]!=0xa5){retained=false;break;}
        u64 during=0;svcGetInfo(&during,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
        rc=svcUnmapPhysicalMemory(address,chunk);
        if(R_FAILED(rc)){LOG("release_failed,%08x\n",rc);break;}
        u64 after=0;svcGetInfo(&after,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
        if(i==0||i==127)LOG("cycle,%u,fresh=%u,half_zero=%u,retained=%u,used_before=%llu,during=%llu,after=%llu\n",i,fresh,half_zero,retained,(unsigned long long)before,(unsigned long long)during,(unsigned long long)after);
        if(!fresh||!half_zero||!retained||after!=before){LOG("FAIL invariant at cycle %u\n",i);break;}
        passes++;
    }
    virtmemLock();virtmemRemoveReservation(reservation);virtmemUnlock();
    LOG("cycles_passed=%u/128\n",passes);
end:
    alias_test();
    LOG("VM_PROBE_END\n");fclose(logfile);
    svcExitProcess();
}
