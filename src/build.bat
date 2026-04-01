set PATH=C:\msys64\mingw64\bin;%PATH%

REM Read Version from file
set /p VERSION=<..\VERSION
echo Building HayaletDPI v%VERSION%...

del *.o
mingw32-make CPREFIX=x86_64-w64-mingw32- BIT64=1 WINDIVERTHEADERS=../WinDivert-2.2.0-D/include WINDIVERTLIBS=../WinDivert-2.2.0-D/x64 -j4
copy /Y hayalet.exe ..\bin\x86_64\hayalet.exe

echo Done. Generating Setup...

REM Generate Inno Setup Installer with specific version filename
cd ..
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyAppVersion=%VERSION% setup.iss
cd src

echo Build and Setup finished for v%VERSION%!

REM Smart Cleanup: Remove build artifacts to keep the source directory clean
echo Cleaning up build artifacts...
del *.o
if exist hayalet.exe del /f /q hayalet.exe

echo All clean! Your project is ready in bin/x86_64 and releases/.
pause
