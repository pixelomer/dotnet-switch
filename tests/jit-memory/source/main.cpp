// Original generic JIT-memory probe; libnx's public API supplies the mappings.
#include <switch.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <pthread.h>

extern "C" { u32 __nx_applet_exit_mode = 1; }
static FILE* logFile;
static unsigned long checks;
static void check(bool condition, const char* name)
{
    ++checks;
    if (!condition) {
        fprintf(logFile, "FAIL %s checks=%lu\n", name, checks);
        fflush(logFile);
        abort();
    }
}
static void result(Result rc, const char* name)
{
    if (R_FAILED(rc)) {
        fprintf(logFile, "RESULT %s rc=%08x\n", name, rc);
        fflush(logFile);
    }
    check(R_SUCCEEDED(rc), name);
}
using Function = uint64_t (*)();
struct Work {
    Function function;
    uint64_t expected;
    std::atomic<unsigned> errors{0};
};
static void* worker(void* arg)
{
    Work* work = static_cast<Work*>(arg);
    for (unsigned n = 0; n < 4096; ++n)
        if (work->function() != work->expected)
            work->errors.fetch_add(1, std::memory_order_relaxed);
    return nullptr;
}
static void checkOnWorkers(Function function, uint64_t expected)
{
    Work work{function, expected};
    pthread_t threads[4];
    for (auto& thread : threads)
        check(pthread_create(&thread, nullptr, worker, &work) == 0, "pthread_create");
    for (auto& thread : threads)
        check(pthread_join(thread, nullptr) == 0, "pthread_join");
    check(work.errors.load() == 0, "cross-thread code execution");
}
static void permissions(void* address, uint32_t expected, const char* name)
{
    MemoryInfo info{}; u32 pageInfo;
    result(svcQueryMemory(&info, &pageInfo, reinterpret_cast<u64>(address)), "query memory");
    check(info.perm == expected, name);
}
static void emitConstant(Jit& code, unsigned value)
{
    result(jitTransitionToWritable(&code), "writable transition");
    // AArch64 MOVZ X0, imm16; RET X30, encoded from the architectural instruction format.
    const uint32_t words[] = {0xd2800000u | (value << 5), 0xd65f03c0u};
    memcpy(jitGetRwAddr(&code), words, sizeof(words));
    result(jitTransitionToExecutable(&code), "executable transition/cache publication");
}
int main()
{
    logFile = fopen("sdmc:/switch/dotnet-jit-memory-probe.txt", "w");
    if (!logFile) return 1;
    setvbuf(logFile, nullptr, _IONBF, 0);
    fprintf(logFile, "BEGIN JIT primitive; no managed runtime\n");
    fprintf(logFile, "hints create=%d control=%d map_process=%d unmap_process=%d protect_process=%d\n",
        envIsSyscallHinted(0x4b), envIsSyscallHinted(0x4c), envIsSyscallHinted(0x77),
        envIsSyscallHinted(0x78), envIsSyscallHinted(0x73));

    // All generated code is retired after joins. No code is patched while executing.
    for (unsigned round = 0; round < 16; ++round) {
        for (unsigned cycle = 0; cycle < 16; ++cycle) {
            Jit code{};
            result(jitCreate(&code, 8192), "jitCreate");
            if (round == 0 && cycle == 0)
                fprintf(logFile, "backend=%u rw=%p rx=%p source=%p size=%zu\n",
                    unsigned(code.type), code.rw_addr, code.rx_addr, code.src_addr, code.size);
            auto function = reinterpret_cast<Function>(jitGetRxAddr(&code));
            emitConstant(code, 123);
            check(function() == 123, "initial generated function");
            permissions(code.rx_addr, Perm_Rx, "RX alias permissions");
            if (code.type == JitType_CodeMemory) {
                permissions(code.rw_addr, Perm_Rw, "RW alias permissions");
                check(code.rw_addr != code.rx_addr, "distinct aliases");
            }
            checkOnWorkers(function, 123);
            emitConstant(code, 456);
            checkOnWorkers(function, 456);

            // Verify instruction addresses are RX addresses, not writable aliases.
            result(jitTransitionToWritable(&code), "writable for ADR");
            const uint32_t adr[] = {0x10000000u, 0xd65f03c0u}; // ADR X0, .; RET
            memcpy(code.rw_addr, adr, sizeof(adr));
            result(jitTransitionToExecutable(&code), "publish ADR");
            checkOnWorkers(function, reinterpret_cast<uint64_t>(code.rx_addr));
            result(jitClose(&code), "retire code after joins");
        }
        fprintf(logFile, "ROUND %u checks=%lu native_used=%zu\n", round, checks, mallinfo().uordblks);
    }
    fprintf(logFile, "PASS checks=%lu cycles=256 worker_threads=3072 calls=12582912\n", checks);
    fclose(logFile);
    return 0;
}
