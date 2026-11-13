param([string]$OutputDirectory = 'x64/SceneSettingsBenchmark', [ValidateRange(1,10)][int]$ProcessRuns = 3)
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = [IO.Path]::GetFullPath((Join-Path $repository $OutputDirectory))
New-Item -ItemType Directory -Force $output | Out-Null
$header = Join-Path $repository 'x64/JsonParserFairBenchmark/deps/json.hpp'
if (!(Test-Path -LiteralPath $header)) { throw 'Run the existing fair JSON benchmark first to prepare the pinned nlohmann header.' }
if ((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash -ne 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63') { throw 'Unexpected nlohmann/json header hash.' }
Copy-Item -LiteralPath $header -Destination $output
# Build current production sources rather than silently benchmarking an old DLL.
$project = Join-Path $repository 'External/REDoxWrapper/REDoxWrapper.csproj'
& dotnet publish $project -c Release -r win-x64 -o (Join-Path $output 'redox') --ignore-failed-sources -p:NuGetAudit=false
if ($LASTEXITCODE -ne 0) { throw 'Production REDox wrapper publish failed.' }
Copy-Item -LiteralPath (Join-Path $output 'redox/REDoxWrapper.dll') -Destination $output
& python (Join-Path $PSScriptRoot 'PrepareLayouts.py') $repository $output
if ($LASTEXITCODE -ne 0) { throw 'Layout generation failed.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools required.' }
$sources = @('Tools/SceneSettingsBenchmark/Benchmark.cpp', 'Game/Scene/FlightSceneLoader.cpp',
    'Engine/Resources/JsonAssetReader.cpp', 'Game/Cinematics/CinematicSettings.cpp',
    'Game/Scene/CloudSettingsLoader.cpp', 'Game/Combat/CombatSettingsLoader.cpp', 'Engine/Scene/CloudSettings.cpp',
    'Game/Combat/TargetingSystem.cpp', 'Game/Combat/GunAimSolver.cpp')
$flags = @('/nologo','/O2','/DNDEBUG','/std:c++20','/EHsc','/MD','/utf-8','/DNOMINMAX','/DUNICODE',
    ('/I"{0}"' -f $repository), ('/I"{0}"' -f $output),
    ('/I"{0}"' -f (Join-Path $repository 'packages/directxtex_desktop_win10.2026.5.8.1/include')),
    ('/FI"{0}"' -f (Join-Path $PSScriptRoot 'Backend.h')),
    ('/Fe"{0}/SceneSettingsBenchmark.exe"' -f $output), ('/Fo"{0}\\"' -f $output))
$flags += $sources | ForEach-Object { '"{0}"' -f (Join-Path $repository $_) }
$flags += '/link user32.lib'
$response = Join-Path $output 'compile.rsp'
$flags | Set-Content -LiteralPath $response -Encoding Unicode
$batch = Join-Path $output 'build.cmd'
@('@echo off', ('call "{0}/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul' -f $installation),
    ('cl.exe @"{0}"' -f $response), 'if errorlevel 1 exit /b %errorlevel%',
    ('cl.exe /nologo /O2 /std:c++20 /EHsc /MD /utf-8 /DNOMINMAX /DUNICODE /I"{0}" /Fe"{1}/CacheChecks.exe" /Fo"{1}\\" "{2}/CacheChecks.cpp" "{0}/Engine/Resources/JsonAssetReader.cpp"' -f $repository, $output, $PSScriptRoot),
    'if errorlevel 1 exit /b %errorlevel%',
    ('cl.exe /nologo /c /O2 /std:c++20 /EHsc /MD /utf-8 /DNOMINMAX /DUNICODE /I"{0}" /I"{0}/packages/directxtex_desktop_win10.2026.5.8.1/include" /I"{0}/packages/AssimpCpp.5.0.1.6/build/native/include" /Fo"{1}/CombatAudio.obj" "{0}/Game/Audio/CombatAudio.cpp"' -f $repository, $output),
    'if errorlevel 1 exit /b %errorlevel%') | Set-Content -LiteralPath $batch -Encoding utf8
& $env:ComSpec /d /c $batch
if ($LASTEXITCODE -ne 0) { throw 'Benchmark compilation failed.' }
& (Join-Path $output 'CacheChecks.exe') $output
if ($LASTEXITCODE -ne 0) { throw 'Document cache regression checks failed.' }
# Keep execution sequential and alternate which backend runs first per process pair.
for ($run = 1; $run -le $ProcessRuns; ++$run) {
    $order = if ($run % 2) { @('redox','nlohmann') } else { @('nlohmann','redox') }
    foreach ($backend in $order) {
        & (Join-Path $output 'SceneSettingsBenchmark.exe') $repository $output $backend $run
        if ($LASTEXITCODE -ne 0) { throw "$backend benchmark failed." }
    }
    $a = (Get-FileHash -LiteralPath (Join-Path $output "redox-$run-trace.txt") -Algorithm SHA256).Hash
    $b = (Get-FileHash -LiteralPath (Join-Path $output "nlohmann-$run-trace.txt") -Algorithm SHA256).Hash
    if ($a -ne $b) { throw "Returned data/call trace differs for process pair $run. Do not use timings." }
}
$summary = foreach ($backend in @('redox','nlohmann')) {
    $values = @(1..$ProcessRuns | ForEach-Object {
        Import-Csv -LiteralPath (Join-Path $output "$backend-$_.csv") | ForEach-Object { [double]$_.microseconds_per_load }
    } | Sort-Object)
    [pscustomobject]@{backend=$backend; samples=$values.Count; median_us=$values[[int][Math]::Floor($values.Count/2)]; min_us=$values[0]; max_us=$values[-1]}
}
$summary | Export-Csv -LiteralPath (Join-Path $output 'summary.csv') -NoTypeInformation
$summary | Format-Table
Get-CimInstance Win32_Processor | Select-Object Name | Out-File (Join-Path $output 'environment.txt')
& dotnet --info | Out-File (Join-Path $output 'environment.txt') -Append
Get-ChildItem -LiteralPath (Join-Path $repository 'Assets') -Recurse -Filter '*.json' | Get-FileHash -Algorithm SHA256 |
    Export-Csv -LiteralPath (Join-Path $output 'input-hashes.csv') -NoTypeInformation
