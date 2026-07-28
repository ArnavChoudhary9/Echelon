@echo off
setlocal EnableDelayedExpansion

set BUILD_DEBUG=0
set BUILD_RELEASE=0

if "%~1"=="" (
    set BUILD_DEBUG=1
    set BUILD_RELEASE=1
) else (
    :parse
    if "%~1"=="" goto done

    if /I "%~1"=="-d" (
        set BUILD_DEBUG=1
    ) else if /I "%~1"=="--debug" (
        set BUILD_DEBUG=1
    ) else if /I "%~1"=="-r" (
        set BUILD_RELEASE=1
    ) else if /I "%~1"=="--release" (
        set BUILD_RELEASE=1
    ) else if /I "%~1"=="-h" (
        goto help
    ) else if /I "%~1"=="--help" (
        goto help
    ) else (
        echo Unknown option: %~1
        exit /b 1
    )

    shift
    goto parse
)

:done

echo Generating project files...
Vendor\premake5.exe gmake

if "%BUILD_DEBUG%"=="1" (
    echo.
    echo Building Debug configuration...
    make config=debug
)

if "%BUILD_RELEASE%"=="1" (
    echo.
    echo Building Release configuration...
    make config=release
)

echo.
echo Build complete!
exit /b 0

:help
echo Usage: build.bat [OPTIONS]
echo.
echo Options:
echo   -d, --debug     Build Debug configuration
echo   -r, --release   Build Release configuration
echo   -h, --help      Show this help message
echo.
echo No options builds both Debug and Release.
exit /b 0
