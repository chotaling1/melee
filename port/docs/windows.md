# Running the port on Windows

The port cross-compiles for Windows from the Linux build host:

    PORT_TARGET=x86-windows-gnu .venv/bin/python port/configure.py
    .venv/bin/ninja -C port -f build-x86-windows-gnu.ninja

Output: `port/build-x86-windows-gnu/melee.exe` (32-bit console program,
statically linked, no DLLs besides Windows system ones). `port/tools/check.sh`
builds it on every change, but the build host can't run it.

## Requirements

- 64-bit Windows 10 or 11. The game's main memory is mapped at 0x80000000,
  so the 32-bit executable is marked large-address-aware
  (`port/tools/pe_laa.py`), which only gives it a 4 GB address space on
  64-bit Windows.
- Your own Melee NTSC 1.02 disc image (`.iso`).

## Run the headless test match

From WSL the build is reachable in Explorer at
`\\wsl.localhost\<distro>\home\openclaw\.openclaw\workspaces\melee-port\port\build-x86-windows-gnu\melee.exe`;
copy it anywhere. In PowerShell:

```powershell
$env:MELEE_ISO = "C:\Games\Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso"
$env:MELEE_PORT_DEMO_MATCH = "1"
$env:MELEE_PORT_STAGE = "line"
$env:MELEE_PORT_INPUT = "60:A,120:A"
$env:MELEE_PORT_FRAMES = "9000"
$env:MELEE_PORT_TRACE = "60"
.\melee.exe 2> run.log
```

Expected: it finishes in a few seconds and `run.log` ends with
`reached MELEE_PORT_FRAMES=9000, exiting`. The fighter lines
(`f60 p0 kind 1 motion ...`) should be identical to the Linux run's trace in
`port/tests/demo_line.trace`, which is a quick determinism check across
operating systems:

```powershell
Select-String -Path run.log -Pattern '^\[port\] (f\d+ p\d|p\d kind \d+ attrs)' |
  ForEach-Object { $_.Line -replace '^\[port\] ', '' } > trace.txt
```

then compare `trace.txt` with the repo's `port/tests/demo_line.trace`.

Verified 2026-09-16 on Windows 11 Home (PORT-012): exit 0 at 9000 retraces
in about 1 s, and the log is byte-identical to the Linux run, including a
per-frame trace (`MELEE_PORT_TRACE=1`, 5502 fighter lines). When the host
is the machine running WSL, the build and the ISO can be used in place
through `\\wsl.localhost\<distro>\...` (set `MELEE_ISO` to that path).

## If it crashes

The Windows crash handler prints `fatal exception`, the faulting address, EIP
and a backtrace plus the image base. Send the log; addresses can be mapped
back with the Linux build's symbols only roughly, so a Windows symbolizer is
a follow-up if needed.
