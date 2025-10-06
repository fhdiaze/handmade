# format.ps1
Get-ChildItem -Path . -Recurse -Include "*.c", "*.h" |
ForEach-Object {
    Write-Host "Formatting $($_.Name)"
    clang-format -i $_.FullName
}