@echo off

setlocal enabledelayedexpansion

REM Captured now because "shift" (used below during argument parsing) can end up
REM shifting %0 too once all positional args are consumed, which would corrupt %~dp0.
set "ScriptDir=%~dp0"

set "BuildMode=debug"
set "Architecture=x64"
set "LiveBuild=0"
set "AppFileName=app"
set "PlatformFileName=plat_win"
set "PlatformFilePath=./src/%PlatformFileName%.c"
set "AppFilePath=./src/%AppFileName%.c"
set "Outdir=./bin"
set "Datadir=./data"
set "OutAppFileName=handmade_app"
set "OutPlatFileName=win_handmade"
set "OutPlatformFilePath=%Outdir%/%OutPlatFileName%.exe"
set "OutAppFilePath=%Outdir%/%OutAppFileName%.dll"
set "FlagsFile=%ScriptDir%../compile_flags.txt"
set "DebugFlags=-g -gcodeview -O0 -DDEBUG -Wl,/DEBUG:FULL -fms-runtime-lib=static_dbg"
@REM set "DebugFlags=!DebugFlags! -fsanitize=address -fno-omit-frame-pointer"
set "ReleaseFlags=-O3 -DNDEBUG -flto -Wl,/opt:ref -Wl,/opt:icf -fms-runtime-lib=static"
set "Flags="
set "AppFlags=-shared -Wl,/MAP:%Outdir%/%OutAppFileName%.map,/MAPINFO:EXPORTS -Wl,/EXPORT:sound_create_samples -Wl,/EXPORT:game_update_and_render -Wl,/PDB:%Outdir%/%OutAppFileName%_%random%.pdb"
set "PlatformFlags=-luser32 -lgdi32 -lwinmm -Wl,/subsystem:windows -Wl,/MAP:%Outdir%/%OutPlatFileName%.map,/MAPINFO:EXPORTS"

:parse_args

if "%~1"=="" goto :done_args
if /i "%~1"=="/m" (
    if "%~2"=="" (
        echo Error: /m requires a value. & exit /b 1
    )
    set "BuildMode=%~2" & shift & shift & goto :parse_args
)
if /i "%~1"=="/a" (
    if "%~2"=="" (
        echo Error: /a requires a value. & exit /b 1
    )
    set "Architecture=%~2" & shift & shift & goto :parse_args
)
if /i "%~1"=="/lb" (
    set "LiveBuild=1"       & shift        & goto :parse_args
)
echo Error: Unknown argument "%~1".
exit /b 1

:done_args

if /i not "%BuildMode%"=="debug" if /i not "%BuildMode%"=="release" (
    echo Error: Invalid build mode "%BuildMode%". Must be "debug" or "release".
    exit /b 1
)

if /i not "%Architecture%"=="x86" if /i not "%Architecture%"=="x64" (
    echo Error: Invalid architecture "%Architecture%". Must be "x86" or "x64".
    exit /b 1
)

if not exist "%Outdir%" (
    echo Creating %Outdir%...
    mkdir "%Outdir%"
) else (
    echo Cleaning %Outdir%...
    if %LiveBuild% equ 1 (
        del /q "%Outdir%\*.txt" 2>nul
        del /q "%Outdir%\*.pdb" 2>nul
    ) else (
        del /s /q "%Outdir%\*" 2>nul
    )
)

if not exist "%Datadir%" (
    echo Creating %Datadir%...
    mkdir "%Datadir%"
) else (
    echo Cleaning %Datadir%...
    del /q "%Datadir%\log.txt" 2>nul
)

REM Read flags from file (path is relative to this script's location, not the caller's cwd)
for /f "usebackq tokens=*" %%A in ("%FlagsFile%") do (
    set "line=%%A"
    set "line=!line: =!"
    if not "!line!"=="" if not "!line:~0,2!"=="//" (
        set "Flags=!Flags! %%A"
    )
)

if "%Architecture%"=="x86" (
    set "Flags=!Flags! -m32"
    echo Building for 32-bit ^(x86^)...
) else (
    set "Flags=!Flags! -m64"
    echo Building for 64-bit ^(x64^)...
)

if "%BuildMode%"=="debug" (
    set "Flags=!Flags! %DebugFlags%"
    echo Building in DEBUG mode...
) else (
    set "Flags=!Flags! %ReleaseFlags%"
    echo Building in RELEASE mode...
)

set "AppFlags=!AppFlags! !Flags!"

echo Building %OutAppFilePath% ...
echo.
echo clang !AppFlags! %AppFilePath% -o %OutAppFilePath%
echo.
echo Waiting for pdb file>"%Outdir%\lock.tmp"
echo.

clang !AppFlags! %AppFilePath% -o %OutAppFilePath%

del /q "%Outdir%\lock.tmp" 2>nul

if errorlevel 1 (
    echo Building %OutAppFilePath% failed!
    exit /b %errorlevel%
)

echo.
echo Building %OutAppFilePath% succeeded!
echo.

echo ================================================================================
echo.

if %LiveBuild% equ 1 (
    echo Live build completed. Skipping platform build.
    exit /b 0
)

set "PlatformFlags=!PlatformFlags! !Flags!"

echo Building %OutPlatformFilePath% ...
echo.
echo clang !PlatformFlags! %PlatformFilePath% -o %OutPlatformFilePath%
echo.

clang !PlatformFlags! %PlatformFilePath% -o %OutPlatformFilePath%

if errorlevel 1 (
    echo Building %OutPlatformFilePath% failed!
    exit /b %errorlevel%
)

echo.
echo Building %OutPlatformFilePath% succeeded!
