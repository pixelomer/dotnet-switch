# Heap-backed virtual mapping capability probe

Install devkitPro's devkitA64, switch-tools and libnx through the
[devkitPro toolchain instructions](https://devkitpro.org/wiki/Getting_Started).
Use libnx 4.10.0 or newer and set DEVKITPRO to its installation prefix.
The Makefile includes that installation's libnx/switch_rules and links libnx.

From this repository's root:

```sh
make -C tests/virtual-memory -j4
```

Build products are placed in tests/virtual-memory and its build subdirectory.
Rebuilding replaces generated files; make clean removes the generated build
directory and NRO/NACP/ELF outputs. Keep user inputs outside those locations.

Run tests/virtual-memory/dotnet-vm-probe.nro in full application-memory
homebrew mode. It creates sdmc:/switch/dotnet-runtime-tests if needed and
overwrites virtual-memory.txt there. Preserve an existing log before running.
The workload uses only memory owned or reserved by its process and exits to HOME.

The physical-mapping path requires both syscall availability hints and process
system-resource allocation; otherwise it records a skip. When attempted, it
selects and reserves an unmapped alias-region slice, commits 2 MiB, checks
half-range decommit/recommit and retained bytes, and releases the mapping.
It is bounded to 128 cycles.

The separate heap-alias workload maps a 2 MiB aligned backing allocation into
a reserved 64 MiB stack-region range. It writes a pattern, unmaps half, clears
returned backing, remaps it, checks both halves and finally unmaps and frees
backing. It also allows up to 128 cycles. Cleanup failure retains mapped backing
rather than freeing memory still owned by the kernel.

Read the skip/failure records and cycle counts, not only the final process
marker. The workload is single-threaded, with fixed allocation/reservation sizes,
no GC, no injected failures and no production allocator. Explicit memset supplies
zero-filled heap-alias recommit; it is not an OS zeroing guarantee or a general
absence-of-leaks claim.
