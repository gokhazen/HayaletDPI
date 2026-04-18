set PATH=C:\msys64\mingw64\bin;%PATH%

REM Read Version from file
set /p VERSION=<..\VERSION
echo [SYSTEM] Starting Build for HayaletDPI v%VERSION%...

REM Update version.h dynamically from VERSION file
echo [SYSTEM] Syncing version.h...
echo #define HAYALET_VERSION "%VERSION%" > version.h
echo #define HAYALET_FULL_TITLE "HayaletDPI v%VERSION% Platinum Edition" >> version.h
echo #define HAYALET_SUBTITLE "Developed by Gokhan Ozen" >> version.h

REM Forced Cleanup: Remove any old binaries to prevent "ghost" files
del /q *.o 2>nul
del /q hayalet.exe 2>nul
del /q "..\bin\x86_64\hayalet.exe" 2>nul

echo [SYSTEM] Compiling...
mingw32-make CPREFIX=x86_64-w64-mingw32- BIT64=1 WINDIVERTHEADERS=../WinDivert-2.2.0-D/include WINDIVERTLIBS=../WinDivert-2.2.0-D/x64 -j4

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed! Check the output above for errors.
    pause
    exit /b %ERRORLEVEL%
)

if not exist "hayalet.exe" (
    echo.
    echo [ERROR] Compilation finished but hayalet.exe was not created!
    pause
    exit /b 1
)

REM Portable Distribution
echo [SYSTEM] Preparing Portable Binaries...
if not exist "..\bin\x86_64" mkdir "..\bin\x86_64"
copy /Y hayalet.exe "..\bin\x86_64\hayalet.exe"
copy /Y "..\WinDivert-2.2.0-D\x64\WinDivert.dll" "..\bin\x86_64\"
copy /Y "..\WinDivert-2.2.0-D\x64\WinDivert64.sys" "..\bin\x86_64\"
copy /Y "webview2\build\native\x64\WebView2Loader.dll" "..\bin\x86_64\"

REM Optional: Copy licenses (userfiles is already in bin usually)
xcopy /E /I /Y "..\licenses" "..\bin\x86_64\licenses" >nul
xcopy /E /I /Y "..\ui_v2" "..\bin\x86_64\ui_v2" >nul

echo [SYSTEM] Generating Setup...
cd ..
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyAppVersion=%VERSION% setup.iss
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Setup creation failed!
    cd src
    pause
    exit /b 1
)
cd src

echo.
echo [SUCCESS] Build and Setup finished for v%VERSION%!
echo Portable: bin\x86_64\
echo Setup: releases\

REM Smart Cleanup
del *.o 2>nul
if exist hayalet.exe del /f /q hayalet.exe

echo [SYSTEM] Done.
pause
