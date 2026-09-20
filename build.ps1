param([switch]$Test, [switch]$Run)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$buildRoot = $PSScriptRoot
# MinGW make needs an ASCII path on Windows. The files stay in the same folder.
$fso = New-Object -ComObject Scripting.FileSystemObject
$buildRoot = $fso.GetFolder($PSScriptRoot).ShortPath
$localCmake = Get-ChildItem -LiteralPath (Join-Path $buildRoot 'tools') -Directory -Filter 'cmake-*-windows-x86_64' -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
$compilerFolder = Join-Path $buildRoot 'tools\mingw64\bin'
if ($localCmake) { $cmakeExe = Join-Path ($fso.GetFolder($localCmake.FullName).ShortPath) 'bin\cmake.exe' }
else { $cmakeExe = (Get-Command cmake -ErrorAction Stop).Source }
if (Test-Path -LiteralPath $compilerFolder) {
    $env:PATH = "$compilerFolder;$env:PATH"
    $env:GCC_EXEC_PREFIX = ($buildRoot.Replace('\','/') + '/tools/mingw64/lib/gcc/')
    & $cmakeExe -S $buildRoot -B (Join-Path $buildRoot 'build') -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
} else {
    & $cmakeExe -S $buildRoot -B (Join-Path $buildRoot 'build')
}
if ($LASTEXITCODE -ne 0) { throw 'Konfiguracja CMake nie powiodla sie.' }
& $cmakeExe --build (Join-Path $buildRoot 'build') --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Budowanie nie powiodlo sie.' }
if ($Test) {
    $ctestExe = Join-Path (Split-Path $cmakeExe) 'ctest.exe'
    & $ctestExe --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Testy nie przeszly.' }
}
if ($Run) {
    $gameExe = Join-Path $PSScriptRoot 'build\OrbitalOdds.exe'
    if (-not (Test-Path -LiteralPath $gameExe)) { $gameExe = Join-Path $PSScriptRoot 'build\Release\OrbitalOdds.exe' }
    Start-Process -FilePath $gameExe -WorkingDirectory $PSScriptRoot
}
