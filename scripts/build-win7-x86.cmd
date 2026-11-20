@echo off
setlocal
set ROOT=%~dp0..
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set VSPATH=
set VSPATH_FILE=%TEMP%\cursorhalo-vspath.txt
%VSWHERE% -products * -version "[17.0,18.0)" -property installationPath > "%VSPATH_FILE%"
set /p VSPATH=<"%VSPATH_FILE%"
del "%VSPATH_FILE%" >nul 2>nul
if "%VSPATH%"=="" (
    echo Cannot find Visual Studio 2022 Build Tools.
    exit /b 1
)
set CMAKE=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
if not exist "%CMAKE%" (
    echo Cannot find the CMake bundled with Build Tools.
    exit /b 1
)
REM Always reconfigure so resource/source-list changes are picked up.
"%CMAKE%" -S "%ROOT%" -B "%ROOT%\build\win7-x86" ^
    -G "Visual Studio 17 2022" -A Win32 -T v141 ^
    -DCMAKE_SYSTEM_VERSION=10.0.19041.0
if errorlevel 1 exit /b 2
"%CMAKE%" --build "%ROOT%\build\win7-x86" --config Release
if errorlevel 1 exit /b 3
if not exist "%ROOT%\dist" mkdir "%ROOT%\dist"
copy /y "%ROOT%\build\win7-x86\Release\ZaYuCursorHalo.exe" "%ROOT%\dist\ZaYuCursorHalo.exe" >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\check-win7-compat.ps1" -Exe "%ROOT%\dist\ZaYuCursorHalo.exe" -VsPath "%VSPATH%"
if errorlevel 1 (
    echo.
    echo Win7 compatibility gate failed. Build rejected.
    exit /b 4
)
echo Built: %ROOT%\dist\ZaYuCursorHalo.exe
