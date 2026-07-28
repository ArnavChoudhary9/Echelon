#!/bin/bash

echo Generating project files...
Vendor/premake5 gmake

echo
echo Building Debug configuration...
make config=debug

echo
echo Building Release configuration...
make config=release

echo
echo Build complete!
echo Debug executable: bin/Debug-windows-x86_64/EchelonEditor/EchelonEditor
echo Release executable: bin/Release-windows-x86_64/EchelonEditor/EchelonEditor