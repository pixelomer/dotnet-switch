# Recoverable AArch64 fault probe

Use the devkitA64/libnx prerequisites in the
[virtual-memory recipe](../virtual-memory/README.md). From the repository root:

```sh
make -C tests/exception-return -j4
```

Run tests/exception-return/dotnet-exception-probe.nro in full application-memory
homebrew mode. It creates sdmc:/switch/dotnet-runtime-tests if needed and
overwrites exception-return.txt there. Preserve any existing log first.
Keep user inputs outside the generated build and NRO/NACP/ELF/map locations;
rebuilding replaces outputs and make clean removes generated build products.

Four workers each attempt 1,024 deliberate null-load round trips. Two use zero
TPIDR_EL0; two temporarily use a nonzero pointer to their stack observation.
Assembly restores the original pointer before returning to C.

The handler accepts only the exact probe instruction and a zero fault address.
It changes the kernel exception frame's PC to the known continuation and x0
to 0x42, then calls svcReturnFromException(0). Unknown faults remain fatal.
The handler uses kernel-saved scratch registers without C/library calls,
counts through an atomic exclusive loop and stores per-thread observations
through the interrupted x20 pointer.

Checks cover x0, x1, x9, x19, q0, NZCV and TPIDR_EL0, not all registers.
Read individual worker pass counts and the final caught/expected counts;
normal return from main alone does not establish complete coverage.

This is a restricted native exception primitive, not C# NullReferenceException
support. A runtime bridge still needs complete context storage, unwind/redirection
integration, stack-overflow policy and GC coordination. It must not reuse a
process-global exception dump as surviving per-thread state after kernel return.

References: [libnx exception entry](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/exception.s),
[libnx context definitions](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/arm/thread_context.h),
[Horizon exception handling](https://switchbrew.org/wiki/SVC#Exception_handling).
