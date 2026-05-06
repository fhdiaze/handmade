$IncludedFileExts = "*.c", "*.h"

Write-Host "STATICS FOUND" -ForegroundColor Green
Get-ChildItem -Path . -Recurse -Include $IncludedFileExts |
Select-String -Pattern "static" | Select-Object FileName, LineNumber, Line | Out-Host

Write-Host ""
Write-Host "-------" -ForegroundColor Green
Write-Host ""
Write-Host "GLOBALS FOUND" -ForegroundColor Green

Get-ChildItem -Path . -Recurse -Include $IncludedFileExts |
Select-String -Pattern "GLOBAL_VARIABLE" | Select-Object FileName, LineNumber, Line | Out-Host

Get-ChildItem -Path . -Recurse -Include $IncludedFileExts |
Select-String -Pattern "INTERNAL" | Select-Object FileName, LineNumber, Line | Out-Host

Get-ChildItem -Path . -Recurse -Include $IncludedFileExts |
Select-String -Pattern "LOCAL_PERSIST" | Select-Object FileName, LineNumber, Line | Out-Host