# Pause and inspect busy threads

Use the devkitA64/libnx prerequisites in the
[virtual-memory recipe](../virtual-memory/README.md). From the repository root:

```sh
make -C tests/thread-pause -j4
```

Run tests/thread-pause/dotnet-thread-pause-probe.nro in full application-memory
homebrew mode. It creates sdmc:/switch/dotnet-runtime-tests if needed,
overwrites thread-pause.txt with a BEGIN record, closes it and reopens it for
append. Preserve an existing log first. Keep user inputs outside generated
build/output locations; rebuilding replaces files and make clean removes
the generated build directory and NRO/NACP/ELF outputs.

Two workers use cores 0 and 1 with priority 0x30. Each assembly loop makes no
calls and increments a counter. For up to 128 rounds per worker, the coordinator
pauses it and compares two svcGetThreadContext3 snapshots separated by a
1 ms wait. It checks counter stability, a PC inside the loop and the expected
x0, x19, q0 and TPIDR_EL0 values. After resumption, the counter must advance
within a bounded wait. These intervals and counts are workload parameters,
not captured measurements.

The coordinator releases each acquired pause before evaluating the checks;
failure to resume aborts. It signals workers to stop, joins and closes them,
then records pass/expected counts and a failure flag. Inspect those records,
not only normal return from main. Assembly restores each worker's original
thread pointer before return.

## Runtime boundaries

Stopping a call-free native loop and reading registers is not a complete GC
suspension mechanism. A runtime must recognize safe points, account for
prologs/epilogs and native critical sections, retain ownership of every pause
and coordinate all-thread memory ordering.

Context inspection is not a general register-context writeback interface.
Any GC design using snapshots must separately address moving roots and the
lifetime of saved register/stack locations. No precise GC root-pinning or
managed rendezvous implementation is supplied by this workload.
Do not infer all-thread ordering solely from a local fence or TLB invalidation.

References:
[KThread](https://github.com/Atmosphere-NX/Atmosphere/blob/1.11.2/libraries/libmesosphere/source/kern_k_thread.cpp),
[context reader](https://github.com/Atmosphere-NX/Atmosphere/blob/1.11.2/libraries/libmesosphere/source/arch/arm64/kern_k_thread_context.cpp),
[scheduler lock](https://github.com/Atmosphere-NX/Atmosphere/blob/1.11.2/libraries/libmesosphere/include/mesosphere/kern_k_scheduler_lock.hpp),
[libnx declarations](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h).
No kernel source is copied into this probe.
