# Resolution and scope

> Release preparation update (2026-10-05): see [current executable qualification and package provenance](../RELEASE-QUALIFICATION.md). Historical validation and pending-test statements in this document describe the original development snapshot. The final ZIP is assembled after qualification and has not been run on Windows.


Each record preserves a registry layer, relative path, acquisition status, numeric error, values (name/type/text), child-key names, and optional command analysis. `user` means HKCU\Software\Classes; `machine` means HKLM\Software\Classes; `merged` means HKCR; `user_raw` and `machine_raw` mean HKCU and HKLM respectively. The native 64-bit view is inspected. No other users' hives, 32-bit view, offline hives, or remote registries are inspected.

Windows defines HKCR as a merged view, with documented per-user and machine merge rules. Reading that view avoids incorrectly treating precedence as a single universal override rule. We preserve each source and let Windows supply its merged view. Differences are evidence. The merged extension's default ProgID is not declared the protected effective default.

Windows supports documented association APIs, including QueryCurrentDefault, but this release intentionally uses no COM activation or shell handler resolution. This trades effective-default reporting for a smaller inspection surface that cannot load registration-selected code. UserChoice's ProgId value is included without its hash. A stale or invalid UserChoice cannot be detected here. No API-reported effective association is claimed.

The extension, all referenced ProgIDs, OpenWith applications, `SystemFileAssociations` for the extension and perceived types, and `*` supplemental file verbs are inspected across the three Classes layers. Extension, file-type and `Applications\executable` records include description, application metadata, icon strings, CurVer, shell default, verb metadata, command and DDE strings. RegisteredApplications capability paths are followed only below the local Software root and matching FileAssociations references add ProgIDs. CurVer targets are recorded once; chains are not recursively resolved. App Paths, COM classes, indirect resource strings, icons, packaged-app manifests, policy XML, context-menu handler implementations and nested/cascading verbs are not resolved. DelegateExecute and DDE are evidence only and can mean a command template is insufficient to determine handling.

The command parser recognizes a quoted first token or an unquoted token ending in `.exe`. It reports literal absolute targets conservatively, without shell parsing, CreateProcess simulation, environment expansion, PATH search, App Paths search, wrapper recursion, resource loading, or placeholder substitution. Quoted paths do not imply a safe command. Unquoted paths with spaces, unmatched quotes, non-executable tokens and relative executables remain unresolved. Placeholder reporting is lexical; an environment variable containing a placeholder-like prefix may also produce a lexical token. Executable presence does not establish file validity, signature, architecture, launchability, or final document handler; a wrapper executable can dispatch elsewhere.

Presence checks reject UNC/device paths, non-fixed drives, alternate streams and unusual path components. Every prefix is checked for reparse points before descending. No executable contents are opened. Registry and filesystem races can invalidate acquired evidence; checks are best effort and do not promise an atomic snapshot or protection against a concurrent adversarial filesystem mutation. There is no intentional network operation.

Acquisition limits: 64 KiB per registry value, 256 values and 512 child names per key, 256 KiB retained value bytes per key, 256 inspected file types, 128 verbs per type/layer, 4096 report records, and 4 Mi UTF-16 code units of aggregate metadata. Limits, access errors, malformed strings and other acquisition failures mark partial reports. Unsupported registry data types are identified but payloads are omitted. Invalid UTF-16 is replaced in UTF-8 conversion; embedded NUL in registry strings marks partial acquisition. Limits may truncate unrelated application registration enumeration, so completed scope does not mean exhaustive Windows configuration coverage.

## Authoritative sources

- [Merged View of HKEY_CLASSES_ROOT](https://learn.microsoft.com/en-us/windows/win32/sysinfo/merged-view-of-hkey-classes-root)
- [File Types](https://learn.microsoft.com/en-us/windows/win32/shell/fa-file-types)
- [Application Registration](https://learn.microsoft.com/en-us/windows/win32/shell/app-registration)
- [IApplicationAssociationRegistration](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iapplicationassociationregistration)
- [QueryCurrentDefault](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iapplicationassociationregistration-querycurrentdefault)

These sources describe Windows mechanisms; they do not imply this utility establishes an effective association.
