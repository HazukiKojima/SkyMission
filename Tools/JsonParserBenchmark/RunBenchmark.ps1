param(
    [string]$OutputDirectory,
    [ValidateRange(1, 10)][int]$ProcessRuns = 3,
    [switch]$ManagedSupplement,
    [switch]$IncludeGameOptimized,
    [switch]$FairOptimized
)
$ErrorActionPreference = 'Stop'
if ($FairOptimized -and $IncludeGameOptimized) { throw 'Choose FairOptimized or IncludeGameOptimized.' }
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repository 'x64/JsonParserBenchmark' }
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force (Join-Path $output 'deps') | Out-Null

# 比較用の依存物はビルド出力へ置き、ゲーム本体の依存関係は変更しない
$header = Join-Path $output 'deps/json.hpp'
$expectedHash = 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63'
if (!(Test-Path -LiteralPath $header)) {
    Invoke-WebRequest 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' -OutFile $header
}
if ((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'nlohmann/json 3.12.0 header hash mismatch.'
}

& dotnet build (Join-Path $PSScriptRoot 'JsonParserBenchmark.csproj') -c Release --ignore-failed-sources -p:NuGetAudit=false
if ($LASTEXITCODE -ne 0) { throw 'Dataset preparation build failed.' }
$managed = Join-Path $PSScriptRoot 'bin/Release/net10.0/JsonParserBenchmark.dll'
& dotnet $managed $repository $output prepare
if ($LASTEXITCODE -ne 0) { throw 'Dataset preparation failed.' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ x64 tools are required.' }
$commands = @('@echo off', ('call "{0}\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul' -f $installation))
$parsers = @('Redox', 'Standard')
if ($FairOptimized) { $parsers = @('MatchedRedox', 'MatchedStandard') }
if ($IncludeGameOptimized) { $parsers += 'GameOptimized' }
foreach ($parser in $parsers) {
    $project = Join-Path $PSScriptRoot "NativeBridge/$parser/${parser}Benchmark.csproj"
    if ($parser -eq 'GameOptimized') { $project = Join-Path $PSScriptRoot 'NativeBridge/GameOptimized/GameOptimized.csproj' }
    if ($parser.StartsWith('Matched')) { $project = Join-Path $PSScriptRoot "NativeBridge/$parser/$parser.csproj" }
    $commands += 'dotnet publish "{0}" -c Release -r win-x64 -o "{1}" --ignore-failed-sources -p:NuGetAudit=false' -f $project, $output
    $commands += 'if errorlevel 1 exit /b %errorlevel%'
}
$commands += 'cl.exe /nologo /O2 /DNDEBUG /std:c++20 /EHsc /MD /utf-8 /I"{0}\deps" "{1}" /Fe"{0}\NativeBenchmark.exe" /Fo"{0}\NativeBenchmark.obj"' -f $output, (Join-Path $PSScriptRoot 'NativeBenchmark.cpp')
$batch = Join-Path $output 'build-native.cmd'
$commands | Set-Content -LiteralPath $batch -Encoding utf8
& $env:ComSpec /d /c $batch
if ($LASTEXITCODE -ne 0) { throw 'Native benchmark build failed.' }

# 計測同士を並行実行しない。プロセスごとに7標本、測定順は巡回して入れ替える
for ($run = 1; $run -le $ProcessRuns; ++$run) {
    $nativeArguments = @($output, $run)
    if ($IncludeGameOptimized) { $nativeArguments += 'optimized' }
    if ($FairOptimized) { $nativeArguments += 'fair' }
    & (Join-Path $output 'NativeBenchmark.exe') @nativeArguments
    if ($LASTEXITCODE -ne 0) { throw "Native benchmark run $run failed." }
}
if ($ManagedSupplement) {
    $previousTiered = $env:DOTNET_TieredCompilation
    $previousReadyToRun = $env:DOTNET_ReadyToRun
    try {
        $env:DOTNET_TieredCompilation = '0'
        $env:DOTNET_ReadyToRun = '0'
        & dotnet $managed $repository $output 1
        if ($LASTEXITCODE -ne 0) { throw 'Managed supplement failed.' }
    } finally {
        $env:DOTNET_TieredCompilation = $previousTiered
        $env:DOTNET_ReadyToRun = $previousReadyToRun
    }
}
Write-Output "Raw CSV and environment information: $output"
