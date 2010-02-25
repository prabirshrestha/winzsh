echo off

echo Setting up WinZsh compiling environment...

echo Copying winnt\config.h.winnt to ..\config.h
copy winnt\config.h.winnt ..\config.h

echo Copying winnt\Makefile.mingw to .\Makefile
copy winnt\Makefile.mingw Makefile



