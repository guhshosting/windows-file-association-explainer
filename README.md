# windows-file-association-explainer 1.0.0

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


A Windows-only, x64 C++17 command-line utility for explaining local file association registration evidence. MIT licensed. No telemetry or intentional network access. No elevation is required.

```
windows-file-association-explainer .txt
windows-file-association-explainer example.txt --verbs
windows-file-association-explainer .pdf --registrations
windows-file-association-explainer .pdf --json
windows-file-association-explainer --help
windows-file-association-explainer --version
```

The target filename is parsed lexically. It is never opened, tested for existence, or launched. Extension matching is case-insensitive; Unicode inputs and metadata are supported. A dotless name is rejected; this interface accepts extensions and filenames, not bare ProgID identifiers. Options may appear before or after the single query. `--verbs` filters output to shell/verb records. `--registrations` includes all acquired records (including absent keys) and overrides the verb filter. JSON always includes explicit acquisition status, numeric Windows errors, registry types, command analysis, and limitations. Output is UTF-8 when redirected, and Unicode on the Windows console. Untrusted strings are JSON-escaped in both output formats.

## Registration evidence versus effective behavior

A registration is not proof that Explorer, a browser, a server process, or every other application will use that handler. This release deliberately reports effective-default information as **unavailable**. UserChoice ProgId is read as unvalidated evidence; the protected hash is never read, calculated, forged, or changed. The program does not instantiate COM handlers or call shell association helpers that could load registration-selected code.

Reports show separate HKCU and HKLM Classes registrations, the native 64-bit HKCR merged view, Explorer FileExts metadata, referenced ProgIDs, perceived/content types, OpenWith entries, RegisteredApplications capabilities, application registration, CurVer references, default verb strings, verbs, command templates, DelegateExecute and other string metadata at the inspected keys, and DDE registration. Missing merged ProgIDs and absent literal executable paths are flagged. Differing user and machine extension ProgIDs are flagged. Default `shell` values and an `open` subkey are registration facts; the program does not synthesize a guaranteed selected verb.

`%1` and `%L` commonly refer to the target filename, `%*` to additional arguments; `%V`, positional tokens, and `%~` may have context-dependent shell meanings. Tokens are reported lexically, never substituted. See [resolution](docs/resolution.md) for scope, safety boundaries, and authoritative Windows documentation.

## Safety

Registry handles request only query/enumeration access. The application does not write registry values, alter defaults or UserChoice, execute commands, invoke discovered programs, load associated DLLs, or access the target file. Associated executable presence uses only local file attribute queries for literal absolute `.exe` paths on fixed drives. Relative, environment-variable, UNC, device, ambiguous and reparse paths remain unchecked. A present executable is not validated or trusted.

Reports can contain local paths and installed application metadata. Protect captured output as you would registry diagnostic output. The application itself writes only its requested output streams; shell redirection is controlled by the caller.

## Build and validation

Requires CMake 3.24+, Ninja, an x64 MSVC toolchain and Windows SDK. Windows 10/11 and supported Windows Server desktop environments are intended targets. See [validation](docs/validation.md) for actual qualification; this package was qualified only on Server 2025, with Windows 10/11 qualification outstanding.

From an x64 Native Tools command prompt:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake -S . -B build-analyze -G Ninja -DCMAKE_BUILD_TYPE=Release -DWFA_ANALYZE=ON
cmake --build build-analyze
```

MSVC `/W4 /WX /permissive- /sdl`, static runtime, and ASLR/DEP/CFG are enabled. No third-party runtime dependencies are required. Fixtures use an injected registry provider and never create registry registrations. `scripts/build.cmd` configures the installed validation toolchain; `scripts/validate.cmd` additionally runs static checks and independent Python JSON/CLI/live validation. Run `scripts/schema.ps1` for schema validation, `scripts/imports.cmd` followed by `scripts/audit.py` for source/import review, and `scripts/package.ps1` for ZIP creation and extracted-package smoke tests.

Exit codes: **0** completed acquisition within scope, **1** internal/output failure, **2** invalid input/options, **3** partial acquisition. Missing keys alone are not an error. Partial reports retain safely acquired evidence. The registry is not a transactional snapshot and can change while inspected. Deterministic JSON means the same acquired evidence produces the same bytes, not that a changing registry produces identical reports.

## Package

The ZIP contains the executable, this README, MIT license, and documentation including the JSON schema and validation results. Extract to a local directory and run from a console; no installer or registry registration is needed.

