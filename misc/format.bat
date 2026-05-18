@echo off

echo -------
echo -------

set Wildcard=*.h *.c

for /r . %%f in (%Wildcard%) do (
    echo Formatting %%~nxf
    clang-format -i "%%f"
)
