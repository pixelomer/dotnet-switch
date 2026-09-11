# Horizon executable-memory primitive

This native workload uses libnx 4.12's public JIT and pthread APIs.
Install devkitPro/devkitA64, switch-tools and libnx using the
[capability probe prerequisites](../virtual-memory/README.md), then build
from the repository root:

```sh
make -C tests/jit-memory -j4
```

The Makefile follows the libnx application template. Rebuilding replaces
generated files; make clean removes its build directory and NRO/NACP/ELF
outputs. Keep user inputs outside those locations.

Run tests/jit-memory/dotnet-jit-memory-probe.nro in an application-memory
homebrew context. It overwrites sdmc:/switch/dotnet-jit-memory-probe.txt
and requests exit to HOME. Preserve any existing log before running.

The workload creates and retires 256 code objects across sixteen rounds.
For each object it emits constants, checks RX permissions and, for the
CodeMemory backend, distinct RW/RX addresses and writable permissions.
Each of three emitted functions runs on four joined workers with 4,096 calls
per worker. The final ADR function checks that execution uses the RX address.

Every executable transition publishes through libnx. Callers are joined before
code is rewritten or retired; this does not exercise patching while execution
is active. Failed checks abort. Inspect failure records and the final summary,
not only the existence of an NRO or a process exit.

This is not a managed CoreCLR/Mono JIT workload and does not establish allocator
compatibility, managed exceptions, GC safety or assembly loading. A successful
CodeMemory selection does not verify the process-code-memory fallback.
See [source provenance](../../docs/PROVENANCE.md) for licenses and source inputs.
