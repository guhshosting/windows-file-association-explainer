# Validation record: version 1.0.0, x64

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](../RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


Validated 2026-10-03 on Windows Server 2025 Standard Evaluation, 24H2, build 26100.32230. Toolchain: MSVC 19.51.36260 / tools 14.51.36231; Windows SDK 10.0.26100.0; CMake 4.4.3; LLVM/clang-tidy 23.1.2; Cppcheck 2.22.0. Release uses the static MSVC runtime.

## Results

| Check | Result / scope |
| --- | --- |
| MSVC x64 Release | PASS, C++17, `/W4 /WX /permissive- /sdl`, no Release warning suppression |
| CTest | PASS, 4/4: fixtures, help, version, direct Windows read-only acquisition |
| MSVC `/analyze` | PASS, with the independently reproduced SDK annotation exception below |
| clang-tidy | PASS, all 5 application/test translation units; clang-analyzer, bugprone, performance checks; warnings as errors; easily-swappable-parameters excluded; SDK/STL diagnostics excluded from application findings |
| Cppcheck | PASS, Windows x64/C++17, warning/style/performance/portability, error exit code, Windows library model; unavailable system includes excluded |
| Independent Python JSON/CLI/live checks | PASS, strict UTF-8 JSON parsing, duplicate-key rejection, ordering, repeated identical live output, Unicode fixture, target-presence crosschecks, invalid CLI and output projections |
| Independent draft-07 schema validator | PASS, PowerShell Test-Json validates 6 live reports plus Unicode fixture; invalid-version negative control rejected |
| Source/import safety review | PASS, query/enumeration-only registry access, lexical target parsing, conservative attribute checks, no launch/mutation/network or association-selected loading calls |
| PE inspection | PASS, x64, ASLR, high-entropy ASLR, DEP and CFG; direct imports only ADVAPI32.dll and KERNEL32.dll |
| Extracted-package smoke tests | PASS before delivery, executable hash equality, required package files, help/version, 6 live inputs, 6 invalid CLI cases, projections, UTF-8/Unicode and schema checks |

Release executable SHA-256:

```
c6f750ff2dccca7a4026c13d72b9c059609ba34a3cadaa59e0e722b5ff204a9b
```

The ZIP digest is in the adjacent `.zip.sha256` sidecar. It is not embedded in the ZIP, which would create a circular digest dependency.

## Fixtures and live coverage

Injected, in-memory fixtures cover a simple ProgID, conflicting user/machine defaults, multiple OpenWithProgids/OpenWithList entries, missing referenced ProgID, present/missing executable, multiple verbs, placeholders, quoted paths, malformed quotes, ambiguous unquoted paths, relative and environment-variable targets, UNC targets, inaccessible registry data, deterministic JSON, Unicode extension/metadata, control-character escaping, missing keys, and partial acquisition. Fixtures never write registry data.

Direct Windows tests check invalid/missing registry layers, local read-only registry enumeration, UNC/device/alternate-stream/relative path rejection, local executable attributes, absent executable, and directory rejection.

Live Server 2025 inputs: `.txt` (178 records), `.pdf` (140), `.exe` (123), a nonexistent extension (90), a nonexistent Unicode filename/extension (90), and an unreachable UNC target filename (178). All completed within scope without partial acquisition. Each input was queried twice for byte-identical JSON. Independently captured extension values and UserChoice ProgId before/after remained unchanged. This is a relevant-value crosscheck, not a complete registry snapshot or runtime network trace. The UNC target was only parsed lexically. Associated programs were never launched.

## SDK exception

Windows SDK 10.0.26100.0 `winreg.h` annotates the value-type `ulOptions` parameter of `RegOpenKeyExW` with `_In_opt_`. MSVC emits C6553 for the invalid SAL annotation when analyzing a call. `scripts/sdk-fixture.cpp` reproduces this without application logic; `artifacts/sdk-fixture.txt` records the original warning. Pragmas around the caller do not suppress the declaration diagnostic. Only C6553 is disabled for `src/windows.cpp` in `/analyze` builds, through a source-specific CMake option. All other diagnostics remain errors. Release `/W4 /WX` has no such exception.

## Safety review boundaries

Application source never calls registry mutation APIs, ShellExecute, process creation, dynamic library loading, discovered executables, target-file content APIs, network APIs, or COM activation. UserChoice queries request only ProgId, with case-insensitive key detection; capability paths mentioning UserChoice are rejected. No hashes are read or interpreted. Static CRT imports include standard-stream/file-I/O and runtime library helpers; application code supplies no discovered metadata to these helpers. Console output and stdout/stderr are the application's only intentional writes.

Read-only source and import review, registry snapshots, and safe query tests support the stated design. They do not certify behavior of a compromised OS, concurrently mutated registry/filesystem, arbitrary standard-stream destination chosen by a caller, or every possible Windows configuration.

## Remaining qualification and limitations

Windows 11 and supported Windows 10 editions have **not been tested** here. Before declaring those platforms qualified, repeat Release/CTest, live inspection, schema validation and extracted-package smoke tests on each supported OS, using ordinary users and administrators, populated UserChoice/OpenWith metadata, packaged applications, Unicode console/redirection, and locked-down registry permissions. Other supported Windows Server releases and Server Core environments likewise remain unqualified; this record covers Server 2025 only. Nonadministrator behavior is designed for query access but was not independently qualified under a separate account in this environment.

Effective-default information is deliberately unavailable. This utility reports registration metadata and potential command candidates, not guaranteed application behavior. It does not validate UserChoice hashes, resolve COM/packaged handlers, expand commands, search executable paths, inspect DLL contents, follow reparse points, load icons or resources, inspect other users/32-bit registrations, or produce an atomic snapshot. See [resolution](resolution.md) for the acquisition scope and limits.

Validation evidence in the source workspace: `artifacts/release-build.txt`, `artifacts/static-validation.txt`, `artifacts/sdk-fixture.txt`, `artifacts/imports.txt`, `artifacts/headers.txt`, `artifacts/safety-review.txt`, `artifacts/schema-validation.txt`, `artifacts/live-0.json` through `live-5.json`, `artifacts/fixture.json`, and `artifacts/package-smoke.txt`.
