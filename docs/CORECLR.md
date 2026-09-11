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

## Shared GC operating-system adapter

CoreCLR and NativeAOT share the Horizon GC OS adapter while retaining the
ordinary GC algorithms and event implementation. Advisory reset validates that
the complete range is owned and committed. Affinity reconfiguration recomputes
from the original kernel mask instead of progressively narrowing it; the
maximum CPU index bound comes from that same native mask.

The diagnostic compile selection uses the existing TCP transport with its
default listen port disabled on Horizon; this does not establish debugger
transport support. GNU sincos declarations are scoped to the math target
rather than changing newlib feature visibility globally.

## Embedded host and resident data

Static CoreCLR/RyuJIT linkage requires the GC map encoder, compression runtime,
ICU and the matching native archives. Build the matching ARM64 CoreLib via
clr.corelib with PublicSign=true. Use the runtime source-build guide above
for the complete source prerequisites and commands.

PAL_ProbeMemory walks native mappings and permissions without writing caller
memory or using a Unix pipe. Named shared objects reject unsupported operations
before creating files; unnamed PAL mutexes retain their normal behavior.
The debugger FIFO transport rejects unsupported create/connect requests, and
the embedded host uses EnableDiagnostics_Debugger=0.

Declared resident NRO read-only data uses SetProcessMemoryPermission for its
first write transition, then ordinary data permissions. That transition removes
execute capability irreversibly. Text is excluded from writable transitions.

## Shared virtual arena

The nxvm allocator reserves a shared virtual arena before native workers can
fragment the stack region. A libnx reservation keeps other native stacks out;
sorted first-fit allocation reuses aligned holes with guard gaps. Physical
commitment remains independently bounded by the backing pool.

The arena targets twice the backing budget, with a 64 MiB minimum and an upper
bound of half the Horizon stack region. Reservation retries halve the requested
size; initialization fails if none can be reserved. GC queries actual arena
capacity and maximum address instead of advertising the whole stack region.

## Device-qualified file paths

The PAL recognizes device-qualified roots and preserves their prefixes while
using lexical dot/parent normalization. Directory creation uses the same root
test. File workloads should use a separately closed input rather than reopen
their active writable output log.

## Native file descriptor ownership

The PAL uses libsysbase's reference-counted dup for file-mapping and
standard-stream ownership, preserving the shared cursor. It normalizes missing
exhaustion errno and does not request nonexistent close-on-exec state.
Explicit inherited handles remain unsupported.

The [libnx fcntl correction](https://github.com/pixelomer/libnx/commit/93ca59adeaf4d0d86a456b5266dd8eb5b024fe55)
returns -1 and sets errno to EOPNOTSUPP for unsupported operations; a positive
error value must not be treated as a duplicate descriptor. Use the staged
source-built libnx overlay rather than modifying a system SDK in place.
