import json
import pathlib
import subprocess
import sys
import winreg

ROOT = pathlib.Path(__file__).resolve().parents[1]
ART = ROOT / 'artifacts'
ART.mkdir(exist_ok=True)


def decode(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise AssertionError('Duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(raw.decode('utf-8'), object_pairs_hook=unique,
                      parse_constant=lambda x: (_ for _ in ()).throw(AssertionError(x)))


def run(exe, args, expected=(0,)):
    result = subprocess.run([str(exe), *args], capture_output=True, timeout=30)
    assert result.returncode in expected, (args, result.returncode, result.stderr)
    assert not result.stderr, (args, result.stderr)
    return result.stdout


def validate(data):
    assert data['version'] == '1.0.0' and data['schema_version'] == 1
    assert data['effective_default']['status'] == 'unavailable'
    pairs = [(r['layer'], r['path']) for r in data['records']]
    assert len(pairs) == len(set(pairs)), 'Duplicate records'
    for record in data['records']:
        assert record['values'] == sorted(record['values'], key=lambda v: v['name'])
        assert record['subkeys'] == sorted(record['subkeys'])
        if record['path'].endswith('\\UserChoice'):
            assert all(v['name'] == 'ProgId' for v in record['values'])
        for command in record['commands']:
            if command['presence'] == 'present':
                assert command['parse_status'] == 'literal_absolute_executable'
                assert pathlib.Path(command['target']).is_file(), 'Independent local presence crosscheck'


def snapshot(extension):
    # Independent read-only registry snapshot; UserChoice hash is excluded.
    result = []
    for root, path in [(winreg.HKEY_CURRENT_USER, 'Software\\Classes\\' + extension),
                       (winreg.HKEY_LOCAL_MACHINE, 'Software\\Classes\\' + extension),
                       (winreg.HKEY_CURRENT_USER, 'Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\' + extension + '\\UserChoice')]:
        try:
            with winreg.OpenKey(root, path, 0, winreg.KEY_QUERY_VALUE | winreg.KEY_WOW64_64KEY) as key:
                values = []
                names = ['ProgId'] if path.endswith('UserChoice') else ['', 'Content Type', 'PerceivedType']
                for name in names:
                    try:
                        values.append((name, *winreg.QueryValueEx(key, name)))
                    except FileNotFoundError:
                        pass
                result.append((path, values))
        except FileNotFoundError:
            result.append((path, None))
    return result


def main():
    exe = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/windows-file-association-explainer.exe'
    assert b'1.0.0' in run(exe, ['--version'])
    assert b'Usage:' in run(exe, ['--help'])
    for args in [[], ['--bogus'], ['.txt', '.pdf'], ['bare'], ['.'], ['.txt', '--version']]:
        result = subprocess.run([str(exe), *args], capture_output=True, timeout=30)
        assert result.returncode == 2 and not result.stdout and result.stderr, args
    summary = []
    for index, query in enumerate(['.txt', '.pdf', '.exe', '.wfa_nonexistent_8ab29', 'C:\\does-not-exist\\résumé.文書', '\\\\unreachable.invalid\\share\\document.txt']):
        extension = pathlib.PureWindowsPath(query).suffix if not query.startswith('.') else query
        before = snapshot(extension)
        raw = run(exe, [query, '--json'], (0, 3))
        data = decode(raw)
        validate(data)
        assert before == snapshot(extension), 'Relevant registration metadata changed'
        assert data['extension'] == extension.lower()
        again = run(exe, [query, '--json'], (0, 3))
        assert raw == again, 'Live JSON changed between two immediate acquisitions'
        (ART / f'live-{index}.json').write_bytes(raw)
        summary.append({'query': query, 'partial': data['partial'], 'records': len(data['records'])})
    verbs = decode(run(exe, ['.txt', '--verbs', '--json'], (0, 3)))
    assert all('\\shell' in r['path'] for r in verbs['records'])
    full = decode(run(exe, ['.txt', '--verbs', '--registrations', '--json'], (0, 3)))
    assert any(r['path'] == '.txt' for r in full['records'])
    text = run(exe, ['example.txt'], (0, 3)).decode('utf-8')
    assert 'Effective default: unavailable' in text
    fixture = ROOT / 'build/wfa_tests.exe'
    raw = run(fixture, ['--json-fixture'])
    data = decode(raw)
    validate(data)
    assert data['extension'] == '.文書'
    (ART / 'fixture.json').write_bytes(raw)
    (ART / 'live-summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print('Independent Python JSON/CLI/registry checks passed; 6 live queries, 6 CLI errors, projections and Unicode fixture')


if __name__ == '__main__':
    main()
