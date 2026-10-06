$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$schema = Join-Path $taskRoot 'docs/report.schema.json'
$reports = @(Get-ChildItem -LiteralPath (Join-Path $taskRoot 'artifacts') -Filter 'live-?.json')
$reports += Get-Item -LiteralPath (Join-Path $taskRoot 'artifacts/fixture.json')
foreach($report in $reports) {
 if(-not (Test-Json -Json (Get-Content -Raw -LiteralPath $report.FullName) -SchemaFile $schema)) {throw "Schema validation failed: $report"}
}
$invalid = Get-Content -Raw -LiteralPath $reports[0].FullName | ConvertFrom-Json -AsHashtable
$invalid.version = '0.0.0'
if(Test-Json -Json ($invalid | ConvertTo-Json -Depth 100 -Compress) -SchemaFile $schema -ErrorAction SilentlyContinue) {throw 'Schema negative control unexpectedly accepted'}
"PowerShell Test-Json draft-07 schema validation passed for $($reports.Count) reports; invalid-version negative control rejected"
