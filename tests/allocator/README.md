# Bounded virtual-memory allocator

[include/nxvm.h](../../include/nxvm.h) exposes reserve, commit, decommit and
complete release for data-only memory. Use the devkitPro/devkitA64/libnx
prerequisites in the [capability probe](../virtual-memory/README.md).
From the repository root:

```sh
make -C tests/allocator -j4
```

Run tests/allocator/dotnet-allocator-probe.nro in full application-memory
homebrew mode. It creates sdmc:/switch/dotnet-runtime-tests if needed and
overwrites allocator.txt there. Preserve an existing log first.
Build outputs are generated in the probe directory and its build subdirectory;
make clean removes the generated build directory and NRO/NACP/ELF files.
Keep user inputs elsewhere.

## Ownership contract

The allocator owns a page-aligned backing pool, independent reservation
metadata and a libnx mutex. Commitment consumes free backing pages and maps
them into reserved virtual addresses. Decommit unmaps and returns those pages
to the pool, not to the operating system. Consecutive mappings are coalesced
only when both source and destination are contiguous. Fresh backing pages
are cleared before mapping; repeated commit and decommit are idempotent.

Exhaustion fails commitment without changing live mappings. A commitment
SVC failure rolls back only newly mapped pages. Failed unmap poisons the
allocator and retains ownership; callers must stop using it. Mapped backing
must not be returned to malloc.

This API rejects partial reservation release and offers no executable
permissions, shared mappings or fixed-address replacement. Callers must
coordinate accesses with decommit/release; metadata locking does not protect
a live user pointer from concurrent retirement.

## Native workload

The workload uses a 4 MiB pool, checks exhaustion and invalid/released ranges,
reuses 512 single-page holes for a contiguous virtual mapping and performs
128 full-pool cycles with partial decommit, zeroing and retained-data checks.
Four workers each perform 128 allocation lifetimes with independent data
checks. Completion requires zero live commitments/reservations followed by
pool destruction and reinitialization. These are source workload dimensions,
not recorded results.

Read the ALLOCATOR_END success field and preceding failure records; process
exit alone does not establish success. This is a native allocator workload,
not a managed GC or general runtime compatibility test.

## Host failure injection

On a POSIX host with Bash, a C11 compiler, pthreads and AddressSanitizer/
UndefinedBehaviorSanitizer support, run from the repository root:

```sh
bash tests/allocator/host/run.sh
```

CC can select the host compiler; the default is cc. The script compiles this
checkout's source/nxvm.c with a local libnx test double into
tests/allocator/host/build/failure-test, replacing any existing generated
executable there, then runs it. Do not use the test-double header in a target
build.

The simulated mapping layer uses copies and inaccessible source pages to
exercise 1,000 commit rollbacks after a second mapping fails. It checks old
data preservation, exact accounting and exhaustion/reuse after rollback.
A child process injects failed unmap and checks retained ownership and
fail-closed operations; process teardown reclaims the deliberately poisoned
instance. This simulation tests allocator ownership, not Horizon alias behavior.
