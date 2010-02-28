#/bin/sh

echo "Setting up WinZsh MinGW compiling environment..."

echo "Copying winnt/config.h.winnt to ../config.h"
cp winnt/config.h.winnt ../config.h

echo "Copying winnt/Makefile.mingw to ./Makefile"
cp winnt/Makefile.mingw Makefile



