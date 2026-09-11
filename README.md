# .NET on Horizon: runtime foundations

Native runtime-boundary probes for Nintendo Switch homebrew using devkitA64
and libnx. These source workloads explore memory ownership and platform API
contracts; they are not a complete managed runtime or NativeAOT SDK.
No game code, proprietary binaries or console credentials are included.

- [Virtual-memory capability probe](tests/virtual-memory/README.md)
- [Bounded data allocator](tests/allocator/README.md)
- [Runtime design constraints](docs/DESIGN.md)

The probe Makefile follows devkitPro's libnx application template.

- [Recoverable fault primitive](tests/exception-return/README.md)

- [CoreCLR integration boundaries](docs/CORECLR.md)
