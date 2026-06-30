@echo off

setlocal enabledelayedexpansion

set "BuildMode=debug"
set "Architecture=x64"
set "LiveBuild=0"

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
shift
goto :parse_args

:done_args

if /i not "%BuildMode%"=="debug" if /i not "%BuildMode%"=="release" (
    echo Error: Invalid build mode "%BuildMode%". Must be "debug" or "release".
    exit /b 1
)

if /i not "%Architecture%"=="x86" if /i not "%Architecture%"=="x64" (
    echo Error: Invalid architecture "%Architecture%". Must be "x86" or "x64".
    exit /b 1
)

set "CoreFileName=game"
set "PlatformFileName=win_handmade"
set "PlatformFilePath=.\src\%PlatformFileName%.c"
set "CoreFilePath=.\src\%CoreFileName%.c"
set "Outdir=.\bin"
set "Datadir=.\data"
set "OutPlatformFileName=%Outdir%\%PlatformFileName%.exe"
set "OutCoreFileName=%Outdir%\%CoreFileName%.dll"

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

REM Read flags from file
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
    set "Flags=!Flags! -g"
    set "Flags=!Flags! -gcodeview"
    set "Flags=!Flags! -O0"
    set "Flags=!Flags! -DDEBUG"
    set "Flags=!Flags! -Wl,/DEBUG:FULL"
    set "Flags=!Flags! -fms-runtime-lib=static_dbg"
    @REM set "Flags=!Flags! -fsanitize=address"
    @REM set "Flags=!Flags! -fno-omit-frame-pointer"
    echo Building in DEBUG mode...
) else (
    set "Flags=!Flags! -O3"
    set "Flags=!Flags! -DNDEBUG"
    set "Flags=!Flags! -flto"
    set "Flags=!Flags! -Wl,/opt:ref"
    set "Flags=!Flags! -Wl,/opt:icf"
    set "Flags=!Flags! -fms-runtime-lib=static"
    echo Building in RELEASE mode...
)

set "CoreFlags=!Flags!"
set "CoreFlags=!CoreFlags! -Wl,/MAP:%Outdir%/%CoreFileName%.map"
set "CoreFlags=!CoreFlags! -Wl,/MAPINFO:EXPORTS"
set "CoreFlags=!CoreFlags! -Wl,/EXPORT:sound_create_samples"
set "CoreFlags=!CoreFlags! -Wl,/EXPORT:game_update_and_render"
set "CoreFlags=!CoreFlags! -Wl,/PDB:%Outdir%/%CoreFileName%_%random%.pdb"
set "CoreFlags=!CoreFlags! -shared"

echo Building %OutCoreFileName% ...
echo.
echo clang !CoreFlags! %CoreFilePath% -o %OutCoreFileName%
echo.
echo Waiting for pdb file>"%Outdir%\lock.tmp"
echo.

clang !CoreFlags! %CoreFilePath% -o %OutCoreFileName%

del /q "%Outdir%\lock.tmp" 2>nul

if errorlevel 1 (
    echo Building tix dll failed!
    exit /b %errorlevel%
)

echo.
echo Building tix dll succeeded!
echo.

if %LiveBuild% equ 1 (
    echo Live build completed. Skipping platform build.
    exit /b 0
)

set "PlatformFlags=!Flags!"
set "PlatformFlags=!PlatformFlags! -luser32"
set "PlatformFlags=!PlatformFlags! -lgdi32"
set "PlatformFlags=!PlatformFlags! -lwinmm"
set "PlatformFlags=!PlatformFlags! -Wl,/subsystem:windows"
set "PlatformFlags=!PlatformFlags! -Wl,/MAP:%Outdir%/%PlatformFileName%.map"
set "PlatformFlags=!PlatformFlags! -Wl,/MAPINFO:EXPORTS"

echo Building %OutPlatformFileName% ...
echo.
echo clang !PlatformFlags! %PlatformFilePath% -o %OutPlatformFileName%
echo.

clang !PlatformFlags! %PlatformFilePath% -o %OutPlatformFileName%

if errorlevel 1 (
    echo Building platform exe failed!
    exit /b %errorlevel%
)

echo.
echo Building platform exe succeeded!
