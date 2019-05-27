@echo off
cls
set PS3SDK=/c/PSDK3v2
set WIN_PS3SDK=C:/PSDK3v2
set PS3DEV=%PS3SDK%/ps3dev2
set PATH=%WIN_PS3SDK%/mingw/msys/1.0/bin;%WIN_PS3SDK%/mingw/bin;%PS3DEV%/ppu/bin;
set CYGWIN=nodosfilewarning

mv  *482C *484C *484D PS3HEN_GEN
cd PS3HEN_GEN
gcc main.c  -o HEN_GEN
HEN_GEN PS3HEN.BIN_CEX_482 stage2.bin_482C stage0.bin_482C
HEN_GEN PS3HEN.BIN_CEX_484 stage2.bin_484C stage0.bin_484C
HEN_GEN PS3HEN.BIN_DEX_484 stage2.bin_484D stage0.bin_484D
rm stage*
cp PS3HEN* ../
pause