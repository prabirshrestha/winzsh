echo off

echo Setting up WinZsh compiling environment...

echo Copying winnt\config.h.winnt to ..\config.h
copy winnt\config.h.winnt ..\config.h

echo Copying winnt\Makefile.msvc to .\Makefile
copy winnt\Makefile.msvc Makefile

rem Setup Microsoft make tools paths
rem "C:\Program Files\Microsoft Visual Studio\VC98\Bin\VCVARS32.BAT"

"C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\VCVARS32.BAT"