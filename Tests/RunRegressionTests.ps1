param([string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (!$BuildDirectory) { $BuildDirectory = Join-Path $env:TEMP ('SkyMissionRegression-' + [guid]::NewGuid().ToString('N')) }
$build = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Force $build | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ tools are required.' }
$developerCommand = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$directXTex = Join-Path $repository 'packages\directxtex_desktop_win10.2026.5.8.1\native'
$assimp = Join-Path $repository 'packages\AssimpCpp.5.0.1.6\build\native'
$redox = Join-Path $repository 'External\REDoxWrapper\bin\Debug\net10.0\win-x64\publish\REDoxWrapper.dll'
if (!(Test-Path -LiteralPath $redox)) { throw 'Build Debug x64 once to publish REDoxWrapper.dll before running these tests.' }
Copy-Item -LiteralPath $redox -Destination $build
Copy-Item -LiteralPath (Join-Path $assimp 'bin\Debug\assimp-vc142-mtd.dll') -Destination $build
$sources = @(Get-ChildItem -LiteralPath (Join-Path $repository 'Engine') -Filter '*.cpp' -Recurse |
    Where-Object Name -ne 'Application.cpp' | ForEach-Object FullName)
$sources += @(Get-ChildItem -LiteralPath (Join-Path $repository 'Game') -Filter '*.cpp' -Recurse |
    Where-Object Name -ne 'Application.cpp' | ForEach-Object FullName)
$sources += Join-Path $PSScriptRoot 'RegressionTests.cpp'
$response = @('/nologo', '/std:c++20', '/EHsc', '/MDd', '/utf-8', '/DNOMINMAX', '/DUNICODE', '/D_DEBUG',
    '/W4', '/FS', ('/I"{0}"' -f $repository), ('/I"{0}"' -f (Join-Path $repository 'packages\directxtex_desktop_win10.2026.5.8.1\include')),
    ('/I"{0}"' -f (Join-Path $assimp 'include')), ('/Fe"{0}\RegressionTests.exe"' -f $build),
    ('/Fo"{0}\\"' -f $build))
$response += $sources | ForEach-Object { '"{0}"' -f $_ }
$response += @('/link', '/INCREMENTAL:NO', '/DEBUG:NONE',
    ('"{0}"' -f (Join-Path $directXTex 'lib\x64\Debug\DirectXTex.lib')),
    ('"{0}"' -f (Join-Path $assimp 'lib\Debug\assimp-vc142-mtd.lib')),
    'd3d12.lib', 'dxgi.lib', 'dxguid.lib', 'd3dcompiler.lib', 'windowscodecs.lib', 'ole32.lib', 'user32.lib', 'gdi32.lib')
$responsePath = Join-Path $build 'compile.rsp'
$linkIndex = [array]::IndexOf($response, '/link')
$response = @($response[0..($linkIndex-1)]) + (($response[$linkIndex..($response.Length-1)]) -join ' ')
$response | Set-Content -LiteralPath $responsePath -Encoding Unicode
$shaderCommands = @()
foreach ($shader in Get-ChildItem -LiteralPath (Join-Path $repository 'Engine\Renderer\Shader') -Filter '*.hlsl') {
    if ($shader.BaseName.EndsWith('VS')) { $profile = 'vs_5_0'; $entry = 'VS' }
    elseif ($shader.BaseName.EndsWith('PS')) { $profile = 'ps_5_0'; $entry = 'PS' }
    else { continue }
    $shaderCommands += 'fxc.exe /nologo /T {0} /E {1} /Fo "{2}\{3}.cso" "{4}"' -f $profile, $entry, $build, $shader.BaseName, $shader.FullName
    $shaderCommands += 'if errorlevel 1 exit /b %errorlevel%'
}
$batch = Join-Path $build 'build-tests.cmd'
(@('@echo off', 'chcp 65001 >nul', ('call "{0}" -arch=x64 -host_arch=x64 >nul' -f $developerCommand),
    ('cl.exe @"{0}"' -f $responsePath), 'if errorlevel 1 exit /b %errorlevel%') + $shaderCommands + @('exit /b 0')) | Set-Content -LiteralPath $batch -Encoding utf8
& $env:ComSpec /d /c $batch
if ($LASTEXITCODE -ne 0) { throw "Regression test compilation failed: $LASTEXITCODE" }
& (Join-Path $build 'RegressionTests.exe') $repository (Join-Path $build 'fixtures')
if ($LASTEXITCODE -ne 0) { throw "Regression tests failed: $LASTEXITCODE" }
Write-Host "Regression tests passed. Build directory: $build"
