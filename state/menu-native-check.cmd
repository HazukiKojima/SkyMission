@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl.exe /nologo /Zs /std:c++20 /EHsc /utf-8 /DNOMINMAX /DUNICODE /D_DEBUG /I. /Ipackages\directxtex_desktop_win10.2026.5.8.1\include /Ipackages\AssimpCpp.5.0.1.6\build\native\include Game\Application\Application.cpp Engine\Renderer\Hud\HudDrawList.cpp
