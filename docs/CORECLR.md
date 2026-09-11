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
