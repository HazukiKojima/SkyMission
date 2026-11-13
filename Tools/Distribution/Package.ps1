param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools are required.' }
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
if (!$SkipBuild) {
    & $msbuild (Join-Path $repository 'SkyMission.slnx') /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
    if ($LASTEXITCODE) { throw 'Release build failed.' }
}
$build = Join-Path $repository 'x64/Release'
# 毎回新しいディレクトリに作成し、過去の配布物や余分なファイルを混ぜない。
$output = Join-Path $repository ('artifacts/distribution/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$package = Join-Path $output 'SkyMission'
$runtime = Join-Path $package 'Runtime'
$shaders = Join-Path $runtime 'Shaders'
New-Item -ItemType Directory -Force $shaders | Out-Null
foreach ($name in @('SkyMission.exe', 'REDoxWrapper.dll', 'assimp-vc142-mt.dll')) {
    Copy-Item -LiteralPath (Join-Path $build $name) -Destination $runtime
}
$compiledShaders = @(Get-ChildItem -LiteralPath $build -Filter '*.cso')
if (!$compiledShaders.Count) { throw 'Compiled shaders are missing.' }
$compiledShaders | Copy-Item -Destination $shaders
# VCランタイムも同梱し、配布先にVisual Studioのインストールを要求しない。
$redistRoot = Join-Path $installation 'VC/Redist/MSVC'
$crt = Get-ChildItem -LiteralPath $redistRoot -Filter 'Microsoft.VC*.CRT' -Directory -Recurse |
    Where-Object { $_.Parent.Name -eq 'x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (!$crt) { throw 'Redistributable x64 VC runtime was not found.' }
Get-ChildItem -LiteralPath $crt.FullName -Filter '*.dll' | Copy-Item -Destination $runtime
Copy-Item -LiteralPath (Join-Path $repository 'Assets') -Destination $package -Recurse
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'はじめに.txt') -Destination $package
$developerCommand = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$launcher = Join-Path $PSScriptRoot 'Launcher.cpp'
$batch = Join-Path $output 'build-launcher.cmd'
@('@echo off', ('call "{0}" -arch=x64 -host_arch=x64 >nul' -f $developerCommand),
    ('cl.exe /nologo /O1 /GS- /Zl /utf-8 /Fo"{0}\Launcher.obj" /Fe"{1}\SkyMission.exe" "{2}" /link /ENTRY:LauncherEntry /SUBSYSTEM:WINDOWS /NODEFAULTLIB kernel32.lib user32.lib' -f $output, $package, $launcher),
    'exit /b %errorlevel%') | Set-Content -LiteralPath $batch -Encoding utf8
& $env:ComSpec /d /c $batch
if ($LASTEXITCODE) { throw 'Launcher build failed.' }
$zip = Join-Path $output 'SkyMission.zip'
Compress-Archive -LiteralPath $package -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Distribution: $package"
Write-Host "ZIP: $zip"
