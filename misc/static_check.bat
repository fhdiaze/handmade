@echo off

echo -------
echo -------

set Wildcard=*.h *.c

setlocal enabledelayedexpansion

echo STATICS FOUND:

findstr -s -n -i -l "static" %Wildcard%
