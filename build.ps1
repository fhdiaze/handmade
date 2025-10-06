param(
    [Parameter(Mandatory=$true)]
    [string]$SourceFile,

    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$BuildMode = "Debug"
)

if (!(Test-Path $SourceFile)) {
    Write-Host "Source file not found: $SourceFile"
    exit 1
}

# Get the base filename without extension
$base = [System.IO.Path]::GetFileNameWithoutExtension($SourceFile)
$outdir = ".\bin"
if (!(Test-Path $outdir)) {
    New-Item -ItemType Directory -Path $outdir | Out-Null
}
$output = Join-Path $outdir "$base.exe"

# Read flags from file
$flags = Get-Content "compile_flags.txt" |
    Where-Object { $_.Trim() -ne "" -and -not $_.StartsWith("//") } |
    ForEach-Object { $_.Trim().TrimEnd(',') } |
    Where-Object { $_ -ne "" }

# Add build mode specific flags
if ($BuildMode -eq "Debug") {
    $flags += "-g"              # Debug symbols
    $flags += "-O0"             # No optimization
    $flags += "-DDEBUG"         # Define DEBUG macro
    #$flags += "-fsanitize=address"
    #$flags += "-fno-omit-frame-pointer"
    Write-Host "Building in DEBUG mode..."
} else {
    $flags += "-O3"             # Maximum optimization
    $flags += "-DNDEBUG"        # Define NDEBUG macro
    $flags += "-flto"           # Link-time optimization
    Write-Host "Building in RELEASE mode..."
}

Write-Host "Compiling $SourceFile -> $output"
Write-Host "Flags: $($flags -join ' ')"

clang @flags $SourceFile -o $output

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build succeeded!" -ForegroundColor Green
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}