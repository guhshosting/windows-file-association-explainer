$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $taskRoot
$dist = Join-Path $taskRoot 'dist'
$stage = Join-Path $taskRoot 'artifacts/package-stage'
$extract = Join-Path $taskRoot 'artifacts/extracted-smoke'
New-Item -ItemType Directory -Path $dist,$stage,$extract -Force | Out-Null
Copy-Item -LiteralPath 'build/windows-file-association-explainer.exe','README.md','LICENSE' -Destination $stage -Force
Copy-Item -LiteralPath 'docs' -Destination $stage -Recurse -Force
$zip = Join-Path $dist 'windows-file-association-explainer-1.0.0-windows-x64.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal -Force
Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force
$exe = Join-Path $extract 'windows-file-association-explainer.exe'
if((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath 'build/windows-file-association-explainer.exe' -Algorithm SHA256).Hash){throw 'Extracted executable mismatch'}
foreach($name in @('README.md','LICENSE','docs/report.schema.json','docs/resolution.md','docs/validation.md')) {
 if(-not (Test-Path -LiteralPath (Join-Path $extract $name) -PathType Leaf)){throw "Missing package file: $name"}
}
& 'python' 'scripts/verify.py' $exe
if($LASTEXITCODE){throw 'Extracted package smoke failed'}
& './scripts/schema.ps1'
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$hash + '  ' + (Split-Path -Leaf $zip) | Set-Content -Encoding ascii -LiteralPath ($zip+'.sha256')
"ZIP: $zip"
"SHA-256: $hash"
"Extracted-package smoke and schema checks passed"
$global:LASTEXITCODE=0
