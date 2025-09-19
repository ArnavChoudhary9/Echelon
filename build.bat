@echo off
echo Generating project files...
.\Vendor\premake5.exe gmake

echo.
echo Building Debug configuration...
make config=debug

echo.
echo Building Release configuration...
mkdir bin\Release-windows-x86_64\Sandbox 2>nul
make config=release
copy bin\Release-windows-x86_64\Echelon\Echelon.dll bin\Release-windows-x86_64\Sandbox\ >nul

echo.
echo Build complete!
echo Debug executable: bin\Debug-windows-x86_64\Sandbox\Sandbox.exe
echo Release executable: bin\Release-windows-x86_64\Sandbox\Sandbox.exe