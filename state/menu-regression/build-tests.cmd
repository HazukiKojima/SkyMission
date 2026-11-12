@echo off
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl.exe @"C:\Users\user\DX12\SkyMission\state\menu-regression\compile.rsp"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\BasicPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\BasicPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\BasicVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\BasicVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\BasicVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\CloudPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\CloudPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\CloudPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\CloudPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\CloudPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\CloudPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\HudPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\HudPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\HudPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\HudPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\HudPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\HudPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\HudVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\HudVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\HudVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\HudVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\HudVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\HudVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\ModelPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\ModelPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\ModelVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\ModelVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\ModelVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanFogPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanFogPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanFogVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanFogVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanFogVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\OceanVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\OceanVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\RetroFilmPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\RetroFilmPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\RetroFilmVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\RetroFilmVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\RetroFilmVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyPS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\SkyPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T ps_5_0 /E PS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyPS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\SkyPS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyPS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyPS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /Od /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyVS-Od.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\SkyVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
fxc.exe /nologo /WX /O3 /T vs_5_0 /E VS /Fo "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyVS-O3.cso" "C:\Users\user\DX12\SkyMission\Engine\Renderer\Shader\SkyVS.hlsl"
if errorlevel 1 exit /b %errorlevel%
copy /y "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyVS-Od.cso" "C:\Users\user\DX12\SkyMission\state\menu-regression\SkyVS.cso" >nul
if errorlevel 1 exit /b %errorlevel%
exit /b 0
