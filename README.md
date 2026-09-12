# .NET for Nintendo Switch homebrew

Build and integrate the Horizon ports of CoreCLR/RyuJIT, NativeAOT and Mono.
This repository supplies pinned source-build orchestration, reusable native
platform primitives and focused source workloads. The
[runtime repository](https://github.com/pixelomer/dotnet-runtime) supplies the
platform implementation; [mono-nx](https://github.com/pixelomer/mono-nx)
supplies the Mono launcher and AOT examples.

## Build a runtime

Use Linux x86-64, Python 3.12+, Git, Bash, GCC/G++, Clang/LLVM, CMake, Ninja,
Make, patch, pkg-config and devkitPro's devkitA64, switch-tools, libnx and Switch
portlibs. Set DEVKITPRO to the toolchain installation. The runtime downloads
the Microsoft SDK pinned by its source; Git, ICU and NuGet access is required
on the first build. See the runtime's
[source-build guide](https://github.com/pixelomer/dotnet-runtime/blob/main/eng/libnx/README.md)
for toolchain and native prerequisite details.

From a checkout of this repository:

```sh
python3 build.py --profile net10-coreclr --jobs 8
```

The script fetches the exact runtime commit from runtime.lock.json, builds its
pinned libnx and ICU sources in a separate SDK overlay, then builds the runtime
and matching managed libraries. No sibling checkout or prebuilt runtime is
required. The system devkitPro installation stays unchanged.

| Profile | Use |
| --- | --- |
| net10-coreclr | JIT compilation and dynamic IL loading |
| net10-nativeaot | Ahead-of-time compilation for fixed .NET 10 applications |
| net9-nativeaot | .NET 9.0.3 NativeAOT applications |
| net9-mono-llvm | Mono interpreter/AOT runtime and LLVM cross compiler |

Each profile uses artifacts/sources/PROFILE for its source/build tree.
artifacts/PROFILE/build.json records the source identity, runtime location and
SDK/ICU environment produced by that build. --fetch-only stops after the pinned
source checkout. --source-mirrors FILE optionally maps canonical Git URLs to
source mirrors; it does not select prebuilt runtime directories. Keep any mirror
mapping outside the repository as uncommitted local configuration.

The fetcher rejects dirty dependency checkouts and mismatched existing revisions.
Use a fresh, dedicated checkout when changing pins. Rebuilding updates generated
outputs and SDK overlays; ICU identity changes can remove and recreate its
generated source/build directories. Keep user inputs outside artifacts and
preserve outputs before rebuilding if needed.

Read eng/libnx/README.md in the fetched profile source for its output layouts
and embedding contract. Keep CoreLib, framework DLLs, compiler and native
archives from the same build. CoreCLR requires matching Horizon IL-only
assemblies, not foreign ReadyToRun images or arbitrary native modules.
NativeAOT requires a matching ILC and --noinlinetls; it cannot compile newly
installed managed code at runtime.

## Build an embedding example

The CoreCLR host's metadata checkers require dnfile, pyelftools and pefile
in the Python environment used to invoke the build. From this repository root:

```sh
python3 -m venv artifacts/host-python
. artifacts/host-python/bin/activate
python3 -m pip install dnfile pyelftools pefile
python3 build.py --profile net10-coreclr --example bcl
```

This builds artifacts/net10-coreclr/example/coreclr-host-probe.nro, its ELF/map
and a matching managed/ directory. --example basic selects the smaller
arithmetic, allocation and exception workload. These commands do not deploy.

The host's source and instructions are in
src/coreclr/pal/tests/libnx/host/ in the fetched runtime. Read that guide before
running a workload: it describes log replacement, temporary input ownership,
timeouts and expected exit values. Rebuilding replaces the example's generated
managed and source-snapshot directories. Keep user inputs outside them.

Copy the generated managed directory's contents to
sdmc:/switch/coreclr-probe/, preserving existing files first, then run the NRO
in full application mode. Deploy a coherent payload whenever managed inputs or
probe selection change; leave unrelated DLLs outside this probe directory.

## NativeAOT SDK packaging

After the selected profile build, run its
src/coreclr/nativeaot/Runtime/libnx/package-sdk.py OUTPUT and validate-sdk.py OUTPUT.
Use the same profile source for both helpers and a dedicated generated output
directory. The package retains runtime licenses and records its inputs; ICU and
the devkitPro toolchain remain external prerequisites with source recipes in
the runtime guide.

The .NET 10 package also requires the source-built zlib/Brotli archives when
linking consumers. Its socket implementation is a separate source-built
libs.sfx System.Net.Sockets.dll input; the inherited .NET 9 socket recipe is not
a .NET 10 substitute. Follow the generated package README and the profile's
managed-library guide for these inputs.

## Reusable native components

The include/ and source/ directories contain allocator primitives.
Focused tests under tests/ cover allocation, thread suspension, executable
mappings and exception return. Their guides describe source workloads and
prerequisites, not a guarantee of complete managed runtime compatibility.

See [design constraints](docs/DESIGN.md),
[CoreCLR integration boundaries](docs/CORECLR.md) and
[source provenance](docs/PROVENANCE.md).
Use full application memory and retain runtime-owned resources until process
exit. No Nintendo SDK, game files, FMOD SDK, console keys or private build
tooling are required.
