param(
    [Parameter(Mandatory=$false)]
    [Alias("m")]
    [ValidateSet("debug", "release")]
    [string]$BuildMode = "debug",

    [Parameter(Mandatory=$false)]
    [Alias("a")]
    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x64"
)

$PlatformFile = ".\src\win_handmade.c"
$GameFile = ".\src\game.c"

# Get the base filename without extension
$PlatformFileName = [System.IO.Path]::GetFileNameWithoutExtension($PlatformFile)
$GameFileName = [System.IO.Path]::GetFileNameWithoutExtension($GameFile)

$Outdir = ".\bin"
if (!(Test-Path $Outdir)) {
    New-Item -ItemType Directory -Path $Outdir | Out-Null
}
$OutPlatform = Join-Path $Outdir "$PlatformFileName.exe"
$OutGame = Join-Path $Outdir "$GameFileName.dll"

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
        "-Wl,/DEBUG:FULL"
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
        "-Wl,/opt:icf"
    )
    Write-Host "Building in RELEASE mode..."
}

Write-Host "Compiling $GameFile -> $OutGame"

$GameFlags = $Flags + @(
    "-Wl,/MAP:bin/$GameFileName.map,/MAPINFO:EXPORTS",
    "-Wl,/EXPORT:game_sound_create_samples",
    "-Wl,/EXPORT:game_update_and_render",
    "-shared"
)

Write-Host "Flags: $($GameFlags -join ' ')"

clang @GameFlags $GameFile -o $OutGame

if ($LASTEXITCODE -eq 0) {
    Write-Host "Game DLL build succeeded!" -ForegroundColor Green
} else {
    Write-Host "Game DLL build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Compiling $PlatformFile -> $OutPlatform"

$PlatformFlags = $Flags + @(
    "-luser32",
    "-lgdi32",
    "-lwinmm",
    "-Wl,/subsystem:windows",
    "-Wl,/MAP:bin/$PlatformFileName.map,/MAPINFO:EXPORTS"
)

Write-Host "Flags: $($PlatformFlags -join ' ')"

clang @PlatformFlags $PlatformFile -o $OutPlatform

if ($LASTEXITCODE -eq 0) {
    Write-Host "Platform EXE build succeeded!" -ForegroundColor Green
} else {
    Write-Host "Platform EXE build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
