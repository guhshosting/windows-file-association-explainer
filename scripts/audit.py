import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[1]
source = '\n'.join(p.read_text(encoding='utf-8-sig') for p in (root / 'src').glob('*.cpp'))
forbidden = r'\b(?:Reg(?:Set\w*|Create\w*|Delete\w*|Save\w*|Load\w*|Restore\w*|Replace\w*|UnLoad\w*)|ShellExecute\w*|CreateProcess\w*|WinExec|system|_wsystem|LoadLibrary\w*|GetProcAddress|CoCreateInstance|WinHttp\w*|Internet\w*|WSAStartup|socket|connect|URLDownload\w*|CreateFile\w*|ReadFile|fopen|_wfopen)\s*\('
assert not re.search(forbidden, source), 'Forbidden source API call'
assert 'KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_64KEY' in source
assert not re.search(r'\b(?:KEY_WRITE|KEY_ALL_ACCESS|KEY_SET_VALUE|KEY_CREATE_SUB_KEY)\b', source)
assert 'RegQueryValueExW(h.key, L"ProgId"' in source
assert 'GetFileAttributesW(prefix.c_str())' in source
imports = (root / 'artifacts/imports.txt').read_text(encoding='utf-8-sig')
dlls = sorted(set(re.findall(r'\b[\w.-]+\.dll\b', imports, re.IGNORECASE)))
assert {d.lower() for d in dlls} <= {'kernel32.dll', 'advapi32.dll'}, dlls
assert not re.search(r'\b(?:ShellExecute\w*|CreateProcess\w*|WinExec|RegSet\w*|RegCreate\w*|RegDelete\w*|WinHttp\w*|Internet\w*|WSAStartup|socket|connect)\b', imports)
headers = (root / 'artifacts/headers.txt').read_text(encoding='utf-8-sig')
for expected in ['machine (x64)', 'Dynamic base', 'NX compatible', 'High Entropy Virtual Addresses', 'Guard CF']:
    assert expected.lower() in headers.lower(), expected
print('Source/import safety checks passed')
print('Direct imports:', ', '.join(dlls))
print('Registry access is query/enumerate only; target is lexical input; executable attributes only')
print('No shell launch, process creation, registry mutation, network library, or association-selected loading API in application source')
print('Static MSVC CRT may import file I/O and library APIs for runtime/standard-stream support; application source never supplies discovered metadata to them')
print('x64 ASLR/DEP/high-entropy ASLR/CFG verified in PE headers')
