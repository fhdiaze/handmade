param(
    [Parameter(Mandatory=$false)]
    [Alias("m")]
    [ValidateSet("debug", "release")]
    [string]$BuildMode = "debug",

    [Alias("a")]
    [Parameter(Mandatory=$false)]
    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x64",

    [Alias("lb")]
    [Parameter(Mandatory=$false)]
    [switch]$LiveBuild
)

# Setup
$PlatformFile = ".\src\win_handmade.c"
$GameFile = ".\src\game.c"
$Outdir = ".\bin"
$PlatformFileName = [System.IO.Path]::GetFileNameWithoutExtension($PlatformFile)
$GameFileName = [System.IO.Path]::GetFileNameWithoutExtension($GameFile)
$OutPlatform = Join-Path $Outdir "$PlatformFileName.exe"
$OutGame = Join-Path $Outdir "$GameFileName.dll"

# Clean
Write-Host "Cleaning $Outdir..."

if ($LiveBuild) {
    Remove-Item $Outdir/*.txt -ErrorAction SilentlyContinue
    Remove-Item $Outdir/*.pdb -ErrorAction SilentlyContinue
} else {
    Remove-Item -Path $Outdir -Recurse -Force -ErrorAction SilentlyContinue
}

if (!(Test-Path $Outdir)) {
    New-Item -ItemType Directory -Path $Outdir | Out-Null
}

# Read flags from file
$Flags = Get-Content "compile_flags.txt" |
    Where-Object { $_.Trim() -ne "" -and -not $_.StartsWith("//") } |
    ForEach-Object { $_.Trim().TrimEnd(',') } |
    Where-Object { $_ -ne "" }

# Add architecture flag
if ($Architecture -eq "x86") {
    $Flags += "-m32"
    Write-Host "Building for 32-bit (x86)..."
} else {
    $Flags += "-m64"
    Write-Host "Building for 64-bit (x64)..."
}

# Add build mode specific flags
if ($BuildMode -eq "debug") {
    $Flags += @(
        "-g",              # Debug symbols
        "-gcodeview",
        "-O0",             # No optimization
        "-DDEBUG",         # Define DEBUG macro
        "-Wl,/DEBUG:FULL",
        "-fms-runtime-lib=static_dbg"             # Build statically with C runtime lib
    )
    #$Flags += "-fsanitize=address"
    #$Flags += "-fno-omit-frame-pointer"
    Write-Host "Building in DEBUG mode..."
} else {
    $Flags += @(
        "-O3",             # Maximum optimization
        "-DNDEBUG",        # Define NDEBUG macro
        "-flto",           # Link-time optimization
        "-Wl,/opt:ref",
        "-Wl,/opt:icf",
        "-fms-runtime-lib=static"
    )
    Write-Host "Building in RELEASE mode..."
}

Write-Host "Compiling $GameFile -> $OutGame"

$random = Get-Random -Minimum 0 -Maximum 99999
$GameFlags = $Flags + @(
    "-Wl,/MAP:$Outdir/$GameFileName.map,/MAPINFO:EXPORTS",
    "-Wl,/EXPORT:game_sound_create_samples",
    "-Wl,/EXPORT:game_bitmap_update_and_render",
    "-Wl,/PDB:$Outdir/game_$random.pdb",
    "-shared"
)

Write-Host "GameFlags: $($GameFlags -join ' ')"

clang @GameFlags $GameFile -o $OutGame

if ($LASTEXITCODE -ne 0) {
    Write-Host "Game DLL build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Game DLL build succeeded!" -ForegroundColor Green

if ($LiveBuild) {
    Write-Host "Live build completed. Skipping platform build." -ForegroundColor Cyan
    return
}

Write-Host "Compiling $PlatformFile -> $OutPlatform"

$PlatformFlags = $Flags + @(
    "-luser32",
    "-lgdi32",
    "-lwinmm",
    "-Wl,/subsystem:windows",
    "-Wl,/MAP:$Outdir/$PlatformFileName.map,/MAPINFO:EXPORTS"
)

Write-Host "PlatformFlags: $($PlatformFlags -join ' ')"

clang @PlatformFlags $PlatformFile -o $OutPlatform

if ($LASTEXITCODE -ne 0) {
    Write-Host "Platform EXE build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Platform EXE build succeeded!" -ForegroundColor Green
