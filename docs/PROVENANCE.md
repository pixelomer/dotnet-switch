# Source provenance

Use established public homebrew APIs and source with traceable origin and
licenses. Do not incorporate proprietary Nintendo SDK/tools, decompilations
of them or repackaged derivatives. A public GitHub URL or license label alone
does not establish origin.

## Platform sources

- dotnet/runtime v10.0.12, commit
  4271d88e0aebf3d04f188f1334c2220d80555ef6, with its MIT license and retained
  third-party notices. Runtime platform changes belong to that source history.
- switchbrew/libnx v4.12.0, commit
  7644c9b26099aa2d2145bc72a21ee24190e92085, under its ISC license.
  Its public JIT, syscall and thread-context APIs are the probe interfaces.
- devkitPro/devkitA64 and the installed libnx application Makefile template.
- Original probe/adapter source in this repository.

Preserve upstream notices when adapting code. For additional dependencies,
identify the original source, exact revision and relevant file licenses.

## Behavioral references

Atmosphere-NX/Atmosphere 1.11.2, commit
5388824be146a89619e8d641acd64599cf1c5f62, provides public kernel behavior
references, including libraries/libmesosphere/source/svc/kern_svc_code_memory.cpp.
Libmesosphere is GPLv2; reading it does not permit relicensing its implementation
as MIT. No kernel implementation is copied into this repository's probes.

References:

- https://github.com/dotnet/runtime/tree/v10.0.12
- https://github.com/switchbrew/libnx/tree/v4.12.0
- https://github.com/Atmosphere-NX/Atmosphere/tree/1.11.2
