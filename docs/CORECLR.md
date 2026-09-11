# CoreCLR on Horizon: integration boundaries

The .NET 10 runtime lineage is based on public v10.0.12, commit
4271d88e0aebf3d04f188f1334c2220d80555ef6. Keep runtime compiler, CoreLib,
framework and native archives from one matching source build.

The [runtime repository](https://github.com/pixelomer/dotnet-runtime) contains
the platform implementation. Use its
[source-build guide](https://github.com/pixelomer/dotnet-runtime/blob/main/eng/libnx/README.md)
for downloadable host/toolchain dependencies, the pinned libnx source overlay,
ICU and build commands. This repository's native probes are not a replacement
for that runtime or its managed integration workloads.

## Required interfaces

CoreCLR needs Horizon context conversion and native unwinding, not Linux
signal/context structures or fake Unix headers. The PAL must preserve the
register and unwind locations needed by moving GC.

An embedded host must supply static runtime/JIT linkage and explicit native
imports. Reading IL/metadata assemblies is separate from native-module loading;
native P/Invokes require linked exports or a supported loader.

Executable allocation needs writable/executable alias ownership, RX-relative
relocation, cache publication, bounded placement and reclamation after users
stop. The NativeAOT data allocator does not make pages executable.
CoreCLR's executable allocator and libnx's JIT API have different allocation
contracts and are not interchangeable merely because each exposes two views.

Runtime thread registration, all-thread ordering, safe points, root reporting,
TLS teardown and exception return must be integrated as one ownership contract.
A native memory or thread probe does not establish managed GC/EH correctness.

## Runtime selection

CoreCLR provides dynamic managed loading and an optimizing JIT.
NativeAOT compiles managed code ahead of time and does not by itself provide
arbitrary new IL loading or Reflection.Emit. Changing runtime version alone
does not establish compatibility with runtime hooks, code patching, private
reflection assumptions or native dependencies.

Mono interpreter/AOT/LLVM profiles have their own build and embedding contracts.
A host LLVM AOT compiler does not imply an on-device LLVM JIT.
Keep their source inputs and outputs separate from CoreCLR and NativeAOT.

Primary references:
- https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/
- https://github.com/dotnet/runtime/tree/v10.0.12/src/coreclr/pal
- https://github.com/dotnet/runtime/blob/v10.0.12/src/coreclr/utilcode/executableallocator.cpp
- https://switchbrew.github.io/libnx/jit_8h.html

## Executable mapping ownership

The runtime's VMToOSInterface uses MapProcessCodeMemory,
SetProcessMemoryPermission and MapProcessMemory with the loader's borrowed
process handle. It retains CoreCLR's executable allocator and loader-heap
logic, including dynamic interleaved stubs when template sharing is unavailable.

Writable views may overlap and retire independently. Partial commitment and
map/protect failure rollback must preserve the backing and primary mapping
while any owner remains. Cached writers publish through the PAL; generated code
uses its executable address for PC-relative access to adjacent writable data.
This process-mapping contract differs from one libnx CodeMemory object per
allocation and does not replace managed GC or exception integration.

## File-backed PE images

CoreCLR keeps its PE layout/relocation logic and uses a PAL mapping adapter.
Without a file pager, read-only views are pinned snapshots; shared writable-file
mappings fail explicitly. Positional reads use fsdevPread inside the owning
libnx driver, with an independent cursor and no copied private descriptor layout.
Use the runtime source-build guide's staged libnx overlay through LIBNX_ROOT.

Making AliasCode writable converts it to AliasCodeData, which cannot regain
execute permission. The PAL fixes execute capability at creation and supplies
temporary writer aliases for private images. The PE relocation decoder writes
through a scoped view; its primary executable address is neither unmapped nor
made writable. Independent backing/protection runs map separately and roll back
on failure. Active writers pin their primary pages and publish caches on release.

## PAL exception integration

Horizon user exceptions enter CoreCLR's SEHProcessException, heap record
promotion and PAL virtual-unwind transitions. Ordinary dispatch runs below
the original SP, supports nested faults and lets the runtime handler leave
through managed dispatch. The ARM64 RestoreCompleteContext deliberate-fault
mechanism uses kernel-assisted return to preserve X16/X17; no Unix signal ABI
is fabricated.

SEH-enabled PAL threads use the common Horizon thread registry shared with
NativeAOT. Process write-buffer flushing uses its synchronized kernel
pause/context protocol. Shared-memory mapping retains its object logic and
uses the PAL adapter. Unsupported cross-process file locks, writable shared-file
mappings, subprocess dumps and activation fail explicitly.
Native fault handling alone does not establish managed GC/EH correctness.

## Resident native modules

The PAL retains module management and delegates resident-NRO lookup to the
Horizon adapter. It uses NRO segments/BSS, the homebrew loader's argv path and
standard System V ELF dynamic symbol/hash metadata. Linkers must retain exports
explicitly; hidden, TLS and undefined symbols are rejected.
Releasing a module reference does not unload the resident NRO, and external
native-module loading fails explicitly. Managed IL loading is separate.

Resident metadata uses the section-bound code address and hidden PC-relative
references. Do not substitute fabricated POSIX process or synchronization
success for missing platform operations: PAL thread and synchronization state
machines depend on real native lifetime and waiting semantics.

## PAL startup and synchronization

The PAL object manager, synchronization worker, thread startup handshake and
resume semaphore retain their existing state machines. Worker commands use a
bounded native mutex/condition-variable byte channel with backpressure,
monotonic timeouts, FIFO ordering and drain-before-EOF closure.
Shutdown parking uses a native condition variable rather than unsupported poll.

svcGetProcessId supplies native process identity. Session IDs remain unavailable;
foreign-process handles/monitoring and subprocess creation fail explicitly.
minipal_getexepath uses the homebrew loader path shared with module lookup.
No dummy descriptors or successful unsupported operations are supplied.
