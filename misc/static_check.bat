@echo off

echo -------
echo -------

set Wildcard=*.h *.c

echo STATICS FOUND:

findstr -s -n -i -l "static" %Wildcard%
