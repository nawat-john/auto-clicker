@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo  Building Ultra Auto Clicker and Key Spammer (C++)
echo ===================================================

set "PATH=C:\msys64\ucrt64\bin;%PATH%"

set CXX=g++.exe
set WINDRES=windres.exe

echo [1/2] Compiling Windows Resources (Manifest ^& Styles)...
%WINDRES% -i res/resource.rc -o res/resource.o -O coff
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile resources.
    exit /b %errorlevel%
)

echo [2/2] Compiling C++20 Source Files...
%CXX% -std=c++20 -O3 -Wall -Wextra -DUNICODE -D_UNICODE ^
    src/main.cpp ^
    src/app_ui.cpp ^
    src/engine.cpp ^
    src/config.cpp ^
    src/hotkeys.cpp ^
    src/registry.cpp ^
    res/resource.o ^
    -o clicker.exe ^
    -mwindows ^
    -municode ^
    -static ^
    -lcomctl32 ^
    -lwinmm ^
    -lgdi32 ^
    -ldwmapi ^
    -luxtheme ^
    -ladvapi32

if %errorlevel% equ 0 (
    echo.
    echo ===================================================
    echo  [SUCCESS] clicker.exe built successfully!
    echo ===================================================
) else (
    echo.
    echo [ERROR] Compilation failed with error code %errorlevel%.
    exit /b %errorlevel%
)
