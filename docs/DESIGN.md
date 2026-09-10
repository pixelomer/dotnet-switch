# Runtime constraints

## Memory

The .NET Unix GC interface distinguishes virtual reservation from commitment.
Alignment trimming, partial release, commitment, decommitment and zero-filled
recommit must preserve their respective ownership contracts. A backing
allocation whose release is ignored cannot supply those semantics.

The public runtime baseline
[d75fa786b88e10f45ca9263773d84e47033cfb01](https://github.com/exelix11/dotnet_runtime/blob/d75fa786b88e10f45ca9263773d84e47033cfb01/src/coreclr/gc/unix/gcenv.unix.cpp)
provides the Unix VirtualReserveInner/VirtualDecommit interface reference.
Its Unix implementation is not a Horizon ABI adapter.

A MapPhysicalMemory availability hint alone does not establish that a process
has the required resource allocation. The probe checks both before attempting
that path. See [the SVC prerequisites](https://switchbrew.org/wiki/SVC#MapPhysicalMemory).

Heap-backed aliases instead reserve a virtual range with libnx, map owned heap
pages with MapMemory and unmap the exact corresponding backing address.
Source pages are inaccessible while mapped; allocator metadata must remain
outside them. Backing ownership must survive every outstanding alias.
Decommit can recycle pages within a bounded backing pool even though the
loader's heap remains physically allocated; that is not returning RAM to the OS.

The capability probe is not a production reserve/commit/decommit/release
allocator or a general POSIX mmap implementation. A compatibility layer would
also need partial unmap and explicit ownership for fixed-address replacement.

## ABI and execution

A runtime and its native support libraries must agree on structure layouts,
errno values, pthread objects, stat/socket types, TLS, signals and calling
conventions. Supplying missing symbols does not make Linux native archives
compatible with libnx/newlib. Use source-built platform support.

GC safepoints, suspension and thread teardown need their own ownership and
ordering contracts. A void API cannot report an unsupported operation through
an invented failure return. Native process exit is not managed exception
propagation, successful shutdown or evidence that a managed workload completed.
