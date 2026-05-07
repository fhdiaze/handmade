$IncludedFileExts = "*.c", "*.h"

Write-Host "STATICS FOUND" -ForegroundColor Green
Get-ChildItem -Path . -Recurse -Include $IncludedFileExts |
Select-String -Pattern "static" | Select-Object FileName, LineNumber, Line | Out-Host
