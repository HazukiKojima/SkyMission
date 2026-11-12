@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
MSBuild.exe "C:\Users\user\DX12\SkyMission/SkyMission.vcxproj" /m:2 /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal
