@echo off
setlocal enabledelayedexpansion

set "BuildMode=debug"
set "Architecture=x64"
set "LiveBuild=0"

:parse_args

if "%~1"=="" goto :done_args
if /i "%~1"=="/m"  ( set "BuildMode=%~2"    & shift & shift & goto :parse_args )
if /i "%~1"=="/a"  ( set "Architecture=%~2" & shift & shift & goto :parse_args )
if /i "%~1"=="/lb" ( set "LiveBuild=1"       & shift        & goto :parse_args )
shift
goto :parse_args

:done_args

set "GameFileName=game"
set "PlatformFileName=win_handmade"
set "PlatformFilePath=.\src\%PlatformFileName%.c"
set "GameFilePath=.\src\%GameFileName%.c"
set "Outdir=.\bin"
set "Datadir=.\data"
set "OutPlatform=%Outdir%\%PlatformFileName%.exe"
set "OutGame=%Outdir%\%GameFileName%.dll"

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

:: Read flags from file
setlocal enabledelayedexpansion
set "Flags="
for /f "tokens=*" %%A in (compile_flags.txt) do (
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
    set "Flags=!Flags! -g -gcodeview -O0 -DDEBUG -Wl,/DEBUG:FULL -fms-runtime-lib=static_dbg"
    echo Building in DEBUG mode...
) else (
    set "Flags=!Flags! -O3 -DNDEBUG -flto -Wl,/opt:ref -Wl,/opt:icf -fms-runtime-lib=static"
    echo Building in RELEASE mode...
)

set "GameFlags=!Flags! -Wl,/MAP:%Outdir%/%GameFileName%.map,/MAPINFO:EXPORTS -Wl,/EXPORT:sound_create_samples -Wl,/EXPORT:game_update_and_render -Wl,/PDB:%Outdir%/game_%random%.pdb -shared"

echo Building game dll...
echo.
echo clang !GameFlags! %GameFilePath% -o %OutGame%
echo.
echo Waiting for pdb file>"%Outdir%\lock.tmp"
echo.

clang !GameFlags! %GameFilePath% -o %OutGame%

del /q "%Outdir%\lock.tmp" 2>nul

if errorlevel 1 (
    echo Building game dll failed!
    exit /b %errorlevel%
)

echo.
echo Building game dll succeeded!
echo.

if %LiveBuild% equ 1 (
    echo Live build completed. Skipping platform build.
    exit /b 0
)

set "PlatformFlags=!Flags! -luser32 -lgdi32 -lwinmm -Wl,/subsystem:windows -Wl,/MAP:%Outdir%/%PlatformFileName%.map,/MAPINFO:EXPORTS"

echo Building platform exe...
echo.
echo clang !PlatformFlags! %PlatformFilePath% -o %OutPlatform%
echo.

clang !PlatformFlags! %PlatformFilePath% -o %OutPlatform%

if errorlevel 1 (
    echo Building platform exe failed!
    exit /b %errorlevel%
)

echo.
echo Building platform exe succeeded!
